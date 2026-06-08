#include "ApiClient.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <Wt/WApplication.h>
#include <Wt/Http/Client.h>
#include <Wt/Http/Message.h>
#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Value.h>

namespace fd {

namespace {

// --- small, null-safe JSON helpers --------------------------------------
// NOTE (Wt version): the null check uses Wt::Json::Type::Null. Older Wt 4
// spells it Wt::Json::NullType — change here if your headers differ.
bool isNull(const Wt::Json::Value& v) {
    return v.type() == Wt::Json::Type::Null;
}

std::string jstr(const Wt::Json::Object& o, const char* key) {
    auto it = o.find(key);
    if (it == o.end() || isNull(it->second)) return {};
    return static_cast<std::string>(it->second);
}

double jnum(const Wt::Json::Object& o, const char* key) {
    auto it = o.find(key);
    if (it == o.end() || isNull(it->second)) return 0.0;
    return static_cast<double>(it->second);
}

int jint(const Wt::Json::Object& o, const char* key) {
    return static_cast<int>(jnum(o, key));
}

// Issue a GET and deliver the parsed "data" array (JSON:API collection) to
// `onArray`, or an error string to `onErr`. Updates are pushed to the client.
void getCollection(
    const std::string& url,
    std::function<void(const Wt::Json::Array&)> onArray,
    ApiClient::ErrorCallback onErr) {
    // The client must outlive the async request: keep it alive via the lambda.
    auto client = std::make_shared<Wt::Http::Client>();
    client->setTimeout(std::chrono::seconds(15));

    // NOTE (Wt version): the first slot arg is the asio error_code. Recent Wt
    // uses Wt::AsioWrapper::error_code; older builds use
    // boost::system::error_code. Adjust the type if compilation complains.
    client->done().connect(
        [client, onArray, onErr](Wt::AsioWrapper::error_code err,
                                 const Wt::Http::Message& response) {
            if (err) {
                onErr(err.message());
            } else if (response.status() != 200) {
                onErr("HTTP " + std::to_string(response.status()));
            } else {
                try {
                    Wt::Json::Object root;
                    Wt::Json::parse(response.body(), root);
                    const Wt::Json::Array& data = root.get("data");
                    onArray(data);
                } catch (const std::exception& e) {
                    onErr(std::string("parse error: ") + e.what());
                }
            }
            // We're inside the application's context here; push the UI update
            // performed by the callbacks above.
            if (auto* app = Wt::WApplication::instance()) app->triggerUpdate();
        });

    std::vector<Wt::Http::Message::Header> headers{
        {"Accept", "application/vnd.api+json"}};
    if (!client->get(url, headers)) {
        onErr("could not start request to " + url);
    }
}

fd::Driver parseDriver(const Wt::Json::Object& res) {
    const Wt::Json::Object& a = res.get("attributes");
    Driver d;
    d.id = jstr(res, "id");
    d.name = jstr(a, "name");
    d.driver_type_id = jint(a, "driver_type_id");
    auto it = a.find("active");
    d.active = (it == a.end()) ? true : static_cast<bool>(it->second);
    return d;
}

fd::Load parseLoad(const Wt::Json::Object& res) {
    const Wt::Json::Object& a = res.get("attributes");
    Load l;
    l.id = jstr(res, "id");
    l.driver_id = jstr(a, "driver_id");
    l.dispatch_week_id = jstr(a, "dispatch_week_id");
    l.pickup_date = jstr(a, "pickup_date");
    l.delivery_date = jstr(a, "delivery_date");
    l.currency = jstr(a, "currency");
    l.run_type_id = jint(a, "run_type_id");
    l.load_status_id = jint(a, "load_status_id");
    l.rate = jnum(a, "rate");
    l.deadhead_miles = jnum(a, "deadhead_miles");
    l.loaded_miles = jnum(a, "loaded_miles");
    return l;
}

fd::Position parsePosition(const Wt::Json::Object& res) {
    const Wt::Json::Object& a = res.get("attributes");
    Position p;
    p.id = jstr(res, "id");
    p.equipment_id = jstr(a, "equipment_id");
    p.driver_id = jstr(a, "driver_id");
    p.recorded_at = jstr(a, "recorded_at");
    p.location_source_id = jint(a, "location_source_id");
    p.lat = jnum(a, "lat");
    p.lng = jnum(a, "lng");
    p.speed_mph = jnum(a, "speed_mph");
    return p;
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

// POST a JSON:API body and deliver the parsed "data" object to `onObj`.
void postJson(const std::string& url, const std::string& body,
              std::function<void(const Wt::Json::Object&)> onObj,
              ApiClient::ErrorCallback onErr) {
    auto client = std::make_shared<Wt::Http::Client>();
    client->setTimeout(std::chrono::seconds(15));
    client->done().connect(
        [client, onObj, onErr](Wt::AsioWrapper::error_code err,
                               const Wt::Http::Message& response) {
            if (err) {
                onErr(err.message());
            } else if (response.status() != 200 && response.status() != 201) {
                onErr("HTTP " + std::to_string(response.status()) + ": " +
                      response.body());
            } else {
                try {
                    Wt::Json::Object root;
                    Wt::Json::parse(response.body(), root);
                    const Wt::Json::Object& data = root.get("data");
                    onObj(data);
                } catch (const std::exception& e) {
                    onErr(std::string("parse error: ") + e.what());
                }
            }
            if (auto* app = Wt::WApplication::instance()) app->triggerUpdate();
        });

    Wt::Http::Message msg;
    msg.addHeader("Content-Type", "application/vnd.api+json");
    msg.addHeader("Accept", "application/vnd.api+json");
    msg.addBodyText(body);
    if (!client->post(url, msg)) onErr("could not start POST to " + url);
}

}  // namespace

ApiClient::ApiClient(std::string baseUrl) : baseUrl_(std::move(baseUrl)) {}

std::string ApiClient::baseUrlFromEnv() {
    if (const char* env = std::getenv("FLEET_API_BASE_URL")) return env;
    return "http://localhost:5659/api";
}

void ApiClient::fetchDrivers(DriversCallback onOk, ErrorCallback onErr) {
    getCollection(
        baseUrl_ + "/Driver",
        [onOk](const Wt::Json::Array& data) {
            std::vector<Driver> out;
            for (const Wt::Json::Value& v : data) out.push_back(parseDriver(v));
            onOk(std::move(out));
        },
        std::move(onErr));
}

void ApiClient::fetchLoads(LoadsCallback onOk, ErrorCallback onErr) {
    getCollection(
        baseUrl_ + "/Load",
        [onOk](const Wt::Json::Array& data) {
            std::vector<Load> out;
            for (const Wt::Json::Value& v : data) out.push_back(parseLoad(v));
            onOk(std::move(out));
        },
        std::move(onErr));
}

void ApiClient::fetchPositions(PositionsCallback onOk, ErrorCallback onErr) {
    getCollection(
        baseUrl_ + "/PositionReport",
        [onOk](const Wt::Json::Array& data) {
            std::vector<Position> out;
            for (const Wt::Json::Value& v : data) out.push_back(parsePosition(v));
            onOk(std::move(out));
        },
        std::move(onErr));
}

void ApiClient::fetchOptions(const std::string& resource,
                             const std::string& labelAttr, OptionsCallback onOk,
                             ErrorCallback onErr) {
    getCollection(
        baseUrl_ + "/" + resource,
        [onOk, labelAttr](const Wt::Json::Array& data) {
            std::vector<Option> out;
            for (const Wt::Json::Value& v : data) {
                const Wt::Json::Object& res = v;
                const Wt::Json::Object& a = res.get("attributes");
                out.push_back({jstr(res, "id"), jstr(a, labelAttr.c_str())});
            }
            onOk(std::move(out));
        },
        std::move(onErr));
}

void ApiClient::createLoad(const LoadDraft& d, LoadCallback onOk,
                           ErrorCallback onErr) {
    std::vector<std::string> f;
    auto addStr = [&](const char* k, const std::string& v) {
        if (!v.empty())
            f.push_back("\"" + std::string(k) + "\":\"" + jsonEscape(v) + "\"");
    };
    auto addInt = [&](const char* k, int v) {
        f.push_back("\"" + std::string(k) + "\":" + std::to_string(v));
    };
    auto addFixed = [&](const char* k, double v, const char* fmt) {
        char b[32];
        std::snprintf(b, sizeof(b), fmt, v);
        f.push_back("\"" + std::string(k) + "\":" + b);
    };

    addStr("dispatch_week_id", d.dispatch_week_id);
    addStr("shipper_id", d.shipper_id);
    addStr("receiver_id", d.receiver_id);
    addStr("commodity_id", d.commodity_id);
    addStr("pickup_id", d.pickup_id);
    addStr("dropoff_id", d.dropoff_id);
    addStr("driver_id", d.driver_id);
    addStr("equipment_id", d.equipment_id);
    addInt("run_type_id", d.run_type_id);
    addInt("load_status_id", d.load_status_id);
    addFixed("rate", d.rate, "%.2f");
    addFixed("deadhead_miles", d.deadhead_miles, "%.1f");
    addFixed("loaded_miles", d.loaded_miles, "%.1f");
    addStr("pickup_date", d.pickup_date);
    addStr("delivery_date", d.delivery_date);

    std::ostringstream attrs;
    for (size_t i = 0; i < f.size(); ++i) {
        if (i) attrs << ",";
        attrs << f[i];
    }
    const std::string body =
        "{\"data\":{\"type\":\"Load\",\"attributes\":{" + attrs.str() + "}}}";

    postJson(
        baseUrl_ + "/Load", body,
        [onOk](const Wt::Json::Object& obj) { onOk(parseLoad(obj)); },
        std::move(onErr));
}

void ApiClient::postHudCommand(const std::string& commandType,
                               const std::string& arg, ErrorCallback onErr) {
    const std::string body =
        "{\"data\":{\"type\":\"HudCommand\",\"attributes\":{\"command_type\":\"" +
        jsonEscape(commandType) + "\",\"arg\":\"" + jsonEscape(arg) + "\"}}}";
    postJson(
        baseUrl_ + "/HudCommand", body,
        [](const Wt::Json::Object&) { /* fire-and-forget */ }, std::move(onErr));
}

}  // namespace fd

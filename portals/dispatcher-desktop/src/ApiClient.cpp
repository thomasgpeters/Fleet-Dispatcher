#include "ApiClient.h"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>

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

}  // namespace

ApiClient::ApiClient(std::string baseUrl) : baseUrl_(std::move(baseUrl)) {}

std::string ApiClient::baseUrlFromEnv() {
    if (const char* env = std::getenv("FLEET_API_BASE_URL")) return env;
    return "http://localhost:5656/api";
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

}  // namespace fd

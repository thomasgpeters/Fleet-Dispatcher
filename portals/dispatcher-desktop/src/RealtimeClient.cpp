#include "RealtimeClient.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Value.h>

#include "CommBus.h"
#include "PositionBus.h"
#include "models.h"

// Boost.Beast WebSocket client (no TLS here — terminate wss at a proxy and point
// FLEET_REALTIME_URL at the internal ws://). Synchronous read loop on a worker
// thread with exponential-backoff reconnect; stop() closes the socket so the
// blocking read unwinds.
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace fd {

namespace {

struct Url {
    std::string host;
    std::string port = "80";
    std::string target = "/";
};

Url parse_url(const std::string& url) {
    Url u;
    std::string s = url;
    const auto scheme = s.find("://");
    if (scheme != std::string::npos) s = s.substr(scheme + 3);
    const auto slash = s.find('/');
    if (slash != std::string::npos) {
        u.target = s.substr(slash);
        s = s.substr(0, slash);
    }
    const auto colon = s.find(':');
    if (colon != std::string::npos) {
        u.host = s.substr(0, colon);
        u.port = s.substr(colon + 1);
    } else {
        u.host = s;
    }
    return u;
}

std::string jstr(const Wt::Json::Object& o, const char* key) {
    auto it = o.find(key);
    if (it == o.end() || (*it).second.type() == Wt::Json::Type::Null) return {};
    return static_cast<std::string>((*it).second);
}

double jnum(const Wt::Json::Object& o, const char* key) {
    auto it = o.find(key);
    if (it == o.end() || (*it).second.type() == Wt::Json::Type::Null) return 0.0;
    return static_cast<double>((*it).second);
}

using WsStream = websocket::stream<tcp::socket>;

}  // namespace

RealtimeClient::RealtimeClient(std::string url, std::string token)
    : url_(std::move(url)), token_(std::move(token)) {}

RealtimeClient::~RealtimeClient() { stop(); }

std::unique_ptr<RealtimeClient> RealtimeClient::fromEnv() {
    const char* url = std::getenv("FLEET_REALTIME_URL");
    const char* token = std::getenv("FLEET_REALTIME_TOKEN");
    if (!url || !*url || !token || !*token) return nullptr;
    return std::make_unique<RealtimeClient>(url, token);
}

void RealtimeClient::start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread([this] { run(); });
}

void RealtimeClient::stop() {
    if (!running_.exchange(false)) return;
    closeStream();  // unblock a pending read
    if (thread_.joinable()) thread_.join();
}

void RealtimeClient::closeStream() {
    std::lock_guard<std::mutex> lock(stream_mutex_);
    if (stream_) {
        auto ws = std::static_pointer_cast<WsStream>(stream_);
        beast::error_code ec;
        ws->next_layer().close(ec);  // cancels the blocking read
    }
}

void RealtimeClient::run() {
    const Url u = parse_url(url_);
    double backoff = 1.0;

    while (running_.load()) {
        try {
            net::io_context ioc;
            tcp::resolver resolver(ioc);
            auto results = resolver.resolve(u.host, u.port);

            auto ws = std::make_shared<WsStream>(ioc);
            net::connect(ws->next_layer(), results.begin(), results.end());

            std::string target = u.target;
            target += (target.find('?') == std::string::npos) ? "?" : "&";
            target += "token=" + token_;
            ws->handshake(u.host, target);

            ws->write(net::buffer(std::string(
                "{\"action\":\"subscribe\",\"topics\":[\"messages\",\"fleet\"]}")));

            {
                std::lock_guard<std::mutex> lock(stream_mutex_);
                stream_ = ws;  // publish for stop()/closeStream()
            }
            backoff = 1.0;  // healthy connection

            beast::flat_buffer buffer;
            while (running_.load()) {
                buffer.clear();
                ws->read(buffer);  // blocks; closeStream() unwinds it on stop()
                handleFrame(beast::buffers_to_string(buffer.data()));
            }

            beast::error_code ec;
            ws->close(websocket::close_code::normal, ec);
        } catch (const std::exception&) {
            // connect failed / connection dropped / closed by stop()
        }

        {
            std::lock_guard<std::mutex> lock(stream_mutex_);
            stream_.reset();
        }
        if (running_.load()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(backoff * 1000)));
            backoff = std::min(backoff * 2.0, 30.0);  // capped backoff
        }
    }
}

void RealtimeClient::handleFrame(const std::string& text) {
    try {
        Wt::Json::Object root;
        Wt::Json::parse(text, root);
        if (jstr(root, "type") != "event") return;
        const Wt::Json::Object& ev = root.get("event");
        const std::string etype = jstr(ev, "type");

        // Both buses publish via WServer::post per session (thread-safe).
        if (etype == "message") {
            Message m;
            m.id = jstr(ev, "id");
            m.channel_id = jstr(ev, "channel_id");
            m.author_id = jstr(ev, "author_id");
            m.body = jstr(ev, "body");
            m.posted_at = jstr(ev, "posted_at");
            if (!m.id.empty()) CommBus::instance().publish(m);
        } else if (etype == "position") {
            Position p;
            p.id = jstr(ev, "id");
            p.equipment_id = jstr(ev, "equipment_id");
            p.driver_id = jstr(ev, "driver_id");
            p.recorded_at = jstr(ev, "recorded_at");
            p.lat = jnum(ev, "lat");
            p.lng = jnum(ev, "lng");
            p.speed_mph = jnum(ev, "speed_mph");
            if (!p.equipment_id.empty()) PositionBus::instance().publish(p);
        }
    } catch (const std::exception&) {
        // ignore malformed frames
    }
}

}  // namespace fd

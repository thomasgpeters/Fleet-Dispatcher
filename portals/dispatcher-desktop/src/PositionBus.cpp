#include "PositionBus.h"

#include <Wt/WServer.h>

namespace fd {

PositionBus& PositionBus::instance() {
    static PositionBus bus;
    return bus;
}

std::string PositionBus::subscribe(const std::string& sessionId, Handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string token = "p" + std::to_string(++counter_);
    subs_[token] = Sub{sessionId, std::move(handler)};
    return token;
}

void PositionBus::unsubscribe(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    subs_.erase(token);
}

void PositionBus::publish(const Position& position) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* server = Wt::WServer::instance();
    if (!server) return;
    for (const auto& entry : subs_) {
        const std::string sessionId = entry.second.sessionId;
        Handler handler = entry.second.handler;
        server->post(sessionId, [handler, position] { handler(position); });
    }
}

}  // namespace fd

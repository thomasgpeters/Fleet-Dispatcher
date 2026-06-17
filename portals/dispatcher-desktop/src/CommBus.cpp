#include "CommBus.h"

#include <Wt/WServer.h>

namespace fd {

CommBus& CommBus::instance() {
    static CommBus bus;
    return bus;
}

std::string CommBus::subscribe(const std::string& sessionId, Handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string token = "c" + std::to_string(++counter_);
    subs_[token] = Sub{sessionId, std::move(handler)};
    return token;
}

void CommBus::unsubscribe(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    subs_.erase(token);
}

void CommBus::publish(const Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* server = Wt::WServer::instance();
    if (!server) return;
    for (const auto& entry : subs_) {
        const std::string sessionId = entry.second.sessionId;
        Handler handler = entry.second.handler;
        // Run in the target session's context (holds its update lock); the
        // handler updates widgets and calls triggerUpdate().
        server->post(sessionId, [handler, message] { handler(message); });
    }
}

}  // namespace fd

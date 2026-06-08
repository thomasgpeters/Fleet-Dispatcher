#include "HudControlBus.h"

#include <Wt/WServer.h>

namespace fd {

HudControlBus& HudControlBus::instance() {
    static HudControlBus bus;
    return bus;
}

std::string HudControlBus::subscribe(const std::string& sessionId,
                                     Handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string token = "h" + std::to_string(++counter_);
    subs_[token] = Sub{sessionId, std::move(handler)};
    return token;
}

void HudControlBus::unsubscribe(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    subs_.erase(token);
}

void HudControlBus::publish(const HudCommand& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* server = Wt::WServer::instance();
    if (!server) return;
    for (const auto& entry : subs_) {
        const std::string sessionId = entry.second.sessionId;
        Handler handler = entry.second.handler;
        // Run in the target session's context (holds its update lock); the
        // handler updates widgets and calls triggerUpdate().
        server->post(sessionId, [handler, command] { handler(command); });
    }
}

}  // namespace fd

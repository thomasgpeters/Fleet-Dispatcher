// In-process communications bus: real-time push of new messages between console
// sessions on the same server (no polling). A CommPanel publishes when it sends;
// every subscribed CommPanel receives it via Wt server push (WServer::post →
// handler + triggerUpdate), delivered over WebSockets when enabled (wt_config).
//
// NOTE: this delivers messages that originate on THIS server. Messages posted by
// other clients (e.g. mobile) still need a source of truth event — until the
// middleware emits change events, CommPanel keeps a slow reconcile poll as a
// bridge. See docs/DISPATCHER_DESKTOP.md.
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>

#include "models.h"

namespace fd {

class CommBus {
public:
    using Handler = std::function<void(const Message&)>;

    static CommBus& instance();

    // Subscribe on panel open; unsubscribe (with the returned token) on close.
    std::string subscribe(const std::string& sessionId, Handler handler);
    void unsubscribe(const std::string& token);

    // Publish a newly-created message to every subscribed session.
    void publish(const Message& message);

private:
    CommBus() = default;

    struct Sub {
        std::string sessionId;
        Handler handler;
    };

    std::mutex mutex_;
    std::map<std::string, Sub> subs_;
    long counter_ = 0;
};

}  // namespace fd

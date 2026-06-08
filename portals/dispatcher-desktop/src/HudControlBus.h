// In-process command bus for the dispatcher HUD (same-server case).
//
// The control console publishes HudCommands; HUD sessions subscribe and react
// via Wt server push (WServer::post → handler + triggerUpdate). For a remote
// HUD, commands are additionally persisted as a hud_command JSON:API resource
// (see ApiClient::postHudCommand) which the remote can subscribe to.
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace fd {

struct HudCommand {
    enum Type {
        SetMode,        // arg: "today" | "week"
        FocusDriver,    // arg: driver id
        HighlightLoad   // arg: load id
    } type;
    std::string arg;

    // Wire form used for the persisted hud_command.command_type.
    const char* typeCode() const {
        switch (type) {
            case SetMode: return "set_mode";
            case FocusDriver: return "focus_driver";
            case HighlightLoad: return "highlight_load";
        }
        return "unknown";
    }
};

class HudControlBus {
public:
    using Handler = std::function<void(const HudCommand&)>;

    static HudControlBus& instance();

    // HUD sessions subscribe on open and unsubscribe (with the returned token)
    // on close. sessionId = Wt::WApplication::instance()->sessionId().
    std::string subscribe(const std::string& sessionId, Handler handler);
    void unsubscribe(const std::string& token);

    // Control surfaces publish; delivered to every HUD session via server push.
    void publish(const HudCommand& command);

private:
    HudControlBus() = default;

    struct Sub {
        std::string sessionId;
        Handler handler;
    };

    std::mutex mutex_;
    std::map<std::string, Sub> subs_;
    long counter_ = 0;
};

}  // namespace fd

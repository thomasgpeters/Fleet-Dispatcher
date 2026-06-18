// In-process position bus: real-time fleet-location push to console/HUD sessions.
// The RealtimeClient publishes position events (from the Kafka stream via the
// bridge) here; MapView and HudView subscribe and update their maps live —
// fleet locations are realtime data, not an ALS poll. Mirrors CommBus.
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>

#include "models.h"

namespace fd {

class PositionBus {
public:
    using Handler = std::function<void(const Position&)>;

    static PositionBus& instance();

    std::string subscribe(const std::string& sessionId, Handler handler);
    void unsubscribe(const std::string& token);
    void publish(const Position& position);

private:
    PositionBus() = default;

    struct Sub {
        std::string sessionId;
        Handler handler;
    };

    std::mutex mutex_;
    std::map<std::string, Sub> subs_;
    long counter_ = 0;
};

}  // namespace fd

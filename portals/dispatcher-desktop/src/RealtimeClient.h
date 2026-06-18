// RealtimeClient — server-wide outbound WebSocket client to the realtime bridge
// (realtime/). Consumes ALS->Kafka events and publishes them into CommBus, which
// fans out to every console session via Wt server push. One connection per
// server process (not per session).
//
// Built only when CMake option FD_REALTIME_CLIENT=ON (needs Boost.Beast). When
// off, the desktop keeps intra-server CommBus + the reconcile poll — so this is
// a pure accelerator, never a hard dependency.
//
// NOTE: not compiled in the dev sandbox; the .cpp uses Boost.Beast (sync client
// on a background thread with reconnect). Build + verify on a Linux box.
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace fd {

class RealtimeClient {
public:
    // url e.g. "ws://localhost:8765"; token = a bridge JWT (service principal).
    RealtimeClient(std::string url, std::string token);
    ~RealtimeClient();

    void start();  // spawns the background connect/read loop
    void stop();   // signals + closes + joins

    // Convenience: build from env (FLEET_REALTIME_URL, FLEET_REALTIME_TOKEN).
    // Returns nullptr if no URL/token configured.
    static std::unique_ptr<RealtimeClient> fromEnv();

private:
    std::string url_;
    std::string token_;
    std::atomic<bool> running_{false};
    std::thread thread_;

    // Active stream, so stop() can close it cross-thread to interrupt a blocking
    // read. Opaque here (Boost types live in the .cpp).
    std::mutex stream_mutex_;
    std::shared_ptr<void> stream_;  // shared_ptr<websocket::stream<tcp::socket>>

    void run();
    void handleFrame(const std::string& text);
    void closeStream();
};

}  // namespace fd

// Fleet Dispatcher — desktop dispatcher portal (C++ / Wt).
//
// Two entry points on one Wt server:
//   "/"     control console  (DispatcherApp → Shell)
//   "/hud"  HUD display       (HudApp → HudView), driven by the console via the
//                             in-process HudControlBus (server push).
//
// Talks to the shared ApiLogicServer JSON:API (FLEET_API_BASE_URL, default
// http://localhost:5659/api); never touches PostgreSQL directly.
// See docs/DISPATCHER_DESKTOP.md.

#include <iostream>
#include <memory>

#include <Wt/WApplication.h>
#include <Wt/WBootstrap5Theme.h>
#include <Wt/WEnvironment.h>
#include <Wt/WLink.h>
#include <Wt/WServer.h>

#include "ApiClient.h"
#include "HudView.h"
#include "LoginView.h"
#include "Shell.h"
#include "models.h"
#ifdef FD_REALTIME_CLIENT
#include "RealtimeClient.h"
#endif

namespace {

void applyChrome(Wt::WApplication* app) {
    app->setTheme(std::make_shared<Wt::WBootstrap5Theme>());
    // Responsive on tablets/phones: without a viewport meta, Safari lays the page
    // out at a default ~980px width in EVERY orientation, so the CSS media queries
    // never see the real width (iPad landscape stayed "stacked"). Must be set
    // before first render — applyChrome runs from the app constructor.
    // VERSION-SENSITIVE: Wt name-based addMetaHeader signature.
    app->addMetaHeader("viewport", "width=device-width, initial-scale=1");
    app->useStyleSheet(Wt::WLink("css/fleet-dispatcher.css"));
    app->enableUpdates(true);  // server push (async API + HUD command bus)
}

}  // namespace

// The dispatcher console. The session owns one ApiClient; we gate on auth —
// show the login screen first, then the shell once a token + user are in hand.
class DispatcherApp : public Wt::WApplication {
public:
    explicit DispatcherApp(const Wt::WEnvironment& env) : Wt::WApplication(env) {
        setTitle("Fleet Dispatcher — Dispatcher Console");
        applyChrome(this);
        api_ = std::make_unique<fd::ApiClient>(fd::ApiClient::baseUrlFromEnv());
        showLogin();
    }

private:
    std::unique_ptr<fd::ApiClient> api_;

    void showLogin() {
        root()->clear();
        root()->addNew<fd::LoginView>(
            api_.get(), [this](fd::AppUser user) { showShell(std::move(user)); });
    }

    void showShell(fd::AppUser user) {
        root()->clear();
        root()->addNew<fd::Shell>(api_.get(), std::move(user),
                                  [this] { showLogin(); });
    }
};

// The HUD display (often a wall screen with no operator to log in). It stays
// unauthenticated for now. When ALS auth is enforced on reads, give this a
// service token (e.g. ASSISTANT-style API key / a dedicated HUD login) via
// api_->setAuthToken(...) — tracked in docs/AUTHENTICATION.md / TODO.
class HudApp : public Wt::WApplication {
public:
    explicit HudApp(const Wt::WEnvironment& env) : Wt::WApplication(env) {
        setTitle("Fleet Dispatcher — HUD");
        applyChrome(this);
        // Each session owns its ApiClient (read-only board data).
        api_ = std::make_unique<fd::ApiClient>(fd::ApiClient::baseUrlFromEnv());
        root()->addNew<fd::HudView>(api_.get());
    }

private:
    std::unique_ptr<fd::ApiClient> api_;
};

int main(int argc, char** argv) {
    try {
        Wt::WServer server(argc, argv, WTHTTP_CONFIGURATION);
        server.addEntryPoint(
            Wt::EntryPointType::Application,
            [](const Wt::WEnvironment& env) {
                return std::make_unique<DispatcherApp>(env);
            },
            "");
        server.addEntryPoint(
            Wt::EntryPointType::Application,
            [](const Wt::WEnvironment& env) {
                return std::make_unique<HudApp>(env);
            },
            "/hud");

        // Start the server, then (optionally) the realtime bridge client, which
        // feeds CommBus from ALS->Kafka events. Use start/wait/stop (instead of
        // run) so the client gets a clean lifecycle.
        if (server.start()) {
#ifdef FD_REALTIME_CLIENT
            auto realtime = fd::RealtimeClient::fromEnv();  // null if unconfigured
            if (realtime) {
                realtime->start();
                std::cerr << "realtime: bridge client started\n";
            } else {
                std::cerr << "realtime: FLEET_REALTIME_URL/TOKEN unset — client off\n";
            }
#endif
            Wt::WServer::waitForShutdown();
#ifdef FD_REALTIME_CLIENT
            if (realtime) realtime->stop();
#endif
            server.stop();
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << std::endl;
        return 1;
    }
}

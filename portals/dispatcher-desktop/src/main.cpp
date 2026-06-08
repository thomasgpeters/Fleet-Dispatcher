// Fleet Dispatcher — desktop dispatcher portal (C++ / Wt).
//
// Two entry points on one Wt server:
//   "/"     control console  (DispatcherApp → Shell)
//   "/hud"  HUD display       (HudApp → HudView), driven by the console via the
//                             in-process HudControlBus (server push).
//
// Talks to the shared ApiLogicServer JSON:API (FLEET_API_BASE_URL, default
// http://localhost:5656/api); never touches PostgreSQL directly.
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
#include "Shell.h"

namespace {

void applyChrome(Wt::WApplication* app) {
    app->setTheme(std::make_shared<Wt::WBootstrap5Theme>());
    app->useStyleSheet(Wt::WLink("css/fleet-dispatcher.css"));
    app->enableUpdates(true);  // server push (async API + HUD command bus)
}

}  // namespace

class DispatcherApp : public Wt::WApplication {
public:
    explicit DispatcherApp(const Wt::WEnvironment& env) : Wt::WApplication(env) {
        setTitle("Fleet Dispatcher — Dispatcher Console");
        applyChrome(this);
        root()->addNew<fd::Shell>();
    }
};

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
        server.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << std::endl;
        return 1;
    }
}

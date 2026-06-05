// Fleet Dispatcher — desktop dispatcher portal (C++ / Wt).
//
// Entry point: builds the dispatcher shell (app bar, nav, Today|Week command
// bar, content outlet). Talks to the shared ApiLogicServer JSON:API (default
// http://localhost:5656/api, override with FLEET_API_BASE_URL) — it does not
// connect to PostgreSQL directly. See docs/DISPATCHER_DESKTOP.md.

#include <memory>

#include <Wt/WApplication.h>
#include <Wt/WBootstrap5Theme.h>
#include <Wt/WEnvironment.h>
#include <Wt/WLink.h>

#include "Shell.h"

class DispatcherApp : public Wt::WApplication {
public:
    explicit DispatcherApp(const Wt::WEnvironment& env) : Wt::WApplication(env) {
        setTitle("Fleet Dispatcher — Dispatcher Portal");

        // Bootstrap theme — CSS/JS served from the deployed Wt resources/ folder.
        setTheme(std::make_shared<Wt::WBootstrap5Theme>());
        useStyleSheet(Wt::WLink("css/fleet-dispatcher.css"));

        // Server push: required for the async JSON:API client to update the UI
        // (and, later, the HUD command bus).
        enableUpdates(true);

        root()->addNew<fd::Shell>();
    }
};

int main(int argc, char** argv) {
    return Wt::WRun(argc, argv, [](const Wt::WEnvironment& env) {
        return std::make_unique<DispatcherApp>(env);
    });
}

// Fleet Dispatcher — desktop dispatcher portal (C++ / Wt).
//
// Scaffold entry point: renders the dispatcher shell with a Bootstrap theme.
// The application talks to the shared ApiLogicServer JSON:API (default
// http://localhost:5656/api, override with FLEET_API_BASE_URL) — it does not
// connect to PostgreSQL directly. Build out the weekly board, load intake, and
// assignment views from here; see docs/domain-model.md for the resources.
//
// Styling: Wt's Bootstrap theme pulls its CSS/JS from the Wt `resources/`
// folder, which CMake deploys next to the executable (see CMakeLists.txt). The
// app also loads its own overrides from `css/fleet-dispatcher.css` in the
// docroot.

#include <Wt/WApplication.h>
#include <Wt/WBootstrap5Theme.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WEnvironment.h>
#include <Wt/WLink.h>
#include <Wt/WText.h>

#include <cstdlib>
#include <memory>
#include <string>

namespace {

std::string apiBaseUrl() {
    if (const char* env = std::getenv("FLEET_API_BASE_URL")) {
        return env;
    }
    return "http://localhost:5656/api";
}

} // namespace

class DispatcherApp : public Wt::WApplication {
public:
    explicit DispatcherApp(const Wt::WEnvironment& env) : Wt::WApplication(env) {
        setTitle("Fleet Dispatcher — Dispatcher Portal");

        // Bootstrap "flavor": Wt::WBootstrap5Theme serves Bootstrap 5 from the
        // deployed Wt resources/ folder. (For an older Wt, swap to
        // Wt::WBootstrapTheme with setVersion(Wt::BootstrapVersion::v3).)
        setTheme(std::make_shared<Wt::WBootstrap5Theme>());

        // App-specific overrides, served from the docroot (css/ is copied in
        // alongside the binary by CMake).
        useStyleSheet(Wt::WLink("css/fleet-dispatcher.css"));

        auto* page = root()->addNew<Wt::WContainerWidget>();
        page->setStyleClass("container py-4");

        auto* header = page->addNew<Wt::WContainerWidget>();
        header->setStyleClass("mb-4");
        header->addNew<Wt::WText>("<h1 class=\"display-6\">Fleet Dispatcher</h1>");
        header->addNew<Wt::WText>(
            "<p class=\"text-muted\">Dispatcher portal — desktop (C++ / Wt).</p>");

        auto* card = page->addNew<Wt::WContainerWidget>();
        card->setStyleClass("card");
        auto* body = card->addNew<Wt::WContainerWidget>();
        body->setStyleClass("card-body");
        body->addNew<Wt::WText>(
            "Connected to JSON:API at <code>" + apiBaseUrl() + "</code>");

        // TODO: weekly board (Mon->Mon), load intake form, driver/equipment
        //       assignment — all backed by the shared JSON:API.
    }
};

int main(int argc, char** argv) {
    return Wt::WRun(argc, argv, [](const Wt::WEnvironment& env) {
        return std::make_unique<DispatcherApp>(env);
    });
}

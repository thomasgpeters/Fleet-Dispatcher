// Fleet Dispatcher — desktop dispatcher portal (C++ / Wt).
//
// Scaffold entry point: renders the dispatcher shell. The application talks to
// the shared ApiLogicServer JSON:API (default http://localhost:5656/api,
// override with FLEET_API_BASE_URL) — it does not connect to PostgreSQL
// directly. Build out the weekly board, load intake, and assignment views from
// here; see docs/domain-model.md for the resources.

#include <Wt/WApplication.h>
#include <Wt/WBreak.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WEnvironment.h>
#include <Wt/WText.h>

#include <cstdlib>
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

        root()->addNew<Wt::WText>("<h1>Fleet Dispatcher</h1>");
        root()->addNew<Wt::WText>("<p>Dispatcher portal (desktop / Wt).</p>");
        root()->addNew<Wt::WBreak>();
        root()->addNew<Wt::WText>(
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

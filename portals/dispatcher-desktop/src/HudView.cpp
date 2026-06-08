#include "HudView.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

#include <Wt/WApplication.h>
#include <Wt/WString.h>
#include <Wt/WTable.h>
#include <Wt/WTableCell.h>
#include <Wt/WText.h>
#include <Wt/WTimer.h>

namespace fd {

namespace {
std::string fmt(double v, const char* f) {
    char b[32];
    std::snprintf(b, sizeof(b), f, v);
    return b;
}
}  // namespace

HudView::HudView(ApiClient* api) : api_(api) {
    addStyleClass("container py-3 fd-hud");

    auto* header = addNew<Wt::WContainerWidget>();
    header->addStyleClass("d-flex justify-content-between align-items-center mb-3");
    header->addNew<Wt::WText>("<h1 class=\"h3 mb-0\">Fleet HUD</h1>");
    modeLabel_ = header->addNew<Wt::WText>();
    modeLabel_->addStyleClass("badge bg-primary");
    modeLabel_->setText("WEEK");

    board_ = addNew<BoardView>(api_, mode_);

    addNew<Wt::WText>("<h2 class=\"h5 mt-4\">Truck locations</h2>");
    positionsBody_ = addNew<Wt::WContainerWidget>();

    // React to console commands via server push.
    auto* app = Wt::WApplication::instance();
    token_ = HudControlBus::instance().subscribe(
        app->sessionId(), [this](const HudCommand& cmd) {
            apply(cmd);
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
        });

    // Live-ish refresh of truck locations (until realtime push from telemetry).
    loadPositions();
    auto* timer = addChild(std::make_unique<Wt::WTimer>());
    timer->setInterval(std::chrono::seconds(15));
    timer->timeout().connect([this] { loadPositions(); });
    timer->start();
}

HudView::~HudView() {
    HudControlBus::instance().unsubscribe(token_);
}

void HudView::apply(const HudCommand& command) {
    switch (command.type) {
        case HudCommand::SetMode: {
            mode_ = (command.arg == "today") ? BoardMode::Today : BoardMode::Week;
            if (board_) board_->setMode(mode_);
            if (modeLabel_)
                modeLabel_->setText(mode_ == BoardMode::Today ? "TODAY" : "WEEK");
            break;
        }
        case HudCommand::FocusDriver:
        case HudCommand::HighlightLoad:
            // TODO: wire when the board/map gains driver focus & load highlight.
            break;
    }
}

void HudView::loadPositions() {
    // Names first (so the table can show unit numbers), then positions.
    api_->fetchOptions(
        "Equipment", "unit_number",
        [this](std::vector<Option> opts) {
            equipName_.clear();
            for (const auto& o : opts) equipName_[o.id] = o.label;
        },
        [](std::string) {});

    api_->fetchPositions(
        [this](std::vector<Position> ps) {
            positions_ = std::move(ps);
            renderPositions();
        },
        [](std::string) {});
}

void HudView::renderPositions() {
    if (!positionsBody_) return;
    positionsBody_->clear();

    // Latest position per equipment (positions arrive newest-last is not
    // guaranteed, so compare recorded_at strings — ISO-8601 sorts lexically).
    std::map<std::string, Position> latest;
    for (const auto& p : positions_) {
        if (p.equipment_id.empty()) continue;
        auto it = latest.find(p.equipment_id);
        if (it == latest.end() || p.recorded_at > it->second.recorded_at)
            latest[p.equipment_id] = p;
    }

    auto* table = positionsBody_->addNew<Wt::WTable>();
    table->addStyleClass("table table-sm table-striped");
    table->setHeaderCount(1);
    table->elementAt(0, 0)->addNew<Wt::WText>("Rig");
    table->elementAt(0, 1)->addNew<Wt::WText>("Lat");
    table->elementAt(0, 2)->addNew<Wt::WText>("Lng");
    table->elementAt(0, 3)->addNew<Wt::WText>("Speed (mph)");
    table->elementAt(0, 4)->addNew<Wt::WText>("Reported");

    int row = 0;
    for (const auto& kv : latest) {
        const Position& p = kv.second;
        ++row;
        const std::string rig = equipName_.count(p.equipment_id)
            ? equipName_[p.equipment_id] : p.equipment_id;
        table->elementAt(row, 0)->addNew<Wt::WText>(Wt::WString::fromUTF8(rig));
        table->elementAt(row, 1)->addNew<Wt::WText>(fmt(p.lat, "%.5f"));
        table->elementAt(row, 2)->addNew<Wt::WText>(fmt(p.lng, "%.5f"));
        table->elementAt(row, 3)->addNew<Wt::WText>(fmt(p.speed_mph, "%.0f"));
        table->elementAt(row, 4)->addNew<Wt::WText>(Wt::WString::fromUTF8(p.recorded_at));
    }
    if (row == 0) {
        positionsBody_->addNew<Wt::WText>(
            "<p class=\"text-muted\">No truck locations yet.</p>");
    }
}

}  // namespace fd

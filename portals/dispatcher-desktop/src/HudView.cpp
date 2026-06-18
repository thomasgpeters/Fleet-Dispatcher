#include "HudView.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>

#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WString.h>
#include <Wt/WTable.h>
#include <Wt/WTableCell.h>
#include <Wt/WText.h>
#include <Wt/WTimer.h>
#include <Wt/Json/Object.h>

#include "PositionBus.h"

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

    // The HUD is a wall display: force the dark theme regardless of OS setting
    // (shared design system — docs/DESIGN_SYSTEM.md).
    Wt::WApplication::instance()->doJavaScript(
        "document.documentElement.setAttribute('data-fd-theme','dark');");

    auto* header = addNew<Wt::WContainerWidget>();
    header->addStyleClass("d-flex justify-content-between align-items-center mb-3");
    header->addNew<Wt::WText>("<h1 class=\"h3 mb-0\">Fleet HUD</h1>");
    modeLabel_ = header->addNew<Wt::WText>();
    modeLabel_->addStyleClass("badge bg-primary");
    modeLabel_->setText("WEEK");

    // --- Truck-locations map -------------------------------------------------
    // WLeafletMap (Wt >= 4.5) renders Leaflet. Leaflet's JS/CSS are loaded from
    // a CDN here; for an offline/self-contained deploy, host them locally or set
    // the leaflet URLs in wt_config.xml and drop these two lines.
    auto* app = Wt::WApplication::instance();
    app->useStyleSheet(Wt::WLink("https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"));
    app->require("https://unpkg.com/leaflet@1.9.4/dist/leaflet.js");

    addNew<Wt::WText>("<h2 class=\"h5\">Truck locations</h2>");
    auto map = std::make_unique<Wt::WLeafletMap>();
    map->setHeight(420);
    Wt::Json::Object tileOptions;
    tileOptions["maxZoom"] = 19;
    tileOptions["attribution"] = "&copy; OpenStreetMap contributors";
    map->addTileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", tileOptions);
    map->panTo(Wt::WLeafletMap::Coordinate(39.5, -98.35));  // continental US
    map_ = addWidget(std::move(map));

    positionsBody_ = addNew<Wt::WContainerWidget>();

    // --- Board (read-only, mirrors console mode) -----------------------------
    addNew<Wt::WText>("<h2 class=\"h5 mt-4\">Board</h2>");
    board_ = addNew<BoardView>(api_, mode_);

    // React to console commands via server push.
    token_ = HudControlBus::instance().subscribe(
        app->sessionId(), [this](const HudCommand& cmd) {
            apply(cmd);
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
        });

    loadPositions();  // initial snapshot (ALS)

    // Realtime fleet locations from the Kafka stream (via the bridge →
    // PositionBus). Live map updates with no DB read-back per fix.
    posToken_ = PositionBus::instance().subscribe(
        Wt::WApplication::instance()->sessionId(),
        [this](const Position& p) {
            applyLive(p);
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
        });

    // Slow reconcile fallback (the stream is the primary live source).
    auto* timer = addChild(std::make_unique<Wt::WTimer>());
    timer->setInterval(std::chrono::seconds(60));
    timer->timeout().connect([this] { loadPositions(); });
    timer->start();
}

HudView::~HudView() {
    HudControlBus::instance().unsubscribe(token_);
    PositionBus::instance().unsubscribe(posToken_);
}

void HudView::applyLive(const Position& p) {
    positions_.erase(
        std::remove_if(positions_.begin(), positions_.end(),
                       [&](const Position& x) { return x.equipment_id == p.equipment_id; }),
        positions_.end());
    positions_.push_back(p);
    updateMap();
    renderPositions();
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
            // TODO: pan/zoom the map to a driver, or highlight a load.
            break;
    }
}

void HudView::loadPositions() {
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
            updateMap();
            renderPositions();
        },
        [](std::string) {});
}

void HudView::updateMap() {
    if (!map_) return;

    // Latest position per rig (ISO-8601 recorded_at sorts lexically).
    std::map<std::string, Position> latest;
    for (const auto& p : positions_) {
        if (p.equipment_id.empty()) continue;
        auto it = latest.find(p.equipment_id);
        if (it == latest.end() || p.recorded_at > it->second.recorded_at)
            latest[p.equipment_id] = p;
    }

    // Rebuild markers.
    for (auto* m : markers_) map_->removeMarker(m);
    markers_.clear();

    bool first = true;
    for (const auto& kv : latest) {
        const Position& p = kv.second;
        Wt::WLeafletMap::Coordinate pos(p.lat, p.lng);
        auto marker = std::make_unique<Wt::WLeafletMap::LeafletMarker>(pos);
        markers_.push_back(marker.get());
        map_->addMarker(std::move(marker));
        if (first) {
            map_->panTo(pos);  // center on the first rig we have
            first = false;
        }
    }
}

void HudView::renderPositions() {
    if (!positionsBody_) return;
    positionsBody_->clear();

    std::map<std::string, Position> latest;
    for (const auto& p : positions_) {
        if (p.equipment_id.empty()) continue;
        auto it = latest.find(p.equipment_id);
        if (it == latest.end() || p.recorded_at > it->second.recorded_at)
            latest[p.equipment_id] = p;
    }

    auto* table = positionsBody_->addNew<Wt::WTable>();
    table->addStyleClass("table table-sm table-striped mt-2");
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

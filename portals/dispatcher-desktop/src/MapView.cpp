#include "MapView.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

#include <Wt/WApplication.h>
#include <Wt/WLink.h>
#include <Wt/WString.h>
#include <Wt/WTable.h>
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

MapView::MapView(ApiClient* api) : api_(api) {
    addStyleClass("fd-map");
    addNew<Wt::WText>("<h2 class=\"h5\">Fleet locations</h2>");

    // Leaflet JS/CSS from CDN (host locally for an offline deploy; see HudView).
    auto* app = Wt::WApplication::instance();
    app->useStyleSheet(Wt::WLink("https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"));
    app->require("https://unpkg.com/leaflet@1.9.4/dist/leaflet.js");

    auto map = std::make_unique<Wt::WLeafletMap>();
    map->setHeight(460);
    Wt::Json::Object tileOptions;
    tileOptions["maxZoom"] = 19;
    tileOptions["attribution"] = "&copy; OpenStreetMap contributors";
    map->addTileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", tileOptions);
    map->panTo(Wt::WLeafletMap::Coordinate(39.5, -98.35));  // continental US
    map_ = addWidget(std::move(map));

    tableBody_ = addNew<Wt::WContainerWidget>();

    load();  // initial snapshot (ALS)

    // Realtime fleet locations: apply position events straight from the Kafka
    // stream (via the bridge → PositionBus). No DB read-back per fix.
    posToken_ = PositionBus::instance().subscribe(
        Wt::WApplication::instance()->sessionId(),
        [this](const Position& p) {
            applyLive(p);
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
        });

    // Slow reconcile fallback (the stream is the primary live source).
    auto* timer = addChild(std::make_unique<Wt::WTimer>());
    timer->setInterval(std::chrono::seconds(60));
    timer->timeout().connect([this] { load(); });
    timer->start();
}

MapView::~MapView() { PositionBus::instance().unsubscribe(posToken_); }

void MapView::applyLive(const Position& p) {
    // Upsert latest fix for this rig, then re-render from the bounded set.
    positions_.erase(
        std::remove_if(positions_.begin(), positions_.end(),
                       [&](const Position& x) { return x.equipment_id == p.equipment_id; }),
        positions_.end());
    positions_.push_back(p);
    updateMap();
    renderTable();
}

void MapView::load() {
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
            renderTable();
        },
        [](std::string) {});
}

void MapView::updateMap() {
    if (!map_) return;

    // Latest position per rig (ISO-8601 recorded_at sorts lexically).
    std::map<std::string, Position> latest;
    for (const auto& p : positions_) {
        if (p.equipment_id.empty()) continue;
        auto it = latest.find(p.equipment_id);
        if (it == latest.end() || p.recorded_at > it->second.recorded_at)
            latest[p.equipment_id] = p;
    }

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
            map_->panTo(pos);
            first = false;
        }
    }
}

void MapView::renderTable() {
    if (!tableBody_) return;
    tableBody_->clear();

    std::map<std::string, Position> latest;
    for (const auto& p : positions_) {
        if (p.equipment_id.empty()) continue;
        auto it = latest.find(p.equipment_id);
        if (it == latest.end() || p.recorded_at > it->second.recorded_at)
            latest[p.equipment_id] = p;
    }

    auto* table = tableBody_->addNew<Wt::WTable>();
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
                                    ? equipName_[p.equipment_id]
                                    : p.equipment_id;
        table->elementAt(row, 0)->addNew<Wt::WText>(Wt::WString::fromUTF8(rig));
        table->elementAt(row, 1)->addNew<Wt::WText>(fmt(p.lat, "%.5f"));
        table->elementAt(row, 2)->addNew<Wt::WText>(fmt(p.lng, "%.5f"));
        table->elementAt(row, 3)->addNew<Wt::WText>(fmt(p.speed_mph, "%.0f"));
        table->elementAt(row, 4)
            ->addNew<Wt::WText>(Wt::WString::fromUTF8(p.recorded_at));
    }
    if (row == 0) {
        tableBody_->addNew<Wt::WText>(
            "<p class=\"fd-muted\">No truck locations yet.</p>");
    }
}

}  // namespace fd

#include "FleetView.h"

#include <string>

#include <Wt/WString.h>
#include <Wt/WText.h>

#include "Toaster.h"
#include "icons.h"

namespace fd {

namespace {
const char* driverType(int id) {
    switch (id) {
        case 1: return "Company";
        case 2: return "Owner-Operator";
        default: return "—";
    }
}
// trailer_type name by id (matches seed) — for the colour legend.
const char* trailerName(int id) {
    switch (id) {
        case 1: return "Step-deck";
        case 2: return "RGN low-boy";
        case 3: return "Flatbed";
        case 4: return "Car carrier";
        case 5: return "Power only";
        default: return "Other";
    }
}
// power_unit name by id (matches seed).
const char* powerUnitName(int id) {
    switch (id) {
        case 1: return "Tractor";
        case 2: return "RAM 3500";
        case 3: return "RAM 4500";
        default: return "Power unit";
    }
}
// Thousands-grouped integer ("48000" → "48,000").
std::string commas(int v) {
    std::string d = std::to_string(v < 0 ? -v : v), out;
    int c = 0;
    for (int i = static_cast<int>(d.size()) - 1; i >= 0; --i) {
        out.push_back(d[i]);
        if (++c % 3 == 0 && i > 0) out.push_back(',');
    }
    std::string r(out.rbegin(), out.rend());
    return (v < 0 ? "-" : "") + r;
}
}  // namespace

FleetView::FleetView(ApiClient* api) : api_(api) {
    addStyleClass("fd-fleet");
    addNew<Wt::WText>("<h2 class=\"h5\">Fleet</h2>");

    addNew<Wt::WText>("<div class=\"fd-panel-title mt-2\">Drivers</div>");
    driversBody_ = addNew<Wt::WContainerWidget>();
    driversBody_->addNew<Wt::WText>("<p class=\"fd-muted\">Loading…</p>");

    addNew<Wt::WText>("<div class=\"fd-panel-title mt-3\">Equipment</div>");
    equipBody_ = addNew<Wt::WContainerWidget>();
    equipBody_->addNew<Wt::WText>("<p class=\"fd-muted\">Loading…</p>");

    // Bottom-right stack for vehicle-info toasts (same UX as the board loads).
    vehicleToasts_ = addNew<Toaster>(Toaster::Position::BottomRight);

    loadDrivers();
    loadEquipment();
}

void FleetView::showVehicleToast(const EquipmentInfo& e) {
    const std::string title =
        e.unit_number + " · " + trailerName(e.trailer_type_id);
    std::string body = "<div>" + std::string(powerUnitName(e.power_unit_id)) + "</div>";
    if (e.deck_length_ft > 0 || e.weight_capacity_lbs > 0) {
        body += "<div class=\"fd-toast-sub\">Deck " +
                std::to_string(e.deck_length_ft) + " ft · " +
                commas(e.weight_capacity_lbs) + " lb capacity</div>";
    }
    std::string feats;
    if (e.has_ramps) feats += "ramps";
    if (e.has_duals) feats += (feats.empty() ? "" : " · ") + std::string("duals");
    if (!feats.empty())
        body += "<div class=\"fd-toast-sub\">" + feats + "</div>";
    // Sticky so the dispatcher can line several rigs up side by side.
    vehicleToasts_->notify(title, body, Toaster::Level::Info, 0);
}

void FleetView::loadDrivers() {
    api_->fetchDrivers(
        [this](std::vector<Driver> drivers) {
            driversBody_->clear();
            if (drivers.empty()) {
                driversBody_->addNew<Wt::WText>("<p class=\"fd-muted\">No drivers.</p>");
                return;
            }
            // A clean custom roster (not a Bootstrap table): colour avatar +
            // name/home-base, a type badge, and a status dot. Reads professionally
            // on the dark panel and shows each driver's palette colour.
            auto* list = driversBody_->addNew<Wt::WContainerWidget>();
            list->addStyleClass("fd-roster");
            for (const Driver& d : drivers) {
                const std::string status =
                    d.active ? "<span class=\"fd-status is-active\">Active</span>"
                             : "<span class=\"fd-status\">Inactive</span>";
                const std::string sub =
                    d.home_base.empty() ? "" :
                    "<div class=\"fd-roster-sub\">" + d.home_base + "</div>";
                list->addNew<Wt::WText>(Wt::WString::fromUTF8(
                    "<div class=\"fd-roster-row\">" +
                    personAvatar(d.name, d.avatar_color_id) +
                    "<div class=\"fd-roster-main\">"
                    "<div class=\"fd-roster-name\">" + d.name + "</div>" + sub +
                    "</div>"
                    "<span class=\"fd-badge\">" + driverType(d.driver_type_id) + "</span>" +
                    status +
                    "</div>"));
            }
        },
        [this](std::string err) {
            driversBody_->clear();
            driversBody_->addNew<Wt::WText>(Wt::WString::fromUTF8(
                "<p class=\"text-danger\">Could not load drivers: " + err + "</p>"));
        });
}

void FleetView::loadEquipment() {
    api_->fetchEquipment(
        [this](std::vector<EquipmentInfo> equip) {
            equipBody_->clear();
            if (equip.empty()) {
                equipBody_->addNew<Wt::WText>(
                    "<p class=\"fd-muted\">No equipment.</p>");
                return;
            }
            // Legend: the trailer-type colour key (the types actually present).
            std::string legend = "<div class=\"fd-legend\">";
            bool seen[6] = {false, false, false, false, false, false};
            for (const EquipmentInfo& e : equip) {
                int t = e.trailer_type_id;
                if (t >= 1 && t <= 5 && !seen[t]) {
                    seen[t] = true;
                    legend += "<span class=\"fd-legend-item\">"
                              "<span class=\"fd-equip-swatch\" style=\"background:" +
                              trailerHex(t) + "\"></span>" + trailerName(t) + "</span>";
                }
            }
            legend += "</div>";
            equipBody_->addNew<Wt::WText>(Wt::WString::fromUTF8(legend));

            // Chips coloured by trailer type.
            auto* list = equipBody_->addNew<Wt::WContainerWidget>();
            list->addStyleClass("fd-fleet-equip");
            for (const EquipmentInfo& e : equip) {
                auto* chip = list->addNew<Wt::WText>(Wt::WString::fromUTF8(
                    "<span class=\"fd-equip-chip is-clickable\">"
                    "<span class=\"fd-equip-swatch\" style=\"background:" +
                    trailerHex(e.trailer_type_id) + "\"></span>" +
                    e.unit_number + "</span>"));
                chip->clicked().connect([this, e]() { showVehicleToast(e); });
            }
        },
        [this](std::string err) {
            equipBody_->clear();
            equipBody_->addNew<Wt::WText>(Wt::WString::fromUTF8(
                "<p class=\"text-danger\">Could not load equipment: " + err + "</p>"));
        });
}

}  // namespace fd

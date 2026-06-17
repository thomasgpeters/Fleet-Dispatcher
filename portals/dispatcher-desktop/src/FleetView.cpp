#include "FleetView.h"

#include <Wt/WString.h>
#include <Wt/WTable.h>
#include <Wt/WText.h>

namespace fd {

namespace {
const char* driverType(int id) {
    switch (id) {
        case 1: return "Company";
        case 2: return "Owner-Operator";
        default: return "—";
    }
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

    loadDrivers();
    loadEquipment();
}

void FleetView::loadDrivers() {
    api_->fetchDrivers(
        [this](std::vector<Driver> drivers) {
            driversBody_->clear();
            if (drivers.empty()) {
                driversBody_->addNew<Wt::WText>("<p class=\"fd-muted\">No drivers.</p>");
                return;
            }
            auto* table = driversBody_->addNew<Wt::WTable>();
            table->addStyleClass("table table-sm table-striped");
            table->setHeaderCount(1);
            table->elementAt(0, 0)->addNew<Wt::WText>("Driver");
            table->elementAt(0, 1)->addNew<Wt::WText>("Type");
            table->elementAt(0, 2)->addNew<Wt::WText>("Status");
            int row = 0;
            for (const Driver& d : drivers) {
                ++row;
                table->elementAt(row, 0)->addNew<Wt::WText>(
                    Wt::WString::fromUTF8(d.name));
                table->elementAt(row, 1)->addNew<Wt::WText>(driverType(d.driver_type_id));
                table->elementAt(row, 2)
                    ->addNew<Wt::WText>(d.active ? "Active" : "Inactive");
            }
        },
        [this](std::string err) {
            driversBody_->clear();
            driversBody_->addNew<Wt::WText>(Wt::WString::fromUTF8(
                "<p class=\"text-danger\">Could not load drivers: " + err + "</p>"));
        });
}

void FleetView::loadEquipment() {
    // Equipment shown by unit number (generic option fetch — no extra model).
    api_->fetchOptions(
        "Equipment", "unit_number",
        [this](std::vector<Option> equip) {
            equipBody_->clear();
            if (equip.empty()) {
                equipBody_->addNew<Wt::WText>(
                    "<p class=\"fd-muted\">No equipment.</p>");
                return;
            }
            auto* list = equipBody_->addNew<Wt::WContainerWidget>();
            list->addStyleClass("fd-fleet-equip");
            for (const Option& e : equip) {
                auto* chip = list->addNew<Wt::WText>(
                    Wt::WString::fromUTF8(e.label));
                chip->addStyleClass("badge bg-primary me-1 mb-1");
            }
        },
        [this](std::string err) {
            equipBody_->clear();
            equipBody_->addNew<Wt::WText>(Wt::WString::fromUTF8(
                "<p class=\"text-danger\">Could not load equipment: " + err + "</p>"));
        });
}

}  // namespace fd

// FleetView — a list view of the fleet (drivers + equipment), fetched from the
// JSON:API. Rendered in the center work panel when "Fleet" is chosen in the menu.
#pragma once

#include <Wt/WContainerWidget.h>

#include "ApiClient.h"
#include "models.h"

namespace Wt {
class WText;
}

namespace fd {

class Toaster;

class FleetView : public Wt::WContainerWidget {
public:
    explicit FleetView(ApiClient* api);

private:
    ApiClient* api_;
    Wt::WContainerWidget* driversBody_ = nullptr;
    Wt::WContainerWidget* equipBody_ = nullptr;
    Toaster* vehicleToasts_ = nullptr;   // bottom-right vehicle-info stack

    void loadDrivers();
    void loadEquipment();
    void showVehicleToast(const EquipmentInfo& e);  // click a rig → info toast
};

}  // namespace fd

// MapView — a Leaflet map of current truck positions for the center work panel,
// refreshed on a timer. Mirrors the HUD's map (same Wt::WLeafletMap usage).
#pragma once

#include <map>
#include <string>
#include <vector>

#include <Wt/WContainerWidget.h>
#include <Wt/WLeafletMap.h>

#include "ApiClient.h"
#include "models.h"

namespace Wt {
class WText;
}

namespace fd {

class MapView : public Wt::WContainerWidget {
public:
    explicit MapView(ApiClient* api);

private:
    ApiClient* api_;
    Wt::WLeafletMap* map_ = nullptr;
    std::vector<Wt::WLeafletMap::Marker*> markers_;
    Wt::WContainerWidget* tableBody_ = nullptr;
    std::vector<Position> positions_;
    std::map<std::string, std::string> equipName_;  // equipment_id -> unit_number

    void load();
    void updateMap();
    void renderTable();
};

}  // namespace fd

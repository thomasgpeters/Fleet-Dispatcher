// HUD display surface (served at /hud): a read-only board that auto-switches
// Today/Week (and reacts to other commands) when the dispatcher console
// publishes to the HudControlBus, plus a live Leaflet map + table of truck
// positions.
#pragma once

#include <map>
#include <string>
#include <vector>

#include <Wt/WContainerWidget.h>
#include <Wt/WLeafletMap.h>

#include "ApiClient.h"
#include "BoardView.h"
#include "HudControlBus.h"
#include "models.h"

namespace Wt {
class WText;
}

namespace fd {

class HudView : public Wt::WContainerWidget {
public:
    explicit HudView(ApiClient* api);
    ~HudView() override;

private:
    ApiClient* api_;
    BoardMode mode_ = BoardMode::Week;
    BoardView* board_ = nullptr;
    Wt::WText* modeLabel_ = nullptr;
    std::string token_;

    // Truck-locations panel: a Leaflet map + a detail table.
    Wt::WLeafletMap* map_ = nullptr;
    std::vector<Wt::WLeafletMap::Marker*> markers_;
    Wt::WContainerWidget* positionsBody_ = nullptr;
    std::vector<Position> positions_;
    std::map<std::string, std::string> equipName_;  // equipment_id -> unit_number

    void apply(const HudCommand& command);
    void loadPositions();
    void updateMap();
    void renderPositions();
};

}  // namespace fd

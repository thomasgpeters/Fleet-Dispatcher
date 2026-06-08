// HUD display surface (served at /hud): a read-only board that auto-switches
// Today/Week (and reacts to other commands) when the dispatcher console
// publishes to the HudControlBus.
#pragma once

#include <string>

#include <Wt/WContainerWidget.h>

#include "ApiClient.h"
#include "BoardView.h"
#include "HudControlBus.h"

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

    void apply(const HudCommand& command);
};

}  // namespace fd

// The dispatcher application shell: an app frame implementing the shared design
// system (docs/DESIGN_SYSTEM.md).
//
//   full-width header   logo (top-left) · profile + role (top-right) + tools
//   ┌───────┬──────────────────────────┬───────────────┐
//   │ LEFT  │        CENTER            │     RIGHT      │
//   │ menu  │   work panel (outlet)    │ communications │
//   │ +work │                          │ (channels)     │
//   │ toggle│                          │                │
//   └───────┴──────────────────────────┴───────────────┘
//   full-width footer
//
// Alerts surface as top-right toasts (Toaster). The JSON:API client is owned by
// the application.
#pragma once

#include <functional>
#include <vector>

#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "ApiClient.h"
#include "BoardView.h"
#include "LoadForm.h"
#include "models.h"

namespace fd {

class Toaster;

class Shell : public Wt::WContainerWidget {
public:
    Shell(ApiClient* api, AppUser user, std::function<void()> onLogout);

private:
    ApiClient* api_;  // owned by the application, not the shell
    AppUser user_;
    std::function<void()> onLogout_;
    BoardMode mode_ = BoardMode::Week;

    // Frame regions.
    Wt::WContainerWidget* leftPanel_ = nullptr;
    Wt::WContainerWidget* rightPanel_ = nullptr;
    Wt::WContainerWidget* content_ = nullptr;  // center work outlet
    Toaster* toaster_ = nullptr;
    Wt::WPushButton* todayBtn_ = nullptr;
    Wt::WPushButton* weekBtn_ = nullptr;
    Wt::WPushButton* leftToggleBtn_ = nullptr;   // arrow flips with open/closed
    Wt::WPushButton* rightToggleBtn_ = nullptr;
    BoardView* board_ = nullptr;

    std::vector<Wt::WPushButton*> menuButtons_;
    bool leftCollapsed_ = false;
    bool rightCollapsed_ = false;
    bool compact_ = false;
    bool commsMode_ = false;        // full comms view active (rail hidden)
    bool rightWasCollapsed_ = false;  // rail state to restore on leaving comms

    void buildHeader();
    void buildToggleBar();  // panel hide/show toggles, anchored above the body
    void buildBody();
    void buildFooter();
    void buildLeftMenu();

    void showBoard();
    void showLoadForm();
    void showFleet();     // fleet list (drivers + equipment)
    void showMap();       // geo-positioning of fleet locations
    void showComms();     // communications take over the full center work area
    void showSettings();  // appearance + app info
    void showProfile();
    void showPlaceholder(const std::string& title);
    void setMode(BoardMode mode);
    void refreshModeButtons();
    void setActiveMenu(Wt::WPushButton* active);
    void toggleLeft();
    void toggleRight();
    void enterCommsMode();  // hide the right rail (comms takes the full work area)
    void exitCommsMode();   // restore the rail when leaving comms
    void toggleTheme();
    void toggleCompact();
};

}  // namespace fd

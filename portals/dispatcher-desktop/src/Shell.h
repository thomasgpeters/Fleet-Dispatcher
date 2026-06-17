// The dispatcher application shell: an app frame implementing the shared design
// system (docs/DESIGN_SYSTEM.md) — a full-width header (navigation), a
// three-column body (collapsible left panel · center work panel · collapsible
// right panel), and a full-width footer. Owns app-wide state; the JSON:API
// client is owned by the application.
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

class Shell : public Wt::WContainerWidget {
public:
    // `api` is owned by the application (bearer token already set); `user` is the
    // signed-in account; `onLogout` returns to the login screen.
    Shell(ApiClient* api, AppUser user, std::function<void()> onLogout);

private:
    ApiClient* api_;  // owned by the application, not the shell
    AppUser user_;
    std::function<void()> onLogout_;
    BoardMode mode_ = BoardMode::Week;

    // Frame regions.
    Wt::WContainerWidget* leftPanel_ = nullptr;
    Wt::WContainerWidget* rightPanel_ = nullptr;
    Wt::WContainerWidget* content_ = nullptr;     // center work outlet
    Wt::WContainerWidget* commandBar_ = nullptr;  // Today | Week (above content)
    Wt::WText* userLabel_ = nullptr;
    Wt::WPushButton* todayBtn_ = nullptr;
    Wt::WPushButton* weekBtn_ = nullptr;
    BoardView* board_ = nullptr;

    std::vector<Wt::WPushButton*> navButtons_;
    bool leftCollapsed_ = false;
    bool rightCollapsed_ = false;

    void buildHeader();
    void buildBody();
    void buildFooter();

    void showBoard();
    void showLoadForm();
    void showProfile();
    void showPlaceholder(const std::string& title);
    void setMode(BoardMode mode);
    void refreshModeButtons();
    void updateUserLabel();
    void setActiveNav(Wt::WPushButton* active);
    void toggleLeft();
    void toggleRight();
    void toggleTheme();
};

}  // namespace fd

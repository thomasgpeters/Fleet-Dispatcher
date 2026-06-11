// The dispatcher application shell: app bar, navigation, command bar (Today |
// Week), and the content outlet. Owns the JSON:API client and app-wide state.
#pragma once

#include <functional>

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
    // `api` is owned by the application (with the bearer token already set);
    // `user` is the signed-in account; `onLogout` returns to the login screen.
    Shell(ApiClient* api, AppUser user, std::function<void()> onLogout);

private:
    ApiClient* api_;  // owned by the application, not the shell
    AppUser user_;
    std::function<void()> onLogout_;
    BoardMode mode_ = BoardMode::Week;

    Wt::WText* userLabel_ = nullptr;
    Wt::WContainerWidget* commandBar_ = nullptr;
    Wt::WContainerWidget* content_ = nullptr;
    Wt::WPushButton* todayBtn_ = nullptr;
    Wt::WPushButton* weekBtn_ = nullptr;
    BoardView* board_ = nullptr;

    void showBoard();
    void showLoadForm();
    void showProfile();
    void showPlaceholder(const std::string& title);
    void setMode(BoardMode mode);
    void refreshModeButtons();
    void updateUserLabel();
};

}  // namespace fd

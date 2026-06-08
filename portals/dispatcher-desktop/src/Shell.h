// The dispatcher application shell: app bar, navigation, command bar (Today |
// Week), and the content outlet. Owns the JSON:API client and app-wide state.
#pragma once

#include <memory>

#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>

#include "ApiClient.h"
#include "BoardView.h"
#include "LoadForm.h"

namespace fd {

class Shell : public Wt::WContainerWidget {
public:
    Shell();

private:
    std::unique_ptr<ApiClient> api_;
    BoardMode mode_ = BoardMode::Week;

    Wt::WContainerWidget* commandBar_ = nullptr;
    Wt::WContainerWidget* content_ = nullptr;
    Wt::WPushButton* todayBtn_ = nullptr;
    Wt::WPushButton* weekBtn_ = nullptr;
    BoardView* board_ = nullptr;

    void showBoard();
    void showLoadForm();
    void showPlaceholder(const std::string& title);
    void setMode(BoardMode mode);
    void refreshModeButtons();
};

}  // namespace fd

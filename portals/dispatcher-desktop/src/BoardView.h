// The dispatcher board: switches between Today's runs and the Mon→Mon week grid.
// Fetches Driver + Load from the JSON:API and renders client-side.
#pragma once

#include <string>
#include <vector>

#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>

#include "ApiClient.h"
#include "models.h"

namespace fd {

class Toaster;

enum class BoardMode { Today, Week };

class BoardView : public Wt::WContainerWidget {
public:
    BoardView(ApiClient* api, BoardMode mode);

    void setMode(BoardMode mode);
    void reload();

private:
    ApiClient* api_;
    BoardMode mode_;
    std::vector<Driver> drivers_;
    std::vector<Load> loads_;
    bool driversLoaded_ = false;
    bool loadsLoaded_ = false;

    Wt::WText* status_ = nullptr;
    Wt::WContainerWidget* body_ = nullptr;
    Toaster* loadToasts_ = nullptr;   // bottom-right load-selection stack

    void renderWhenReady();
    void render();
    void renderToday();
    void renderWeek();
    // Pop a bottom-right toast describing a selected load (stacks on repeat).
    void showLoadToast(const std::string& driverName, const Load& l);
};

}  // namespace fd

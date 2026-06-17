// SettingsView — console settings for the center work panel: appearance
// (theme) and app info. Theme choice is applied via data-fd-theme on <html>
// and persisted in localStorage (shared with the header toggle).
#pragma once

#include <string>

#include <Wt/WContainerWidget.h>

#include "ApiClient.h"
#include "models.h"

namespace fd {

class SettingsView : public Wt::WContainerWidget {
public:
    SettingsView(ApiClient* api, AppUser user);

private:
    ApiClient* api_;
    AppUser user_;

    void setTheme(const std::string& mode);  // "system" | "light" | "dark"
};

}  // namespace fd

#include "SettingsView.h"

#include <Wt/WApplication.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WText.h>

namespace fd {

namespace {
const char* roleLabel(int id) {
    switch (id) {
        case 1: return "Dispatcher";
        case 2: return "Driver";
        case 3: return "Updater";
        default: return "User";
    }
}
}  // namespace

SettingsView::SettingsView(ApiClient* api, AppUser user)
    : api_(api), user_(std::move(user)) {
    addStyleClass("fd-settings");
    addNew<Wt::WText>("<h2 class=\"h5\">Settings</h2>");

    // --- Appearance ---
    addNew<Wt::WText>("<div class=\"fd-panel-title mt-2\">Appearance</div>");
    auto* group = addNew<Wt::WContainerWidget>();
    group->addStyleClass("btn-group mb-2");
    auto addTheme = [&](const char* label, const std::string& mode) {
        auto* b = group->addNew<Wt::WPushButton>(label);
        b->addStyleClass("btn btn-sm btn-outline-primary");
        b->clicked().connect([this, mode] { setTheme(mode); });
    };
    addTheme("System", "system");
    addTheme("Light", "light");
    addTheme("Dark", "dark");

    // --- Account ---
    addNew<Wt::WText>("<div class=\"fd-panel-title mt-3\">Account</div>");
    addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<p class=\"mb-1\"><strong>" + user_.full_name + "</strong> · " +
        roleLabel(user_.app_role_id) + "</p>"));
    addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<p class=\"fd-muted mb-0\">@" + user_.username + "</p>"));

    // --- Connection ---
    addNew<Wt::WText>("<div class=\"fd-panel-title mt-3\">Connection</div>");
    addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<p class=\"fd-muted\">JSON:API · " + api_->baseUrl() + "</p>"));
}

void SettingsView::setTheme(const std::string& mode) {
    // Mirrors the header toggle: explicit light/dark sets data-fd-theme +
    // localStorage; "system" clears both so the OS preference decides.
    // Both attributes move together: data-fd-theme (our tokens) + data-bs-theme
    // (Bootstrap 5.3 native components). "system" clears our override and points
    // Bootstrap at the OS preference (Bootstrap doesn't auto-follow the OS).
    if (mode == "system") {
        Wt::WApplication::instance()->doJavaScript(
            "(function(){var d=document.documentElement;"
            "d.removeAttribute('data-fd-theme');localStorage.removeItem('fd-theme');"
            "var sysDark=window.matchMedia('(prefers-color-scheme: dark)').matches;"
            "d.setAttribute('data-bs-theme',sysDark?'dark':'light');})();");
    } else {
        Wt::WApplication::instance()->doJavaScript(
            "(function(){var d=document.documentElement;"
            "d.setAttribute('data-fd-theme','" + mode + "');"
            "d.setAttribute('data-bs-theme','" + mode + "');"
            "localStorage.setItem('fd-theme','" + mode + "');})();");
    }
}

}  // namespace fd

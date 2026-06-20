#include "Shell.h"

#include <functional>

#include <Wt/WApplication.h>
#include <Wt/WString.h>

#include "CommPanel.h"
#include "FleetView.h"
#include "HudControlBus.h"
#include "MapView.h"
#include "ProfileView.h"
#include "SettingsView.h"
#include "Toaster.h"

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

Shell::Shell(ApiClient* api, AppUser user, std::function<void()> onLogout)
    : api_(api), user_(std::move(user)), onLogout_(std::move(onLogout)) {
    // The shell IS the app frame (full-height flex column): header / body / footer.
    addStyleClass("fd-app");

    // Apply any saved theme override (data-fd-theme) before painting.
    Wt::WApplication::instance()->doJavaScript(
        "(function(){var t=localStorage.getItem('fd-theme');"
        "if(t){document.documentElement.setAttribute('data-fd-theme',t);}})();");

    // Top-right toaster lives above the frame (CSS pins it fixed). Create it
    // first — the comms panel built below takes a pointer to it.
    toaster_ = addNew<Toaster>();

    buildHeader();
    buildBody();
    buildFooter();

    showBoard();
    if (!menuButtons_.empty()) setActiveMenu(menuButtons_.front());  // Board active
    toaster_->notify("Welcome back", user_.full_name, Toaster::Level::Success, 4000);
}

void Shell::buildHeader() {
    auto* header = addNew<Wt::WContainerWidget>();
    header->addStyleClass("fd-header");
    auto* inner = header->addNew<Wt::WContainerWidget>();
    inner->addStyleClass("fd-header-inner");

    // --- Logo branding (top-left) ---
    auto* brand = inner->addNew<Wt::WContainerWidget>();
    brand->addStyleClass("fd-brand");
    brand->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<span class=\"fd-logo\">🚚</span> Fleet Dispatcher"));

    inner->addNew<Wt::WContainerWidget>()->addStyleClass("fd-header-spacer");

    // --- Tools + profile/role (top-right) ---
    auto* tools = inner->addNew<Wt::WContainerWidget>();
    tools->addStyleClass("fd-header-tools");

    auto iconBtn = [&](const Wt::WString& glyph, const char* tip,
                       void (Shell::*slot)()) {
        auto* b = tools->addNew<Wt::WPushButton>(glyph);
        b->addStyleClass("btn btn-sm btn-icon");
        b->setToolTip(tip);
        b->clicked().connect(this, slot);
        return b;
    };
    // Panel toggles use the disclosure aesthetic: ▼ when open, a pointing arrow
    // when closed (▶ for the left panel, ◀ for the right).
    leftToggleBtn_ =
        iconBtn(Wt::WString::fromUTF8("▼"), "Hide / show left panel", &Shell::toggleLeft);
    rightToggleBtn_ =
        iconBtn(Wt::WString::fromUTF8("▼"), "Hide / show right panel", &Shell::toggleRight);
    iconBtn(Wt::WString::fromUTF8("◐"), "Toggle light / dark theme",
            &Shell::toggleTheme);

    // Profile (name) + logged-in role.
    auto* profileBtn = tools->addNew<Wt::WPushButton>(
        Wt::WString::fromUTF8(user_.full_name));
    profileBtn->addStyleClass("btn btn-sm fd-profile-btn");
    profileBtn->setToolTip("Profile");
    profileBtn->clicked().connect(this, &Shell::showProfile);

    auto* role = tools->addNew<Wt::WText>(
        Wt::WString::fromUTF8(roleLabel(user_.app_role_id)));
    role->addStyleClass("fd-role-badge");

    auto* signOut = tools->addNew<Wt::WPushButton>("Sign out");
    signOut->addStyleClass("btn btn-sm btn-icon");
    signOut->clicked().connect([this] {
        api_->clearAuthToken();
        if (onLogout_) onLogout_();
    });
}

void Shell::buildBody() {
    auto* body = addNew<Wt::WContainerWidget>();
    body->addStyleClass("fd-body");

    // Left panel — menuing system + work-panel toggles (collapsible).
    leftPanel_ = body->addNew<Wt::WContainerWidget>();
    leftPanel_->addStyleClass("fd-panel fd-panel-left");
    buildLeftMenu();

    // Center — the work panel (active view outlet).
    auto* center = body->addNew<Wt::WContainerWidget>();
    center->addStyleClass("fd-center");
    content_ = center->addNew<Wt::WContainerWidget>();
    content_->addStyleClass("fd-work");

    // Right panel — communications (channels), collapsible.
    rightPanel_ = body->addNew<Wt::WContainerWidget>();
    rightPanel_->addStyleClass("fd-panel fd-panel-right");
    rightPanel_->addNew<CommPanel>(api_, user_, toaster_);
}

void Shell::buildLeftMenu() {
    leftPanel_->addNew<Wt::WText>("<div class=\"fd-panel-title\">Menu</div>");

    auto* menu = leftPanel_->addNew<Wt::WContainerWidget>();
    menu->addStyleClass("fd-menu");
    auto addItem = [&](const char* label, std::function<void()> on) {
        auto* b = menu->addNew<Wt::WPushButton>(label);
        b->addStyleClass("fd-menu-item btn btn-sm");
        b->clicked().connect([this, b, on] {
            setActiveMenu(b);
            on();
        });
        menuButtons_.push_back(b);
        return b;
    };
    addItem("Board", [this] { showBoard(); });
    addItem("Fleet", [this] { showFleet(); });
    addItem("Map", [this] { showMap(); });
    addItem("New Load", [this] { showLoadForm(); });
    // Selecting Communications lets comms take over the full work area.
    addItem("Communications", [this] { showComms(); });
    addItem("Settings", [this] { showSettings(); });

    // Work-panel toggles: drive how the center renders.
    leftPanel_->addNew<Wt::WText>(
        "<div class=\"fd-panel-title mt-3\">Work panel</div>");

    auto* modeGroup = leftPanel_->addNew<Wt::WContainerWidget>();
    modeGroup->addStyleClass("btn-group w-100 mb-2");
    todayBtn_ = modeGroup->addNew<Wt::WPushButton>("Today");
    weekBtn_ = modeGroup->addNew<Wt::WPushButton>("Week");
    todayBtn_->clicked().connect([this] { setMode(BoardMode::Today); });
    weekBtn_->clicked().connect([this] { setMode(BoardMode::Week); });

    auto* compactBtn = leftPanel_->addNew<Wt::WPushButton>("Compact rows");
    compactBtn->addStyleClass("btn btn-sm btn-outline-secondary w-100");
    compactBtn->clicked().connect(this, &Shell::toggleCompact);
}

void Shell::buildFooter() {
    auto* footer = addNew<Wt::WContainerWidget>();
    footer->addStyleClass("fd-footer");
    auto* inner = footer->addNew<Wt::WContainerWidget>();
    inner->addStyleClass("fd-footer-inner");
    inner->addNew<Wt::WText>(Wt::WString::fromUTF8("<span>© 2026 Fleet Dispatcher</span>"));
    inner->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<span><a href=\"/hud\" target=\"_blank\">HUD</a> · "
        "<a href=\"https://github.com/thomasgpeters/fleet-dispatcher\" "
        "target=\"_blank\">Docs</a> · <a href=\"#\">Support</a></span>"));
}

void Shell::setActiveMenu(Wt::WPushButton* active) {
    for (auto* b : menuButtons_) {
        if (b == active) {
            b->addStyleClass("fd-active");
        } else {
            b->removeStyleClass("fd-active");
        }
    }
}

void Shell::toggleLeft() {
    leftCollapsed_ = !leftCollapsed_;
    if (leftCollapsed_) {
        leftPanel_->addStyleClass("fd-collapsed");
        leftToggleBtn_->setText(Wt::WString::fromUTF8("▶"));  // closed → points right
    } else {
        leftPanel_->removeStyleClass("fd-collapsed");
        leftToggleBtn_->setText(Wt::WString::fromUTF8("▼"));  // open → down arrow
    }
}

void Shell::toggleRight() {
    rightCollapsed_ = !rightCollapsed_;
    if (rightCollapsed_) {
        rightPanel_->addStyleClass("fd-collapsed");
        rightToggleBtn_->setText(Wt::WString::fromUTF8("◀"));  // closed → points left
    } else {
        rightPanel_->removeStyleClass("fd-collapsed");
        rightToggleBtn_->setText(Wt::WString::fromUTF8("▼"));  // open → down arrow
    }
}

void Shell::toggleCompact() {
    compact_ = !compact_;
    if (compact_) content_->addStyleClass("fd-compact");
    else content_->removeStyleClass("fd-compact");
}

void Shell::toggleTheme() {
    // Flip data-fd-theme on <html> relative to the *effective* theme and persist.
    Wt::WApplication::instance()->doJavaScript(
        "(function(){var d=document.documentElement;"
        "var cur=d.getAttribute('data-fd-theme');"
        "var sys=window.matchMedia('(prefers-color-scheme: dark)').matches;"
        "var isDark=cur?(cur==='dark'):sys;"
        "var next=isDark?'light':'dark';"
        "d.setAttribute('data-fd-theme',next);"
        "localStorage.setItem('fd-theme',next);})();");
}

void Shell::setMode(BoardMode mode) {
    mode_ = mode;
    refreshModeButtons();
    if (board_) board_->setMode(mode_);

    // Drive the HUD: in-process push to every HUD session + a recorded command.
    const std::string arg = (mode_ == BoardMode::Today) ? "today" : "week";
    HudControlBus::instance().publish({HudCommand::SetMode, arg});
    api_->postHudCommand("set_mode", arg, [](std::string) { /* best-effort */ });
}

void Shell::refreshModeButtons() {
    const char* active = "btn btn-sm btn-primary";
    const char* idle = "btn btn-sm btn-outline-primary";
    todayBtn_->setStyleClass(mode_ == BoardMode::Today ? active : idle);
    weekBtn_->setStyleClass(mode_ == BoardMode::Week ? active : idle);
}

void Shell::showBoard() {
    exitCommsMode();
    refreshModeButtons();
    content_->clear();
    board_ = content_->addNew<BoardView>(api_, mode_);
}

void Shell::showLoadForm() {
    exitCommsMode();
    board_ = nullptr;
    content_->clear();
    content_->addNew<LoadForm>(api_, [this] {
        if (toaster_) toaster_->notify("Load created", "Added to the board",
                                       Toaster::Level::Success);
        showBoard();
    });
}

void Shell::showProfile() {
    exitCommsMode();
    board_ = nullptr;
    content_->clear();
    content_->addNew<ProfileView>(api_, user_, [this](AppUser updated) {
        user_ = std::move(updated);
        if (toaster_) toaster_->notify("Profile saved", "", Toaster::Level::Success);
    });
}

void Shell::showComms() {
    board_ = nullptr;
    content_->clear();
    // Comms take over the full work area, so hide the (redundant) right rail.
    // Pass a null toaster so only the always-on rail raises new-message toasts
    // — but the rail is collapsed here, so toasts come from the rail instance
    // that still lives in rightPanel_ (just not shown). Avoids duplicates.
    enterCommsMode();
    content_->addNew<CommPanel>(api_, user_, /*toaster=*/nullptr,
                                CommPanel::Layout::Full);
}

void Shell::enterCommsMode() {
    if (commsMode_) return;
    commsMode_ = true;
    rightWasCollapsed_ = rightCollapsed_;
    if (!rightCollapsed_) {
        rightPanel_->addStyleClass("fd-collapsed");  // CSS transitions the width
        rightCollapsed_ = true;
        rightToggleBtn_->setText(Wt::WString::fromUTF8("◀"));
    }
}

void Shell::exitCommsMode() {
    if (!commsMode_) return;
    commsMode_ = false;
    // Only re-open the rail if the user hadn't already collapsed it themselves.
    if (!rightWasCollapsed_ && rightCollapsed_) {
        rightPanel_->removeStyleClass("fd-collapsed");
        rightCollapsed_ = false;
        rightToggleBtn_->setText(Wt::WString::fromUTF8("▼"));
    }
}

void Shell::showFleet() {
    exitCommsMode();
    board_ = nullptr;
    content_->clear();
    content_->addNew<FleetView>(api_);
}

void Shell::showMap() {
    exitCommsMode();
    board_ = nullptr;
    content_->clear();
    content_->addNew<MapView>(api_);
}

void Shell::showSettings() {
    exitCommsMode();
    board_ = nullptr;
    content_->clear();
    content_->addNew<SettingsView>(api_, user_);
}

void Shell::showPlaceholder(const std::string& title) {
    exitCommsMode();
    board_ = nullptr;
    content_->clear();
    content_->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<div class=\"card\"><div class=\"card-body\"><h2 class=\"h5\">" + title +
        "</h2><p class=\"fd-muted\">Coming soon.</p></div></div>"));
}

}  // namespace fd

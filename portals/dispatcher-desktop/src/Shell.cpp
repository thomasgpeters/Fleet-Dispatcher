#include "Shell.h"

#include <functional>

#include <Wt/WApplication.h>
#include <Wt/WString.h>

#include "HudControlBus.h"
#include "ProfileView.h"

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

    buildHeader();
    buildBody();
    buildFooter();

    showBoard();
}

void Shell::buildHeader() {
    auto* header = addNew<Wt::WContainerWidget>();
    header->addStyleClass("fd-header");
    auto* inner = header->addNew<Wt::WContainerWidget>();
    inner->addStyleClass("fd-header-inner");

    // Brand.
    auto* brand = inner->addNew<Wt::WContainerWidget>();
    brand->addStyleClass("fd-brand");
    brand->addNew<Wt::WText>("Fleet Dispatcher");
    brand->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<small>Dispatcher console · API " + api_->baseUrl() + "</small>"));

    // Panel toggles (left).
    auto* leftToggle = inner->addNew<Wt::WPushButton>(Wt::WString::fromUTF8("◧"));
    leftToggle->addStyleClass("btn btn-sm btn-icon");
    leftToggle->setToolTip("Toggle left panel");
    leftToggle->clicked().connect(this, &Shell::toggleLeft);

    // Navigation (full-width header == navigation).
    auto* nav = inner->addNew<Wt::WContainerWidget>();
    nav->addStyleClass("fd-nav");
    auto addNav = [&](const char* label, std::function<void()> on) {
        auto* b = nav->addNew<Wt::WPushButton>(label);
        b->addStyleClass("btn btn-sm");
        b->clicked().connect([this, b, on] {
            setActiveNav(b);
            on();
        });
        navButtons_.push_back(b);
        return b;
    };
    addNav("Board", [this] { showBoard(); });
    addNav("New Load", [this] { showLoadForm(); });
    addNav("Drivers", [this] { showPlaceholder("Drivers"); });
    addNav("Messages", [this] { showPlaceholder("Messages"); });
    addNav("Profile", [this] { showProfile(); });

    inner->addNew<Wt::WContainerWidget>()->addStyleClass("fd-header-spacer");

    // Tools (right): right-panel toggle, theme toggle, user/role, sign out.
    auto* tools = inner->addNew<Wt::WContainerWidget>();
    tools->addStyleClass("fd-header-tools");

    auto* rightToggle = tools->addNew<Wt::WPushButton>(Wt::WString::fromUTF8("◨"));
    rightToggle->addStyleClass("btn btn-sm btn-icon");
    rightToggle->setToolTip("Toggle right panel");
    rightToggle->clicked().connect(this, &Shell::toggleRight);

    auto* themeBtn = tools->addNew<Wt::WPushButton>(Wt::WString::fromUTF8("◐"));
    themeBtn->addStyleClass("btn btn-sm btn-icon");
    themeBtn->setToolTip("Toggle light / dark theme");
    themeBtn->clicked().connect(this, &Shell::toggleTheme);

    userLabel_ = tools->addNew<Wt::WText>();
    userLabel_->addStyleClass("fd-user");
    updateUserLabel();

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

    // Left panel — filters / context (collapsible).
    leftPanel_ = body->addNew<Wt::WContainerWidget>();
    leftPanel_->addStyleClass("fd-panel fd-panel-left");
    leftPanel_->addNew<Wt::WText>("<div class=\"fd-panel-title\">Filters</div>");
    leftPanel_->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<p class=\"fd-muted\">Dispatch week, driver, and status filters land "
        "here.</p>"));

    // Center — command bar + active view.
    auto* center = body->addNew<Wt::WContainerWidget>();
    center->addStyleClass("fd-center");
    commandBar_ = center->addNew<Wt::WContainerWidget>();
    commandBar_->addStyleClass("btn-group mb-3");
    todayBtn_ = commandBar_->addNew<Wt::WPushButton>("Today");
    weekBtn_ = commandBar_->addNew<Wt::WPushButton>("Week");
    todayBtn_->clicked().connect([this] { setMode(BoardMode::Today); });
    weekBtn_->clicked().connect([this] { setMode(BoardMode::Week); });
    content_ = center->addNew<Wt::WContainerWidget>();

    // Right panel — details / inspector (collapsible).
    rightPanel_ = body->addNew<Wt::WContainerWidget>();
    rightPanel_->addStyleClass("fd-panel fd-panel-right");
    rightPanel_->addNew<Wt::WText>("<div class=\"fd-panel-title\">Details</div>");
    rightPanel_->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<p class=\"fd-muted\">Select a load to inspect it here.</p>"));
}

void Shell::buildFooter() {
    auto* footer = addNew<Wt::WContainerWidget>();
    footer->addStyleClass("fd-footer");
    auto* inner = footer->addNew<Wt::WContainerWidget>();
    inner->addStyleClass("fd-footer-inner");
    inner->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<span>© 2026 Fleet Dispatcher</span>"));
    inner->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<span><a href=\"/hud\" target=\"_blank\">HUD</a> · "
        "<a href=\"https://github.com/thomasgpeters/fleet-dispatcher\" "
        "target=\"_blank\">Docs</a> · "
        "<a href=\"#\">Support</a></span>"));
}

void Shell::setActiveNav(Wt::WPushButton* active) {
    for (auto* b : navButtons_) {
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
    } else {
        leftPanel_->removeStyleClass("fd-collapsed");
    }
}

void Shell::toggleRight() {
    rightCollapsed_ = !rightCollapsed_;
    if (rightCollapsed_) {
        rightPanel_->addStyleClass("fd-collapsed");
    } else {
        rightPanel_->removeStyleClass("fd-collapsed");
    }
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

    // Drive the HUD: the same console control pushes a command to every HUD
    // session (in-process, instant) and records it for remote/distributed HUDs.
    const std::string arg = (mode_ == BoardMode::Today) ? "today" : "week";
    HudControlBus::instance().publish({HudCommand::SetMode, arg});
    api_->postHudCommand("set_mode", arg, [](std::string) { /* best-effort */ });
}

void Shell::refreshModeButtons() {
    const char* active = "btn btn-primary";
    const char* idle = "btn btn-outline-primary";
    todayBtn_->setStyleClass(mode_ == BoardMode::Today ? active : idle);
    weekBtn_->setStyleClass(mode_ == BoardMode::Week ? active : idle);
}

void Shell::updateUserLabel() {
    userLabel_->setText(Wt::WString::fromUTF8(
        user_.full_name + " · " + roleLabel(user_.app_role_id)));
}

void Shell::showBoard() {
    commandBar_->show();
    refreshModeButtons();
    content_->clear();
    board_ = content_->addNew<BoardView>(api_, mode_);
}

void Shell::showLoadForm() {
    board_ = nullptr;
    commandBar_->hide();
    content_->clear();
    content_->addNew<LoadForm>(api_, [this] { showBoard(); });
}

void Shell::showProfile() {
    board_ = nullptr;
    commandBar_->hide();
    content_->clear();
    content_->addNew<ProfileView>(api_, user_, [this](AppUser updated) {
        user_ = std::move(updated);
        updateUserLabel();
    });
}

void Shell::showPlaceholder(const std::string& title) {
    board_ = nullptr;
    commandBar_->hide();
    content_->clear();
    content_->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<div class=\"card\"><div class=\"card-body\"><h2 class=\"h5\">" + title +
        "</h2><p class=\"fd-muted\">Coming soon.</p></div></div>"));
}

}  // namespace fd

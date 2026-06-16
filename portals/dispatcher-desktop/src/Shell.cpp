#include "Shell.h"

#include <functional>

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
    addStyleClass("container py-3");

    // --- App bar ---------------------------------------------------------
    auto* bar = addNew<Wt::WContainerWidget>();
    bar->addStyleClass("d-flex justify-content-between align-items-center mb-3");
    auto* brand = bar->addNew<Wt::WContainerWidget>();
    brand->addNew<Wt::WText>("<h1 class=\"h4 mb-0\">Fleet Dispatcher</h1>");
    brand->addNew<Wt::WText>(
        Wt::WString::fromUTF8("<small class=\"text-muted\">Dispatcher console · API " +
                              api_->baseUrl() + "</small>"));

    // Right side: signed-in user + role, then Sign out.
    auto* account = bar->addNew<Wt::WContainerWidget>();
    account->addStyleClass("d-flex align-items-center gap-2");
    userLabel_ = account->addNew<Wt::WText>();
    userLabel_->addStyleClass("text-muted");
    updateUserLabel();
    auto* signOut = account->addNew<Wt::WPushButton>("Sign out");
    signOut->addStyleClass("btn btn-sm btn-outline-secondary");
    signOut->clicked().connect([this] {
        api_->clearAuthToken();
        if (onLogout_) onLogout_();
    });

    // --- Navigation ------------------------------------------------------
    auto* nav = addNew<Wt::WContainerWidget>();
    nav->addStyleClass("btn-group mb-3");
    auto addNav = [&](const char* label, std::function<void()> on) {
        auto* b = nav->addNew<Wt::WPushButton>(label);
        b->addStyleClass("btn btn-outline-secondary");
        b->clicked().connect(std::move(on));
        return b;
    };
    addNav("Board", [this] { showBoard(); });
    addNav("New Load", [this] { showLoadForm(); });
    addNav("Drivers", [this] { showPlaceholder("Drivers"); });
    addNav("Messages", [this] { showPlaceholder("Messages"); });
    addNav("Profile", [this] { showProfile(); });

    // --- Command bar (Today | Week) -------------------------------------
    commandBar_ = addNew<Wt::WContainerWidget>();
    commandBar_->addStyleClass("btn-group mb-3");
    todayBtn_ = commandBar_->addNew<Wt::WPushButton>("Today");
    weekBtn_ = commandBar_->addNew<Wt::WPushButton>("Week");
    todayBtn_->clicked().connect([this] { setMode(BoardMode::Today); });
    weekBtn_->clicked().connect([this] { setMode(BoardMode::Week); });

    // --- Content outlet --------------------------------------------------
    content_ = addNew<Wt::WContainerWidget>();

    showBoard();
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
    // On success, return to the board (which reloads from the API).
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

void Shell::updateUserLabel() {
    userLabel_->setText(Wt::WString::fromUTF8(
        user_.full_name + " · " + roleLabel(user_.app_role_id)));
}

void Shell::showPlaceholder(const std::string& title) {
    board_ = nullptr;
    commandBar_->hide();
    content_->clear();
    content_->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<div class=\"card\"><div class=\"card-body\"><h2 class=\"h5\">" + title +
        "</h2><p class=\"text-muted\">Coming soon.</p></div></div>"));
}

}  // namespace fd

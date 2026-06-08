#include "Shell.h"

#include <functional>

#include <Wt/WString.h>
#include <Wt/WText.h>

namespace fd {

Shell::Shell() : api_(std::make_unique<ApiClient>(ApiClient::baseUrlFromEnv())) {
    addStyleClass("container py-3");

    // --- App bar ---------------------------------------------------------
    auto* bar = addNew<Wt::WContainerWidget>();
    bar->addStyleClass("d-flex justify-content-between align-items-center mb-3");
    auto* brand = bar->addNew<Wt::WContainerWidget>();
    brand->addNew<Wt::WText>("<h1 class=\"h4 mb-0\">Fleet Dispatcher</h1>");
    brand->addNew<Wt::WText>(
        Wt::WString::fromUTF8("<small class=\"text-muted\">Dispatcher console · API " +
                              api_->baseUrl() + "</small>"));
    // (Right side: user/role/health — added with auth later.)

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
    board_ = content_->addNew<BoardView>(api_.get(), mode_);
}

void Shell::showLoadForm() {
    board_ = nullptr;
    commandBar_->hide();
    content_->clear();
    // On success, return to the board (which reloads from the API).
    content_->addNew<LoadForm>(api_.get(), [this] { showBoard(); });
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

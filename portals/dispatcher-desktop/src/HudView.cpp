#include "HudView.h"

#include <Wt/WApplication.h>
#include <Wt/WString.h>
#include <Wt/WText.h>

namespace fd {

HudView::HudView(ApiClient* api) : api_(api) {
    addStyleClass("container py-3 fd-hud");

    auto* header = addNew<Wt::WContainerWidget>();
    header->addStyleClass("d-flex justify-content-between align-items-center mb-3");
    header->addNew<Wt::WText>("<h1 class=\"h3 mb-0\">Fleet HUD</h1>");
    modeLabel_ = header->addNew<Wt::WText>();
    modeLabel_->addStyleClass("badge bg-primary");
    modeLabel_->setText("WEEK");

    board_ = addNew<BoardView>(api_, mode_);

    // React to console commands via server push.
    auto* app = Wt::WApplication::instance();
    token_ = HudControlBus::instance().subscribe(
        app->sessionId(), [this](const HudCommand& cmd) {
            apply(cmd);
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
        });
}

HudView::~HudView() {
    HudControlBus::instance().unsubscribe(token_);
}

void HudView::apply(const HudCommand& command) {
    switch (command.type) {
        case HudCommand::SetMode: {
            mode_ = (command.arg == "today") ? BoardMode::Today : BoardMode::Week;
            if (board_) board_->setMode(mode_);
            if (modeLabel_)
                modeLabel_->setText(mode_ == BoardMode::Today ? "TODAY" : "WEEK");
            break;
        }
        case HudCommand::FocusDriver:
        case HudCommand::HighlightLoad:
            // TODO: wire when the board/map gains driver focus & load highlight.
            break;
    }
}

}  // namespace fd

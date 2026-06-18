#include "Toaster.h"

#include <algorithm>

#include <Wt/WApplication.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WText.h>
#include <Wt/WTimer.h>

namespace fd {

namespace {
const char* levelClass(Toaster::Level l) {
    switch (l) {
        case Toaster::Level::Success: return "fd-toast-success";
        case Toaster::Level::Warning: return "fd-toast-warning";
        case Toaster::Level::Danger:  return "fd-toast-danger";
        default:                      return "fd-toast-info";
    }
}
}  // namespace

Toaster::Toaster() {
    // Pinned top-right; the actual fixed positioning is in the CSS.
    addStyleClass("fd-toaster");

    // One sweeper for all toasts — removing a toast never deletes the timer that
    // is firing, so there's no use-after-free.
    sweeper_ = addChild(std::make_unique<Wt::WTimer>());
    sweeper_->setInterval(std::chrono::milliseconds(500));
    sweeper_->timeout().connect(this, &Toaster::sweep);
}

void Toaster::notify(const std::string& title, const std::string& message,
                     Level level, int timeoutMs) {
    auto* toast = addNew<Wt::WContainerWidget>();
    toast->addStyleClass(std::string("fd-toast ") + levelClass(level));

    auto* head = toast->addNew<Wt::WContainerWidget>();
    head->addStyleClass("fd-toast-head");
    head->addNew<Wt::WText>(Wt::WString::fromUTF8("<strong>" + title + "</strong>"));
    auto* close = head->addNew<Wt::WPushButton>(Wt::WString::fromUTF8("×"));
    close->addStyleClass("fd-toast-close");
    // Mark for removal; the sweeper deletes it on the next tick. (Deleting the
    // toast here would destroy the button mid-click.)
    close->clicked().connect([this, toast] { dismiss(toast); });

    if (!message.empty()) {
        auto* bodyText = toast->addNew<Wt::WText>(Wt::WString::fromUTF8(message));
        bodyText->addStyleClass("fd-toast-body");
    }

    const bool sticky = timeoutMs <= 0;
    entries_.push_back(
        {toast,
         std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs),
         sticky});

    if (!sweeper_->isActive()) sweeper_->start();

    if (auto* app = Wt::WApplication::instance()) app->triggerUpdate();
}

void Toaster::dismiss(Wt::WContainerWidget* w) {
    for (Entry& e : entries_) {
        if (e.widget == w) {
            e.sticky = false;
            e.expires = std::chrono::steady_clock::now();  // sweep removes it next tick
        }
    }
    if (!sweeper_->isActive()) sweeper_->start();
}

void Toaster::remove(Wt::WContainerWidget* w) {
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [w](const Entry& e) { return e.widget == w; }),
        entries_.end());
    removeWidget(w);
    if (entries_.empty() && sweeper_->isActive()) sweeper_->stop();
}

void Toaster::sweep() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<Wt::WContainerWidget*> expired;
    for (const Entry& e : entries_) {
        if (!e.sticky && e.expires <= now) expired.push_back(e.widget);
    }
    for (auto* w : expired) remove(w);
}

}  // namespace fd

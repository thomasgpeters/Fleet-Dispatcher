#include "SavedView.h"

#include <Wt/WApplication.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WText.h>

namespace fd {

namespace {
std::string esc(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            default: o += c;
        }
    }
    return o;
}
}  // namespace

SavedView::SavedView(ApiClient* api, AppUser user)
    : api_(api), user_(std::move(user)) {
    addStyleClass("fd-saved");
    addNew<Wt::WText>("<h2 class=\"h5\">Saved messages</h2>");
    list_ = addNew<Wt::WContainerWidget>();
    reload();
}

void SavedView::reload() {
    list_->clear();
    api_->fetchSaved(
        user_.id,
        [this](std::vector<SavedMessage> ss) {
            if (ss.empty()) {
                list_->addNew<Wt::WText>(
                    "<p class=\"fd-muted\">No saved messages yet. Save one from the "
                    "🏷 action in a conversation.</p>");
                if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
                return;
            }
            // Resolve each saved message's body (async; append as they arrive).
            for (const SavedMessage& s : ss) {
                SavedMessage copy = s;
                api_->fetchMessage(
                    s.message_id,
                    [this, copy](Message m) {
                        renderRow(copy, m);
                        if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
                    },
                    [](std::string) { /* skip messages we can't resolve */ });
            }
        },
        [this](std::string err) {
            list_->addNew<Wt::WText>(Wt::WString::fromUTF8(
                "<p class=\"fd-muted\">Couldn't load saved: " + esc(err) + "</p>"));
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
        });
}

void SavedView::renderRow(const SavedMessage& s, const Message& m) {
    auto* row = list_->addNew<Wt::WContainerWidget>();
    row->addStyleClass("fd-saved-item");
    row->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<div class=\"fd-saved-body\">" + esc(m.body) + "</div>"));
    const std::string when =
        s.saved_at.size() >= 10 ? s.saved_at.substr(0, 10) : s.saved_at;
    row->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<div class=\"fd-saved-meta fd-muted\">🔖 saved " + esc(when) + "</div>"));

    auto* remove = row->addNew<Wt::WPushButton>("Remove");
    remove->addStyleClass("btn btn-sm btn-outline-secondary fd-saved-remove");
    const std::string savedId = s.id;
    remove->clicked().connect([this, row, savedId] {
        api_->unsaveMessage(savedId, [](std::string) {});
        row->hide();  // drop it from view immediately
    });
}

}  // namespace fd

#include "CommPanel.h"

#include <algorithm>

#include <Wt/WApplication.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WText.h>
#include <Wt/WTimer.h>

#include "Toaster.h"

namespace fd {

namespace {
std::string snippet(const std::string& s, std::size_t max = 60) {
    std::string out = s;
    if (out.size() > max) out = out.substr(0, max - 1) + "…";
    return out;
}
// Cheap HTML escaping for user-entered message bodies.
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

CommPanel::CommPanel(ApiClient* api, AppUser user, Toaster* toaster)
    : api_(api), user_(std::move(user)), toaster_(toaster) {
    addStyleClass("fd-comm");

    addNew<Wt::WText>("<div class=\"fd-panel-title\">Communications</div>");

    channelList_ = addNew<Wt::WContainerWidget>();
    channelList_->addStyleClass("fd-comm-channels");

    convoTitle_ = addNew<Wt::WText>();
    convoTitle_->addStyleClass("fd-comm-convo-title");

    messageList_ = addNew<Wt::WContainerWidget>();
    messageList_->addStyleClass("fd-comm-messages");

    // Composer.
    auto* composerRow = addNew<Wt::WContainerWidget>();
    composerRow->addStyleClass("fd-comm-composer");
    composer_ = composerRow->addNew<Wt::WLineEdit>();
    composer_->addStyleClass("form-control form-control-sm");
    composer_->setPlaceholderText("Message…");
    composer_->enterPressed().connect(this, &CommPanel::send);
    auto* sendBtn = composerRow->addNew<Wt::WPushButton>("Send");
    sendBtn->addStyleClass("btn btn-sm btn-primary");
    sendBtn->clicked().connect(this, &CommPanel::send);

    loadChannels();

    // Super-dynamic: poll for new messages and toast on arrival.
    poll_ = addChild(std::make_unique<Wt::WTimer>());
    poll_->setInterval(std::chrono::seconds(10));
    poll_->timeout().connect([this] {
        if (!selectedChannelId_.empty()) refreshMessages(/*notifyOnNew=*/true);
    });
    poll_->start();
}

void CommPanel::loadChannels() {
    api_->fetchChannels(
        [this](std::vector<Channel> chs) {
            channels_ = std::move(chs);
            renderChannels();
            if (selectedChannelId_.empty() && !channels_.empty()) {
                selectChannel(channels_.front());
            }
        },
        [this](std::string err) {
            channelList_->clear();
            channelList_->addNew<Wt::WText>(Wt::WString::fromUTF8(
                "<span class=\"fd-muted\">Comms offline: " + esc(err) + "</span>"));
        });
}

void CommPanel::renderChannels() {
    channelList_->clear();
    for (const Channel& c : channels_) {
        auto* btn = channelList_->addNew<Wt::WPushButton>(
            Wt::WString::fromUTF8(c.name));
        btn->addStyleClass("fd-comm-channel btn btn-sm");
        if (c.id == selectedChannelId_) btn->addStyleClass("fd-active");
        Channel copy = c;
        btn->clicked().connect([this, copy] { selectChannel(copy); });
    }
}

void CommPanel::selectChannel(const Channel& c) {
    selectedChannelId_ = c.id;
    selectedChannelName_ = c.name;
    lastLatestId_.clear();
    convoTitle_->setText(Wt::WString::fromUTF8("#" + c.name));
    renderChannels();  // refresh active state
    refreshMessages(/*notifyOnNew=*/false);
}

void CommPanel::refreshMessages(bool notifyOnNew) {
    if (selectedChannelId_.empty()) return;
    const std::string forChannel = selectedChannelId_;
    api_->fetchMessages(
        forChannel,
        [this, forChannel, notifyOnNew](std::vector<Message> msgs) {
            // Ignore a late response for a channel we've since switched away from.
            if (forChannel != selectedChannelId_) return;
            std::sort(msgs.begin(), msgs.end(),
                      [](const Message& a, const Message& b) {
                          return a.posted_at < b.posted_at;
                      });
            renderMessages(msgs, notifyOnNew);
        },
        [](std::string) { /* keep the last render on a transient error */ });
}

void CommPanel::renderMessages(const std::vector<Message>& msgs, bool notifyOnNew) {
    // New-message toast: latest changed, and it isn't ours.
    if (!msgs.empty()) {
        const Message& latest = msgs.back();
        if (notifyOnNew && !lastLatestId_.empty() && latest.id != lastLatestId_ &&
            latest.author_id != user_.id && toaster_) {
            toaster_->notify("New message · #" + selectedChannelName_,
                             snippet(latest.body), Toaster::Level::Info);
        }
        lastLatestId_ = latest.id;
    }

    messageList_->clear();
    // Show the most recent ~30, oldest first.
    const std::size_t maxShown = 30;
    const std::size_t start = msgs.size() > maxShown ? msgs.size() - maxShown : 0;
    for (std::size_t i = start; i < msgs.size(); ++i) {
        const Message& m = msgs[i];
        const bool mine = (m.author_id == user_.id);
        auto* row = messageList_->addNew<Wt::WContainerWidget>();
        row->addStyleClass(mine ? "fd-msg fd-msg-mine" : "fd-msg");
        row->addNew<Wt::WText>(Wt::WString::fromUTF8(
            "<div class=\"fd-msg-body\">" + esc(m.body) + "</div>"));
        const std::string when =
            m.posted_at.size() >= 16 ? m.posted_at.substr(0, 16) : m.posted_at;
        row->addNew<Wt::WText>(Wt::WString::fromUTF8(
            "<div class=\"fd-msg-meta\">" + (mine ? std::string("You") : "—") +
            " · " + esc(when) + "</div>"));
    }
    if (auto* app = Wt::WApplication::instance()) app->triggerUpdate();
}

void CommPanel::send() {
    const std::string body = composer_->text().toUTF8();
    if (body.empty() || selectedChannelId_.empty()) return;
    composer_->setText("");
    api_->createMessage(
        selectedChannelId_, user_.id, body,
        [this](Message) { refreshMessages(/*notifyOnNew=*/false); },
        [this](std::string err) {
            if (toaster_) toaster_->notify("Send failed", err, Toaster::Level::Danger);
        });
}

}  // namespace fd

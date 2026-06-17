#include "CommPanel.h"

#include <algorithm>

#include <Wt/WApplication.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WText.h>
#include <Wt/WTimer.h>

#include "CommBus.h"
#include "Toaster.h"

namespace fd {

namespace {
std::string snippet(const std::string& s, std::size_t max = 60) {
    std::string out = s;
    if (out.size() > max) out = out.substr(0, max - 1) + "…";
    return out;
}
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

    // Real-time push: receive messages sent by other on-server sessions instantly.
    busToken_ = CommBus::instance().subscribe(
        Wt::WApplication::instance()->sessionId(), [this](const Message& m) {
            onPushed(m);
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
        });

    // Slow reconcile poll: bridges off-server messages (e.g. mobile) until the
    // middleware emits change events. On-server traffic arrives via CommBus.
    poll_ = addChild(std::make_unique<Wt::WTimer>());
    poll_->setInterval(std::chrono::seconds(30));
    poll_->timeout().connect([this] {
        if (!selectedChannelId_.empty()) refreshMessages(/*notifyOnNew=*/true);
    });
    poll_->start();
}

CommPanel::~CommPanel() {
    CommBus::instance().unsubscribe(busToken_);
}

std::string CommPanel::channelName(const std::string& id) const {
    for (const Channel& c : channels_)
        if (c.id == id) return c.name;
    return "channel";
}

void CommPanel::loadChannels() {
    api_->fetchChannels(
        [this](std::vector<Channel> chs) {
            channels_ = std::move(chs);
            renderChannels();
            if (selectedChannelId_.empty() && !channels_.empty())
                selectChannel(channels_.front());
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
        auto* btn = channelList_->addNew<Wt::WPushButton>(Wt::WString::fromUTF8(c.name));
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
    renderChannels();
    refreshMessages(/*notifyOnNew=*/false);
}

void CommPanel::refreshMessages(bool notifyOnNew) {
    if (selectedChannelId_.empty()) return;
    const std::string forChannel = selectedChannelId_;
    api_->fetchMessages(
        forChannel,
        [this, forChannel, notifyOnNew](std::vector<Message> msgs) {
            if (forChannel != selectedChannelId_) return;  // switched away
            std::sort(msgs.begin(), msgs.end(),
                      [](const Message& a, const Message& b) {
                          return a.posted_at < b.posted_at;
                      });
            renderMessages(msgs, notifyOnNew);
        },
        [](std::string) { /* keep last render on a transient error */ });
}

void CommPanel::renderMessages(const std::vector<Message>& msgs, bool notifyOnNew) {
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
    const std::size_t maxShown = 30;
    const std::size_t start = msgs.size() > maxShown ? msgs.size() - maxShown : 0;
    for (std::size_t i = start; i < msgs.size(); ++i) renderOne(msgs[i]);
    if (auto* app = Wt::WApplication::instance()) app->triggerUpdate();
}

void CommPanel::renderOne(const Message& m) {
    const bool mine = (m.author_id == user_.id);
    auto* row = messageList_->addNew<Wt::WContainerWidget>();
    row->addStyleClass(mine ? "fd-msg fd-msg-mine" : "fd-msg");
    row->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<div class=\"fd-msg-body\">" + esc(m.body) + "</div>"));
    const std::string when =
        m.posted_at.size() >= 16 ? m.posted_at.substr(0, 16) : m.posted_at;
    row->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<div class=\"fd-msg-meta\">" + (mine ? std::string("You") : "—") + " · " +
        esc(when) + "</div>"));
}

void CommPanel::onPushed(const Message& m) {
    // Our own sends are rendered locally already; ignore the echo.
    if (m.author_id == user_.id) return;
    if (m.channel_id == selectedChannelId_) {
        renderOne(m);
        lastLatestId_ = m.id;
        if (toaster_)
            toaster_->notify("New message · #" + selectedChannelName_,
                             snippet(m.body), Toaster::Level::Info);
    } else if (toaster_) {
        toaster_->notify("New message · #" + channelName(m.channel_id),
                         snippet(m.body), Toaster::Level::Info);
    }
}

void CommPanel::send() {
    const std::string body = composer_->text().toUTF8();
    if (body.empty() || selectedChannelId_.empty()) return;
    composer_->setText("");
    api_->createMessage(
        selectedChannelId_, user_.id, body,
        [this](Message created) {
            renderOne(created);          // show mine immediately (no refetch)
            lastLatestId_ = created.id;
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
            CommBus::instance().publish(created);  // push to other sessions
        },
        [this](std::string err) {
            if (toaster_)
                toaster_->notify("Send failed", err, Toaster::Level::Danger);
        });
}

}  // namespace fd

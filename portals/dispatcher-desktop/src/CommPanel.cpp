#include "CommPanel.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#include <sstream>

#include <Wt/WApplication.h>
#include <Wt/WDialog.h>
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

// Escape a string as a JSON value (used for the channel name we embed in the
// export envelope; the per-resource bodies are already valid JSON).
std::string jsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    char buf[8];
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (c < 0x20) { std::snprintf(buf, sizeof(buf), "\\u%04x", c); o += buf; }
                else o += static_cast<char>(c);  // leave UTF-8 bytes intact
        }
    }
    return o;
}

// Wrap a string as a double-quoted JS string literal for doJavaScript. UTF-8
// bytes pass through (Wt transmits JS as UTF-8); only structural chars escape.
std::string jsStringLiteral(const std::string& s) {
    return "\"" + jsonEscape(s) + "\"";
}

// Current UTC time as ISO-8601 (for the export's exported_at + filename stamp).
std::string isoUtcNow() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[24];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Filename-safe slug: lowercase alnum, runs of other chars -> single '-'.
std::string slug(const std::string& s) {
    std::string o;
    bool dash = false;
    for (unsigned char c : s) {
        if (std::isalnum(c)) {
            o += static_cast<char>(std::tolower(c));
            dash = false;
        } else if (!dash && !o.empty()) {
            o += '-';
            dash = true;
        }
    }
    while (!o.empty() && o.back() == '-') o.pop_back();
    return o.empty() ? "board" : o;
}
}  // namespace

CommPanel::CommPanel(ApiClient* api, AppUser user, Toaster* toaster, Layout layout)
    : api_(api), user_(std::move(user)), toaster_(toaster), layout_(layout) {
    addStyleClass("fd-comm");
    if (layout_ == Layout::Full) {
        addStyleClass("fd-comm-full");
        buildFull();
    } else {
        buildRail();
    }

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

void CommPanel::buildRail() {
    addNew<Wt::WText>("<div class=\"fd-panel-title\">Communications</div>");

    channelList_ = addNew<Wt::WContainerWidget>();
    channelList_->addStyleClass("fd-comm-channels");  // horizontal chips

    buildConvoHead(this, /*full=*/false);  // title + reduced actions

    topicBar_ = addNew<Wt::WContainerWidget>();
    topicBar_->addStyleClass("fd-comm-topics");

    messageList_ = addNew<Wt::WContainerWidget>();
    messageList_->addStyleClass("fd-comm-messages");

    buildComposer(this);
}

void CommPanel::buildFull() {
    // Left: the Channels (Groups) directory — a vertical list grouped by type.
    auto* directory = addNew<Wt::WContainerWidget>();
    directory->addStyleClass("fd-comm-directory");
    directory->addNew<Wt::WText>("<div class=\"fd-panel-title\">Channels</div>");
    channelList_ = directory->addNew<Wt::WContainerWidget>();
    channelList_->addStyleClass("fd-comm-groups");

    // Right: the conversation (toolbar · messages · composer).
    auto* convo = addNew<Wt::WContainerWidget>();
    convo->addStyleClass("fd-comm-convo");
    buildConvoHead(convo, /*full=*/true);  // title + full actions
    topicBar_ = convo->addNew<Wt::WContainerWidget>();
    topicBar_->addStyleClass("fd-comm-topics");
    messageList_ = convo->addNew<Wt::WContainerWidget>();
    messageList_->addStyleClass("fd-comm-messages");
    buildComposer(convo);
}

// The comm-panel header/toolbar: the conversation title + a board-action set.
// Present in BOTH layouts — a reduced (icon) set in the rail, the full (labeled)
// set in the take-over view. Add future actions (stats, status, archive) here,
// gating the rail-only-vs-full ones on `full`.
void CommPanel::buildConvoHead(Wt::WContainerWidget* parent, bool full) {
    auto* head = parent->addNew<Wt::WContainerWidget>();
    head->addStyleClass(full ? "fd-comm-convo-head" : "fd-comm-convo-head fd-comm-head-rail");

    convoTitle_ = head->addNew<Wt::WText>();
    convoTitle_->addStyleClass("fd-comm-convo-title");
    head->addNew<Wt::WContainerWidget>()->addStyleClass("fd-comm-head-spacer");
    exportStatus_ = head->addNew<Wt::WText>();
    exportStatus_->addStyleClass("fd-comm-export-status fd-muted");

    // Export: labeled in the full view, compact icon in the rail.
    auto* exportBtn = head->addNew<Wt::WPushButton>(
        Wt::WString::fromUTF8(full ? "⤓ Export" : "⤓"));
    exportBtn->addStyleClass("btn btn-sm btn-outline-primary fd-comm-export");
    exportBtn->setToolTip("Export this board (messages, members, topics) to a JSON file");
    exportBtn->clicked().connect(this, &CommPanel::exportBoard);
}

void CommPanel::buildComposer(Wt::WContainerWidget* parent) {
    // Reply banner ("Replying to …" + cancel), shown while composing a reply.
    replyBanner_ = parent->addNew<Wt::WContainerWidget>();
    replyBanner_->addStyleClass("fd-comm-reply-banner");
    replyBannerText_ = replyBanner_->addNew<Wt::WText>();
    replyBannerText_->addStyleClass("fd-comm-reply-text fd-muted");
    auto* cancelReplyBtn = replyBanner_->addNew<Wt::WPushButton>(
        Wt::WString::fromUTF8("✕"));
    cancelReplyBtn->addStyleClass("btn btn-sm fd-comm-reply-cancel");
    cancelReplyBtn->clicked().connect(this, &CommPanel::cancelReply);
    replyBanner_->hide();

    // A read-only notice shown in place of the composer when the user can't post
    // (broadcast member, or muted/banned). Hidden by default.
    postNotice_ = parent->addNew<Wt::WText>();
    postNotice_->addStyleClass("fd-comm-post-notice fd-muted");
    postNotice_->hide();

    buildEmojiPanel(parent);  // hidden until the emoji button toggles it

    composerRow_ = parent->addNew<Wt::WContainerWidget>();
    composerRow_->addStyleClass("fd-comm-composer");
    auto* emojiBtn = composerRow_->addNew<Wt::WPushButton>(Wt::WString::fromUTF8("☺"));
    emojiBtn->addStyleClass("btn btn-sm fd-comm-emoji-btn");
    emojiBtn->setToolTip("Emoji");
    emojiBtn->clicked().connect([this] {
        if (emojiPanel_) emojiPanel_->isHidden() ? emojiPanel_->show()
                                                 : emojiPanel_->hide();
    });
    composer_ = composerRow_->addNew<Wt::WLineEdit>();
    composer_->addStyleClass("form-control form-control-sm");
    composer_->setPlaceholderText("Message…");
    composer_->enterPressed().connect(this, &CommPanel::send);
    auto* sendBtn = composerRow_->addNew<Wt::WPushButton>("Send");
    sendBtn->addStyleClass("btn btn-sm btn-primary");
    sendBtn->clicked().connect(this, &CommPanel::send);
}

void CommPanel::buildEmojiPanel(Wt::WContainerWidget* parent) {
    emojiPanel_ = parent->addNew<Wt::WContainerWidget>();
    emojiPanel_->addStyleClass("fd-comm-emoji");
    emojiPanel_->hide();
    // A compact, trucking-relevant set (transport is UTF-8 end to end).
    static const char* kEmoji[] = {
        "👍", "👌", "✅", "❌", "⚠️", "🚚", "🛻", "🚛", "⛽", "🅿️",
        "🕒", "📍", "🗺️", "📦", "📝", "🙏", "😀", "😅", "👀", "🔥"};
    for (const char* e : kEmoji) {
        auto* b = emojiPanel_->addNew<Wt::WPushButton>(Wt::WString::fromUTF8(e));
        b->addStyleClass("btn btn-sm fd-emoji");
        std::string emoji = e;
        b->clicked().connect([this, emoji] {
            if (composer_) composer_->setText(composer_->text().toUTF8() + emoji);
        });
    }
}

void CommPanel::startReply(const Message& m) {
    replyToId_ = m.id;
    if (replyBannerText_)
        replyBannerText_->setText(Wt::WString::fromUTF8(
            "↩ Replying to: " + esc(snippet(m.body))));
    if (replyBanner_) replyBanner_->show();
    if (composer_) composer_->setFocus();
}

void CommPanel::cancelReply() {
    replyToId_.clear();
    if (replyBanner_) replyBanner_->hide();
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
            loadDirectoryMeta();   // badges: my role/standing + unread per channel
            if (selectedChannelId_.empty() && !channels_.empty())
                selectChannel(channels_.front());
        },
        [this](std::string err) {
            channelList_->clear();
            channelList_->addNew<Wt::WText>(Wt::WString::fromUTF8(
                "<span class=\"fd-muted\">Comms offline: " + esc(err) + "</span>"));
        });
}

// Fetch the current user's memberships (role/standing) + per-channel unread, then
// re-render so the directory/rail show badges (mirrors the mobile channel list).
void CommPanel::loadDirectoryMeta() {
    api_->fetchMyMemberships(
        user_.id,
        [this](std::vector<ChannelMember> ms) {
            myMemberships_.clear();
            for (auto& m : ms) myMemberships_[m.channel_id] = m;
            // Unread per channel: messages newer than my last_read_at, not mine.
            for (const Channel& c : channels_) {
                const std::string cid = c.id;
                std::string lastRead;
                auto it = myMemberships_.find(cid);
                if (it != myMemberships_.end()) lastRead = it->second.last_read_at;
                api_->fetchMessages(
                    cid,
                    [this, cid, lastRead](std::vector<Message> msgs) {
                        int n = 0;
                        for (const Message& m : msgs)
                            if (m.author_id != user_.id &&
                                (lastRead.empty() || m.posted_at > lastRead))
                                ++n;
                        if (cid == selectedChannelId_) n = 0;  // currently viewing
                        unread_[cid] = n;
                        renderChannels();
                        if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
                    },
                    [](std::string) {});
            }
            renderChannels();
        },
        [](std::string) { /* badges optional; ignore */ });
}

// Append unread + role/standing badges (WText spans render as XHTML) to a row.
void CommPanel::appendChannelBadges(Wt::WContainerWidget* row,
                                    const std::string& channelId) {
    auto u = unread_.find(channelId);
    if (u != unread_.end() && u->second > 0)
        row->addNew<Wt::WText>(Wt::WString::fromUTF8(
            "<span class=\"fd-badge fd-badge-unread\">" +
            std::to_string(u->second) + "</span>"));
    auto m = myMemberships_.find(channelId);
    if (m != myMemberships_.end()) {
        const int status = m->second.member_status_id;  // 2 muted, 3 banned
        const int role = m->second.member_role_id;      // 1 owner, 3 admin
        if (status == 2 || status == 3)
            row->addNew<Wt::WText>(Wt::WString::fromUTF8(
                "<span class=\"fd-badge fd-badge-danger\">" +
                std::string(status == 3 ? "Banned" : "Muted") + "</span>"));
        if (role == 1)
            row->addNew<Wt::WText>("<span class=\"fd-badge\">Owner</span>");
        else if (role == 3)
            row->addNew<Wt::WText>("<span class=\"fd-badge\">Admin</span>");
    }
}

void CommPanel::renderChannels() {
    channelList_->clear();
    if (layout_ == Layout::Full) { renderDirectory(); return; }
    // Rail: a flat horizontal strip of channel chips (unread count in label).
    for (const Channel& c : channels_) {
        auto u = unread_.find(c.id);
        std::string label = c.name;
        if (u != unread_.end() && u->second > 0)
            label += " (" + std::to_string(u->second) + ")";
        auto* btn = channelList_->addNew<Wt::WPushButton>(Wt::WString::fromUTF8(label));
        btn->addStyleClass("fd-comm-channel btn btn-sm");
        if (c.id == selectedChannelId_) btn->addStyleClass("fd-active");
        Channel copy = c;
        btn->clicked().connect([this, copy] { selectChannel(copy); });
    }
}

void CommPanel::renderDirectory() {
    // Channels (Groups) view: list channels in sections by type
    // (channel_type_id: 1=direct, 2=group, 3=broadcast — see seed_data.sql).
    struct Section { int type; const char* label; };
    static const Section sections[] = {
        {2, "Groups"}, {1, "Direct messages"}, {3, "Broadcast"}};

    bool any = false;
    for (const Section& s : sections) {
        std::vector<const Channel*> in;
        for (const Channel& c : channels_)
            if (c.channel_type_id == s.type) in.push_back(&c);
        if (in.empty()) continue;
        any = true;
        channelList_->addNew<Wt::WText>(Wt::WString::fromUTF8(
            std::string("<div class=\"fd-comm-group-label\">") + s.label + "</div>"));
        for (const Channel* c : in) {
            auto* row = channelList_->addNew<Wt::WContainerWidget>();
            row->addStyleClass("fd-comm-group-row");
            auto* btn =
                row->addNew<Wt::WPushButton>(Wt::WString::fromUTF8(c->name));
            btn->addStyleClass("fd-comm-group-item btn btn-sm");
            if (c->id == selectedChannelId_) btn->addStyleClass("fd-active");
            Channel copy = *c;
            btn->clicked().connect([this, copy] { selectChannel(copy); });
            appendChannelBadges(row, c->id);
        }
    }
    if (!any) {
        channelList_->addNew<Wt::WText>(
            "<div class=\"fd-muted\">No channels yet.</div>");
    }
}

void CommPanel::selectChannel(const Channel& c) {
    selectedChannelId_ = c.id;
    selectedChannelName_ = c.name;
    selectedTopicId_.clear();   // back to General on channel switch
    lastLatestId_.clear();
    allMessages_.clear();
    topics_.clear();
    convoTitle_->setText(Wt::WString::fromUTF8("#" + c.name));
    // Entering a channel clears its unread + stamps it read.
    unread_[c.id] = 0;
    {
        auto it = myMemberships_.find(c.id);
        if (it != myMemberships_.end())
            api_->markChannelRead(it->second.id, [](std::string) {});
    }
    renderChannels();
    renderTopicBar();           // General-only until topics arrive
    refreshMessages(/*notifyOnNew=*/false);
    loadTopics();

    // Gate the composer by this user's role + standing in the channel (P1/P2).
    members_.clear();
    updatePostPermission();  // optimistic (composer on) until members arrive
    const std::string forChannel = selectedChannelId_;
    api_->fetchChannelMembers(
        forChannel,
        [this, forChannel](std::vector<ChannelMember> ms) {
            if (forChannel != selectedChannelId_) return;  // switched away
            members_ = std::move(ms);
            updatePostPermission();
            renderTopicBar();  // members known -> reveal "New" for managers
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
        },
        [](std::string) { /* leave composer enabled on a transient error */ });
}

void CommPanel::loadTopics() {
    const std::string forChannel = selectedChannelId_;
    api_->fetchTopics(
        forChannel,
        [this, forChannel](std::vector<Topic> ts) {
            if (forChannel != selectedChannelId_) return;
            topics_ = std::move(ts);
            renderTopicBar();
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
        },
        [](std::string) { /* topics optional; ignore transient errors */ });
}

bool CommPanel::canManageTopics() const {
    // Dispatcher app-role, or owner/admin in this channel (mirrors server rule).
    if (user_.app_role_id == 1) return true;  // 1 = dispatcher (seed_data.sql)
    for (const ChannelMember& m : members_)
        if (m.user_id == user_.id)
            return m.member_role_id == 1 || m.member_role_id == 3;  // owner/admin
    return false;
}

void CommPanel::renderTopicBar() {
    if (!topicBar_) return;
    topicBar_->clear();
    // Nothing to show if there are no topics and the user can't add any.
    if (topics_.empty() && !canManageTopics()) return;

    auto chip = [&](const std::string& label, const std::string& id) {
        auto* b = topicBar_->addNew<Wt::WPushButton>(Wt::WString::fromUTF8(label));
        b->addStyleClass("fd-comm-topic btn btn-sm");
        if (id == selectedTopicId_) b->addStyleClass("fd-active");
        b->clicked().connect([this, id] { selectTopic(id); });
    };
    chip("General", "");                       // the no-topic stream
    for (const Topic& t : topics_)
        chip(t.name + (t.is_closed ? " (closed)" : ""), t.id);

    if (canManageTopics()) {
        auto* add = topicBar_->addNew<Wt::WPushButton>(Wt::WString::fromUTF8("+ Topic"));
        add->addStyleClass("fd-comm-topic-add btn btn-sm btn-outline-primary");
        add->setToolTip("Create a topic (admins/dispatchers)");
        add->clicked().connect(this, &CommPanel::promptNewTopic);
    }
}

void CommPanel::selectTopic(const std::string& topicId) {
    selectedTopicId_ = topicId;
    renderTopicBar();
    renderTimeline();
}

void CommPanel::promptNewTopic() {
    if (selectedChannelId_.empty()) return;
    auto* dialog = addChild(std::make_unique<Wt::WDialog>("New topic"));
    auto* edit = dialog->contents()->addNew<Wt::WLineEdit>();
    edit->setPlaceholderText("Topic name");
    edit->addStyleClass("form-control");
    auto* create = dialog->footer()->addNew<Wt::WPushButton>("Create");
    create->addStyleClass("btn btn-sm btn-primary");
    auto* cancel = dialog->footer()->addNew<Wt::WPushButton>("Cancel");
    cancel->addStyleClass("btn btn-sm");

    const std::string forChannel = selectedChannelId_;
    auto submit = [this, dialog, edit, forChannel] {
        const std::string name = edit->text().toUTF8();
        if (!name.empty()) {
            api_->createTopic(
                forChannel, name, user_.id,
                [this, forChannel](Topic t) {
                    if (forChannel != selectedChannelId_) return;
                    topics_.push_back(std::move(t));
                    renderTopicBar();
                    if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
                },
                [this](std::string err) {
                    if (toaster_)
                        toaster_->notify("Couldn't create topic", err,
                                         Toaster::Level::Danger);
                });
        }
        dialog->accept();
    };
    create->clicked().connect(submit);
    edit->enterPressed().connect(submit);
    cancel->clicked().connect([dialog] { dialog->reject(); });
    dialog->finished().connect([this, dialog](Wt::DialogCode) { removeChild(dialog); });
    dialog->show();
}

void CommPanel::updatePostPermission() {
    if (!composerRow_ || !postNotice_) return;

    // Lookup ids — must match database/seed_data.sql.
    constexpr int CHANNEL_BROADCAST = 3;
    constexpr int ROLE_OWNER = 1, ROLE_ADMIN = 3;
    constexpr int STATUS_MUTED = 2, STATUS_BANNED = 3;

    int channelType = 0;
    for (const Channel& c : channels_)
        if (c.id == selectedChannelId_) channelType = c.channel_type_id;

    const ChannelMember* me = nullptr;
    for (const ChannelMember& m : members_)
        if (m.user_id == user_.id) { me = &m; break; }

    std::string reason;
    if (me) {
        const bool restricted =
            (me->member_status_id == STATUS_MUTED || me->member_status_id == STATUS_BANNED) &&
            (me->restricted_until.empty() || me->restricted_until > isoUtcNow());
        if (me->member_status_id == STATUS_BANNED && restricted)
            reason = "You are banned from this channel.";
        else if (restricted)
            reason = "You are muted in this channel.";
        else if (channelType == CHANNEL_BROADCAST &&
                 me->member_role_id != ROLE_OWNER && me->member_role_id != ROLE_ADMIN)
            reason = "Only admins can post in this broadcast channel.";
    }
    // members_ empty (not yet loaded / error): stay optimistic, no reason.

    if (reason.empty()) {
        composerRow_->show();
        postNotice_->hide();
    } else {
        composerRow_->hide();
        postNotice_->setText(Wt::WString::fromUTF8("🔒 " + reason));
        postNotice_->show();
    }
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
    allMessages_ = msgs;   // full channel set; the timeline filters by topic
    renderTimeline();
}

bool CommPanel::inSelectedTopic(const Message& m) const {
    return selectedTopicId_.empty() ? m.topic_id.empty()
                                    : m.topic_id == selectedTopicId_;
}

void CommPanel::renderTimeline() {
    messageList_->clear();
    seenIds_.clear();
    std::vector<const Message*> view;
    for (const Message& m : allMessages_)
        if (inSelectedTopic(m)) view.push_back(&m);
    const std::size_t maxShown = 30;
    const std::size_t start = view.size() > maxShown ? view.size() - maxShown : 0;
    for (std::size_t i = start; i < view.size(); ++i) renderOne(*view[i]);
    if (auto* app = Wt::WApplication::instance()) app->triggerUpdate();
}

void CommPanel::renderOne(const Message& m) {
    seenIds_.insert(m.id);
    const bool mine = (m.author_id == user_.id);
    auto* row = messageList_->addNew<Wt::WContainerWidget>();
    row->addStyleClass(mine ? "fd-msg fd-msg-mine" : "fd-msg");

    // Quoted reply: resolve the target from the loaded set (else a placeholder).
    if (!m.reply_to_id.empty()) {
        std::string quoted = "quoted message";
        for (const Message& x : allMessages_)
            if (x.id == m.reply_to_id) { quoted = snippet(x.body); break; }
        row->addNew<Wt::WText>(Wt::WString::fromUTF8(
            "<div class=\"fd-msg-quote\">↩ " + esc(quoted) + "</div>"));
    }

    row->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<div class=\"fd-msg-body\">" + esc(m.body) + "</div>"));
    const std::string when =
        m.posted_at.size() >= 16 ? m.posted_at.substr(0, 16) : m.posted_at;
    row->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<div class=\"fd-msg-meta\">" + (mine ? std::string("You") : "—") + " · " +
        esc(when) + "</div>"));

    // Per-message reply action.
    auto* reply = row->addNew<Wt::WPushButton>(Wt::WString::fromUTF8("↩"));
    reply->addStyleClass("btn btn-sm fd-msg-reply");
    reply->setToolTip("Reply");
    Message copy = m;
    reply->clicked().connect([this, copy] { startReply(copy); });
}

void CommPanel::onPushed(const Message& m) {
    // De-dup: a message can arrive both intra-server (CommBus) and via the
    // external bridge (ALS->Kafka->RealtimeClient->CommBus) — keep it once.
    if (m.channel_id == selectedChannelId_) {
        for (const Message& x : allMessages_)
            if (!m.id.empty() && x.id == m.id) return;  // already have it
        allMessages_.push_back(m);
        lastLatestId_ = m.id;
        renderTimeline();  // shows only if it's in the current topic view
        if (toaster_ && m.author_id != user_.id)
            toaster_->notify("New message · #" + selectedChannelName_,
                             snippet(m.body), Toaster::Level::Info);
    } else if (m.author_id != user_.id) {
        // Another channel: bump its unread badge and toast.
        unread_[m.channel_id] = (unread_.count(m.channel_id) ? unread_[m.channel_id] : 0) + 1;
        renderChannels();
        if (toaster_)
            toaster_->notify("New message · #" + channelName(m.channel_id),
                             snippet(m.body), Toaster::Level::Info);
    }
}

void CommPanel::send() {
    const std::string body = composer_->text().toUTF8();
    if (body.empty() || selectedChannelId_.empty()) return;
    composer_->setText("");
    if (emojiPanel_) emojiPanel_->hide();
    api_->createMessage(
        selectedChannelId_, user_.id, body, selectedTopicId_, replyToId_,
        [this](Message created) {
            allMessages_.push_back(created);
            lastLatestId_ = created.id;
            renderTimeline();            // show mine immediately (no refetch)
            if (auto* a = Wt::WApplication::instance()) a->triggerUpdate();
            CommBus::instance().publish(created);  // push to other sessions
        },
        [this](std::string err) {
            if (toaster_)
                toaster_->notify("Send failed", err, Toaster::Level::Danger);
        });
    cancelReply();  // reply target captured in the call above; clear the banner
}

void CommPanel::exportBoard() {
    if (selectedChannelId_.empty()) return;
    if (exportStatus_) exportStatus_->setText(Wt::WString::fromUTF8("Exporting…"));

    // Pull the board's data as raw JSON:API documents (messages → members →
    // topics), then assemble + download. Chained so the bundle is consistent and
    // we keep a single failure path. Captured by value; the panel owns api_.
    const std::string ch = selectedChannelId_;
    // High page limit so the export isn't truncated by SAFRS pagination. Very
    // large boards should use the DB-level dump path (docs/TODO Feature 4).
    const std::string pg = "&page%5Blimit%5D=10000";
    auto fail = [this](const std::string& e) {
        if (exportStatus_)
            exportStatus_->setText(Wt::WString::fromUTF8("Export failed: " + e));
    };
    api_->fetchRaw(
        "Message?filter%5Bchannel_id%5D=" + ch + pg,
        [this, ch, pg, fail](std::string messages) {
            api_->fetchRaw(
                "ChannelMember?filter%5Bchannel_id%5D=" + ch + pg,
                [this, ch, pg, fail, messages](std::string members) {
                    api_->fetchRaw(
                        "ChannelTopic?filter%5Bchannel_id%5D=" + ch + pg,
                        [this, messages, members](std::string topics) {
                            finishExport(messages, members, topics);
                        },
                        fail);
                },
                fail);
        },
        fail);
}

void CommPanel::finishExport(const std::string& messages, const std::string& members,
                             const std::string& topics) {
    // Channel metadata from what we already hold.
    std::string name = selectedChannelName_;
    int type = 0;
    for (const Channel& c : channels_)
        if (c.id == selectedChannelId_) { name = c.name; type = c.channel_type_id; }

    const std::string when = isoUtcNow();
    std::ostringstream doc;
    doc << "{\"export_version\":1,"
        << "\"exported_at\":\"" << when << "\","
        << "\"channel\":{\"id\":\"" << jsonEscape(selectedChannelId_) << "\","
        << "\"name\":\"" << jsonEscape(name) << "\","
        << "\"channel_type_id\":" << type << "},"
        // The per-resource bodies are already valid JSON:API documents.
        << "\"topics\":" << topics << ","
        << "\"members\":" << members << ","
        << "\"messages\":" << messages << "}";

    const std::string filename = "board-" + slug(name) + "-" + when.substr(0, 10) + ".json";

    // Trigger a client-side download via a Blob object URL (robust for larger
    // payloads; avoids data: URL length limits). All values are JS-escaped.
    if (auto* app = Wt::WApplication::instance()) {
        app->doJavaScript(
            "(function(){var s=" + jsStringLiteral(doc.str()) + ";"
            "var b=new Blob([s],{type:'application/json'});"
            "var u=URL.createObjectURL(b);"
            "var a=document.createElement('a');a.href=u;a.download=" +
            jsStringLiteral(filename) + ";"
            "document.body.appendChild(a);a.click();a.remove();"
            "setTimeout(function(){URL.revokeObjectURL(u);},1000);})();");
    }
    if (exportStatus_) exportStatus_->setText(Wt::WString::fromUTF8("Exported ✓"));
}

}  // namespace fd

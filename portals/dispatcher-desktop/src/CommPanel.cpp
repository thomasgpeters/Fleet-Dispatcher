#include "CommPanel.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <sstream>

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
    auto* composerRow = parent->addNew<Wt::WContainerWidget>();
    composerRow->addStyleClass("fd-comm-composer");
    composer_ = composerRow->addNew<Wt::WLineEdit>();
    composer_->addStyleClass("form-control form-control-sm");
    composer_->setPlaceholderText("Message…");
    composer_->enterPressed().connect(this, &CommPanel::send);
    auto* sendBtn = composerRow->addNew<Wt::WPushButton>("Send");
    sendBtn->addStyleClass("btn btn-sm btn-primary");
    sendBtn->clicked().connect(this, &CommPanel::send);
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
    if (layout_ == Layout::Full) { renderDirectory(); return; }
    // Rail: a flat horizontal strip of channel chips.
    for (const Channel& c : channels_) {
        auto* btn = channelList_->addNew<Wt::WPushButton>(Wt::WString::fromUTF8(c.name));
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
            auto* btn =
                channelList_->addNew<Wt::WPushButton>(Wt::WString::fromUTF8(c->name));
            btn->addStyleClass("fd-comm-group-item btn btn-sm");
            if (c->id == selectedChannelId_) btn->addStyleClass("fd-active");
            Channel copy = *c;
            btn->clicked().connect([this, copy] { selectChannel(copy); });
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
    seenIds_.clear();
    const std::size_t maxShown = 30;
    const std::size_t start = msgs.size() > maxShown ? msgs.size() - maxShown : 0;
    for (std::size_t i = start; i < msgs.size(); ++i) renderOne(msgs[i]);
    if (auto* app = Wt::WApplication::instance()) app->triggerUpdate();
}

void CommPanel::renderOne(const Message& m) {
    seenIds_.insert(m.id);
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
    // De-dup: a message can arrive both intra-server (CommBus) and via the
    // external bridge (ALS->Kafka->RealtimeClient->CommBus) — render it once.
    if (!m.id.empty() && seenIds_.count(m.id)) return;

    if (m.channel_id == selectedChannelId_) {
        renderOne(m);  // inserts into seenIds_
        lastLatestId_ = m.id;
        if (toaster_ && m.author_id != user_.id)
            toaster_->notify("New message · #" + selectedChannelName_,
                             snippet(m.body), Toaster::Level::Info);
    } else if (toaster_ && m.author_id != user_.id) {
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

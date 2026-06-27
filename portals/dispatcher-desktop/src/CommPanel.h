// CommPanel — the communications surface: the channel list, the selected
// conversation, and a composer.
//
// Real-time: subscribes to CommBus and renders pushed messages instantly over
// Wt's WebSocket server push (no polling for on-server traffic). A slow reconcile
// poll bridges messages that originate off-server (e.g. the mobile app) until the
// middleware emits change events. New messages from others raise a top-right toast.
#pragma once

#include <set>
#include <string>
#include <vector>

#include <Wt/WContainerWidget.h>

#include "ApiClient.h"
#include "models.h"

namespace Wt {
class WLineEdit;
class WText;
class WTimer;
}  // namespace Wt

namespace fd {

class Toaster;

class CommPanel : public Wt::WContainerWidget {
public:
    // Rail: the compact right-rail surface (horizontal channel chips).
    // Full:  the take-over view — a vertical Channels (Groups) directory on the
    //        left + the conversation on the right.
    enum class Layout { Rail, Full };

    CommPanel(ApiClient* api, AppUser user, Toaster* toaster,
              Layout layout = Layout::Rail);
    ~CommPanel() override;

private:
    ApiClient* api_;
    AppUser user_;
    Toaster* toaster_;
    Layout layout_;

    std::vector<Channel> channels_;
    std::vector<ChannelMember> members_;   // members of the selected channel
    std::vector<Topic> topics_;            // topics of the selected channel
    std::vector<Message> allMessages_;     // the channel's full set (filtered per topic)
    std::string selectedChannelId_;
    std::string selectedChannelName_;
    std::string selectedTopicId_;          // empty = the General stream
    std::string lastLatestId_;  // newest message id seen (reconcile-poll dedupe)
    std::string busToken_;      // CommBus subscription
    std::set<std::string> seenIds_;  // rendered message ids (de-dup intra/bridge)

    Wt::WContainerWidget* channelList_ = nullptr;
    Wt::WContainerWidget* topicBar_ = nullptr;   // General + topic chips (P3)
    Wt::WText* convoTitle_ = nullptr;
    Wt::WText* exportStatus_ = nullptr;  // Full layout only (export feedback)
    Wt::WContainerWidget* messageList_ = nullptr;
    Wt::WContainerWidget* composerRow_ = nullptr;  // hidden when the user can't post
    Wt::WText* postNotice_ = nullptr;              // why posting is blocked
    Wt::WLineEdit* composer_ = nullptr;
    Wt::WTimer* poll_ = nullptr;

    void buildRail();          // compact right-rail layout
    void buildFull();          // directory + conversation take-over layout
    void buildConvoHead(Wt::WContainerWidget* parent, bool full);  // title + actions
    void buildComposer(Wt::WContainerWidget* parent);
    void loadChannels();
    void renderChannels();
    void renderDirectory();    // grouped vertical channel list (Full layout)
    void selectChannel(const Channel& c);
    void updatePostPermission();  // gate the composer by role/standing (P1/P2)
    void loadTopics();            // fetch the selected channel's topics (P3)
    void renderTopicBar();        // General + topic chips (+ gated "New")
    void selectTopic(const std::string& topicId);  // "" = General
    bool canManageTopics() const; // owner/admin or dispatcher app-role
    void promptNewTopic();        // dialog -> createTopic (managers only)
    void refreshMessages(bool notifyOnNew);
    void renderMessages(const std::vector<Message>& msgs, bool notifyOnNew);
    void renderTimeline();        // render allMessages_ filtered by topic
    bool inSelectedTopic(const Message& m) const;
    void renderOne(const Message& m);          // append a single message row
    void onPushed(const Message& m);           // CommBus delivery (server push)
    std::string channelName(const std::string& id) const;
    void send();
    void exportBoard();                        // Full layout: download a board bundle
    void finishExport(const std::string& messages, const std::string& members,
                      const std::string& topics);
};

}  // namespace fd

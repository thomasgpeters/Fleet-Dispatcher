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
    CommPanel(ApiClient* api, AppUser user, Toaster* toaster);
    ~CommPanel() override;

private:
    ApiClient* api_;
    AppUser user_;
    Toaster* toaster_;

    std::vector<Channel> channels_;
    std::string selectedChannelId_;
    std::string selectedChannelName_;
    std::string lastLatestId_;  // newest message id seen (reconcile-poll dedupe)
    std::string busToken_;      // CommBus subscription
    std::set<std::string> seenIds_;  // rendered message ids (de-dup intra/bridge)

    Wt::WContainerWidget* channelList_ = nullptr;
    Wt::WText* convoTitle_ = nullptr;
    Wt::WContainerWidget* messageList_ = nullptr;
    Wt::WLineEdit* composer_ = nullptr;
    Wt::WTimer* poll_ = nullptr;

    void loadChannels();
    void renderChannels();
    void selectChannel(const Channel& c);
    void refreshMessages(bool notifyOnNew);
    void renderMessages(const std::vector<Message>& msgs, bool notifyOnNew);
    void renderOne(const Message& m);          // append a single message row
    void onPushed(const Message& m);           // CommBus delivery (server push)
    std::string channelName(const std::string& id) const;
    void send();
};

}  // namespace fd

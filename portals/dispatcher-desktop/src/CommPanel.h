// CommPanel — the right-panel communications surface: the channel list, the
// selected conversation (live), and a composer. Polls for new messages and
// raises a top-right toast when one arrives (see Toaster). This is the "always
// on" comms rail for the dispatcher console.
#pragma once

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

private:
    ApiClient* api_;
    AppUser user_;
    Toaster* toaster_;

    std::vector<Channel> channels_;
    std::string selectedChannelId_;
    std::string selectedChannelName_;
    std::string lastLatestId_;  // newest message id seen (for new-message toasts)

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
    void send();
};

}  // namespace fd

// Account & profile view for the dispatcher console: shows the signed-in user
// and lets them edit their profile fields (PATCH /AppUser/{id}).
#pragma once

#include <functional>

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "ApiClient.h"
#include "models.h"

namespace fd {

class ProfileView : public Wt::WContainerWidget {
public:
    // `onSaved(updated)` runs after a successful save so the shell can refresh
    // the app-bar name/role.
    ProfileView(ApiClient* api, AppUser user,
                std::function<void(AppUser)> onSaved);

private:
    ApiClient* api_;
    AppUser user_;
    std::function<void(AppUser)> onSaved_;

    Wt::WLineEdit* fullName_ = nullptr;
    Wt::WLineEdit* email_ = nullptr;
    Wt::WLineEdit* phone_ = nullptr;
    Wt::WLineEdit* title_ = nullptr;
    Wt::WLineEdit* timezone_ = nullptr;
    Wt::WPushButton* save_ = nullptr;
    Wt::WText* status_ = nullptr;

    Wt::WLineEdit* addField(Wt::WContainerWidget* parent, const std::string& label,
                            const std::string& value);
    void save();
};

}  // namespace fd

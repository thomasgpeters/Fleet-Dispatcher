// Sign-in screen for the dispatcher console. Authenticates against the shared
// ApiLogicServer JWT auth (same credentials as the mobile app), then loads the
// signed-in user's profile and hands it to the caller.
#pragma once

#include <functional>

#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPasswordEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "ApiClient.h"
#include "models.h"

namespace fd {

class LoginView : public Wt::WContainerWidget {
public:
    // `api` is owned by the application; on success `onAuthenticated(user)` runs
    // (the bearer token is already set on `api`).
    LoginView(ApiClient* api, std::function<void(AppUser)> onAuthenticated);

private:
    ApiClient* api_;
    std::function<void(AppUser)> onAuthenticated_;

    Wt::WLineEdit* username_ = nullptr;
    Wt::WPasswordEdit* password_ = nullptr;   // WPasswordEdit: masked, non-deprecated
    Wt::WPushButton* submit_ = nullptr;
    Wt::WText* error_ = nullptr;

    void submit();
    void showError(const std::string& message);
};

}  // namespace fd

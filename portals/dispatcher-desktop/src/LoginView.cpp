#include "LoginView.h"

#include <Wt/WString.h>

namespace fd {

LoginView::LoginView(ApiClient* api, std::function<void(AppUser)> onAuthenticated)
    : api_(api), onAuthenticated_(std::move(onAuthenticated)) {
    addStyleClass("container py-5");

    auto* row = addNew<Wt::WContainerWidget>();
    row->addStyleClass("row justify-content-center");
    auto* col = row->addNew<Wt::WContainerWidget>();
    col->addStyleClass("col-12 col-md-5");

    auto* card = col->addNew<Wt::WContainerWidget>();
    card->addStyleClass("card shadow-sm");
    auto* body = card->addNew<Wt::WContainerWidget>();
    body->addStyleClass("card-body");

    body->addNew<Wt::WText>("<h1 class=\"h4 mb-1\">Fleet Dispatcher</h1>");
    body->addNew<Wt::WText>(
        "<p class=\"text-muted\">Sign in to the dispatcher console.</p>");

    auto* userGroup = body->addNew<Wt::WContainerWidget>();
    userGroup->addStyleClass("mb-3");
    userGroup->addNew<Wt::WText>("<label class=\"form-label\">Username</label>");
    username_ = userGroup->addNew<Wt::WLineEdit>();
    username_->addStyleClass("form-control");
    username_->setPlaceholderText("e.g. dispatch1");

    auto* passGroup = body->addNew<Wt::WContainerWidget>();
    passGroup->addStyleClass("mb-3");
    passGroup->addNew<Wt::WText>("<label class=\"form-label\">Password</label>");
    password_ = passGroup->addNew<Wt::WPasswordEdit>();  // masked by default
    password_->addStyleClass("form-control");
    // Enter in the password field submits.
    password_->enterPressed().connect(this, &LoginView::submit);

    error_ = body->addNew<Wt::WText>();
    error_->addStyleClass("text-danger d-block mb-2");

    submit_ = body->addNew<Wt::WPushButton>("Sign in");
    submit_->addStyleClass("btn btn-primary w-100");
    submit_->clicked().connect(this, &LoginView::submit);
}

void LoginView::showError(const std::string& message) {
    error_->setText(Wt::WString::fromUTF8(message));
    submit_->setEnabled(true);
    submit_->setText("Sign in");
}

void LoginView::submit() {
    const std::string username = username_->text().toUTF8();
    const std::string password = password_->text().toUTF8();
    if (username.empty() || password.empty()) {
        showError("Enter your username and password.");
        return;
    }
    error_->setText("");
    submit_->setEnabled(false);
    submit_->setText("Signing in…");

    api_->login(
        username, password,
        // Got a token: store it, then load the profile.
        [this, username](std::string token) {
            api_->setAuthToken(std::move(token));
            api_->fetchUserByUsername(
                username,
                [this](AppUser user) { onAuthenticated_(std::move(user)); },
                [this](std::string err) {
                    api_->clearAuthToken();
                    showError("Signed in, but couldn't load your profile: " + err);
                });
        },
        [this](std::string err) {
            showError(err == "invalid credentials"
                          ? "Incorrect username or password."
                          : ("Sign-in failed: " + err));
        });
}

}  // namespace fd

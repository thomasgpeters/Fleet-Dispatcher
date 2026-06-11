#include "ProfileView.h"

#include <Wt/WString.h>

namespace fd {

namespace {
const char* roleLabel(int id) {
    switch (id) {
        case 1: return "Dispatcher";
        case 2: return "Driver";
        case 3: return "Updater";
        default: return "User";
    }
}
}  // namespace

ProfileView::ProfileView(ApiClient* api, AppUser user,
                         std::function<void(AppUser)> onSaved)
    : api_(api), user_(std::move(user)), onSaved_(std::move(onSaved)) {
    addStyleClass("card");
    auto* body = addNew<Wt::WContainerWidget>();
    body->addStyleClass("card-body");

    // Header: name, @username · role.
    body->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<h2 class=\"h5 mb-0\">" + user_.full_name + "</h2>"));
    body->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<p class=\"text-muted\">@" + user_.username + " · " +
        roleLabel(user_.app_role_id) + "</p>"));

    auto* form = body->addNew<Wt::WContainerWidget>();
    fullName_ = addField(form, "Full name", user_.full_name);
    email_ = addField(form, "Email", user_.email);
    phone_ = addField(form, "Phone", user_.phone);
    title_ = addField(form, "Title", user_.title);
    timezone_ = addField(form, "Time zone", user_.timezone);

    status_ = body->addNew<Wt::WText>();
    status_->addStyleClass("d-block text-muted my-2");

    save_ = body->addNew<Wt::WPushButton>("Save profile");
    save_->addStyleClass("btn btn-primary");
    save_->clicked().connect(this, &ProfileView::save);

    // Avatar upload is a follow-up (WFileUpload -> CMS document); the mobile app
    // already supports it. See docs/AUTHENTICATION.md.
}

Wt::WLineEdit* ProfileView::addField(Wt::WContainerWidget* parent,
                                     const std::string& label,
                                     const std::string& value) {
    auto* group = parent->addNew<Wt::WContainerWidget>();
    group->addStyleClass("mb-3");
    group->addNew<Wt::WText>(Wt::WString::fromUTF8(
        "<label class=\"form-label\">" + label + "</label>"));
    auto* edit = group->addNew<Wt::WLineEdit>();
    edit->addStyleClass("form-control");
    edit->setText(Wt::WString::fromUTF8(value));
    return edit;
}

void ProfileView::save() {
    ProfileEdit edit;
    edit.full_name = fullName_->text().toUTF8();
    edit.email = email_->text().toUTF8();
    edit.phone = phone_->text().toUTF8();
    edit.title = title_->text().toUTF8();
    edit.timezone = timezone_->text().toUTF8();

    save_->setEnabled(false);
    status_->setText("Saving…");

    api_->updateUser(
        user_.id, edit,
        [this](AppUser updated) {
            user_ = std::move(updated);
            status_->setText("Profile saved.");
            save_->setEnabled(true);
            if (onSaved_) onSaved_(user_);
        },
        [this](std::string err) {
            status_->setText(Wt::WString::fromUTF8("Save failed: " + err));
            save_->setEnabled(true);
        });
}

}  // namespace fd

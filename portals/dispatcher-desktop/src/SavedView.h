// SavedView — the dispatcher's personal Saved archive: messages they saved
// across any channel (saved_message), each removable. Mirrors the mobile Saved
// page. Reached from the left menu.
#pragma once

#include <Wt/WContainerWidget.h>

#include "ApiClient.h"
#include "models.h"

namespace fd {

class SavedView : public Wt::WContainerWidget {
public:
    SavedView(ApiClient* api, AppUser user);

private:
    ApiClient* api_;
    AppUser user_;
    Wt::WContainerWidget* list_ = nullptr;

    void reload();
    void renderRow(const SavedMessage& s, const Message& m);
};

}  // namespace fd

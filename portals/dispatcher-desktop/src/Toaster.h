// Toaster — top-right transient notifications ("toasters") for the desktop
// console. A single sweeper timer auto-dismisses expired toasts (so there is no
// delete-during-its-own-callback hazard). Styled via the design tokens
// (.fd-toaster / .fd-toast in docroot/css/fleet-dispatcher.css).
#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <Wt/WContainerWidget.h>

namespace Wt { class WTimer; }

namespace fd {

class Toaster : public Wt::WContainerWidget {
public:
    enum class Level { Info, Success, Warning, Danger };

    Toaster();

    // Show a toast; it fades out after timeoutMs (0 = sticky until closed).
    void notify(const std::string& title, const std::string& message,
                Level level = Level::Info, int timeoutMs = 5000);

private:
    struct Entry {
        Wt::WContainerWidget* widget;
        std::chrono::steady_clock::time_point expires;
        bool sticky;
    };

    Wt::WTimer* sweeper_ = nullptr;
    std::vector<Entry> entries_;

    void dismiss(Wt::WContainerWidget* w);  // mark for removal (safe from a click)
    void remove(Wt::WContainerWidget* w);   // actually delete (called from sweep)
    void sweep();
};

}  // namespace fd

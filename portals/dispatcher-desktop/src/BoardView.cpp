#include "BoardView.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <map>

#include <Wt/WGlobal.h>
#include <Wt/WString.h>
#include <Wt/WTable.h>
#include <Wt/WTableCell.h>

#include "Toaster.h"
#include "icons.h"

namespace fd {

namespace {

std::string isoDate(std::time_t t) {
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[11];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

std::string todayIso() { return isoDate(std::time(nullptr)); }

// The seven "YYYY-MM-DD" dates of the current Sunday→Saturday week.
std::array<std::string, 7> currentWeekDays() {
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    int dow = tm.tm_wday;                  // 0=Sun … 6=Sat
    std::time_t sunday = now - static_cast<std::time_t>(dow) * 86400;  // back to Sunday
    std::array<std::string, 7> days;
    for (int i = 0; i < 7; ++i) days[i] = isoDate(sunday + static_cast<std::time_t>(i) * 86400);
    return days;
}

std::string money(const std::string& currency, double v) {
    char b[32];
    std::snprintf(b, sizeof(b), "%.2f", v);
    return (currency.empty() ? "USD" : currency) + " " + b;
}

std::string statusName(int id) {
    switch (id) {
        case 1: return "draft";
        case 2: return "dispatched";
        case 3: return "in transit";
        case 4: return "delivered";
        case 5: return "settled";
        case 6: return "cancelled";
        default: return "#" + std::to_string(id);
    }
}

const char* runTypeName(int id) {
    switch (id) {
        case 1: return "Long-haul";
        case 2: return "Regional";
        default: return "Run";
    }
}

// "$4,200" (whole dollars, thousands-grouped). Non-USD keeps its code prefix.
std::string dollars(const std::string& currency, double v) {
    long long n = std::llround(v);
    const bool neg = n < 0;
    std::string digits = std::to_string(neg ? -n : n);
    std::string grouped;
    int c = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        grouped.push_back(digits[i]);
        if (++c % 3 == 0 && i > 0) grouped.push_back(',');
    }
    std::reverse(grouped.begin(), grouped.end());
    const std::string sym =
        (currency.empty() || currency == "USD") ? "$" : (currency + " ");
    return (neg ? "-" : "") + sym + grouped;
}

// Toast accent from load status: delivered = success, cancelled = danger,
// in-transit = info, else info.
Toaster::Level levelForStatus(int statusId) {
    switch (statusId) {
        case 4: return Toaster::Level::Success;   // delivered
        case 6: return Toaster::Level::Danger;    // cancelled
        default: return Toaster::Level::Info;
    }
}

}  // namespace

BoardView::BoardView(ApiClient* api, BoardMode mode) : api_(api), mode_(mode) {
    addStyleClass("fd-board");
    status_ = addNew<Wt::WText>();
    status_->addStyleClass("text-muted");
    body_ = addNew<Wt::WContainerWidget>();
    // Bottom-right stack for load selections. Fixed-positioned (CSS), so it lives
    // outside body_ and survives the body re-renders on reload / mode switch.
    loadToasts_ = addNew<Toaster>(Toaster::Position::BottomRight);
    reload();
}

void BoardView::showLoadToast(const std::string& driverName, const Load& l) {
    const std::string title =
        driverName + " · " + statusName(l.load_status_id);

    std::string body = "<div>" + dollars(l.currency, l.rate) + " · " +
                       runTypeName(l.run_type_id) + "</div>";
    if (!l.pickup_date.empty()) {
        std::string dates = "Pickup " + l.pickup_date;
        if (!l.delivery_date.empty()) dates += " → Delivery " + l.delivery_date;
        body += "<div class=\"fd-toast-sub\">" + dates + "</div>";
    }
    const long long loaded = std::llround(l.loaded_miles);
    const long long dead = std::llround(l.deadhead_miles);
    if (loaded > 0 || dead > 0) {
        body += "<div class=\"fd-toast-sub\">" + std::to_string(loaded) +
                " mi loaded · " + std::to_string(dead) + " mi deadhead</div>";
    }

    // Sticky (0) so collected loads keep stacking until the dispatcher closes
    // them (× on each). Matches the "collect several" flow.
    loadToasts_->notify(title, body, levelForStatus(l.load_status_id), 0);
}

void BoardView::setMode(BoardMode mode) {
    if (mode_ == mode) return;
    mode_ = mode;
    if (driversLoaded_ && loadsLoaded_) render();
}

void BoardView::reload() {
    driversLoaded_ = loadsLoaded_ = false;
    status_->setText("Loading…");
    body_->clear();

    api_->fetchDrivers(
        [this](std::vector<Driver> ds) {
            drivers_ = std::move(ds);
            driversLoaded_ = true;
            renderWhenReady();
        },
        [this](std::string e) { status_->setText("Drivers: " + e); });

    api_->fetchLoads(
        [this](std::vector<Load> ls) {
            loads_ = std::move(ls);
            loadsLoaded_ = true;
            renderWhenReady();
        },
        [this](std::string e) { status_->setText("Loads: " + e); });
}

void BoardView::renderWhenReady() {
    if (driversLoaded_ && loadsLoaded_) render();
}

void BoardView::render() {
    body_->clear();
    status_->setText("");
    if (mode_ == BoardMode::Today) renderToday();
    else renderWeek();
}

void BoardView::renderToday() {
    const std::string today = todayIso();
    std::map<std::string, std::string> name;
    for (const auto& d : drivers_) name[d.id] = d.name;

    auto* box = body_->addNew<Wt::WContainerWidget>();
    box->addStyleClass("fd-tablecard");
    auto* table = box->addNew<Wt::WTable>();
    table->addStyleClass("fd-data-table");
    table->setHeaderCount(1);
    table->elementAt(0, 0)->addNew<Wt::WText>("Driver");
    table->elementAt(0, 1)->addNew<Wt::WText>("Status");
    table->elementAt(0, 2)->addNew<Wt::WText>("Rate");

    int row = 0;
    for (const auto& l : loads_) {
        if (l.pickup_date != today) continue;
        ++row;
        const std::string driver = l.driver_id.empty()
            ? "(unassigned)" : name.count(l.driver_id) ? name[l.driver_id] : l.driver_id;
        table->elementAt(row, 0)->addNew<Wt::WText>(Wt::WString::fromUTF8(driver));
        table->elementAt(row, 1)->addNew<Wt::WText>(statusName(l.load_status_id));
        table->elementAt(row, 2)->addNew<Wt::WText>(money(l.currency, l.rate));
        // Whole row is clickable → the same load-info toast as the week grid.
        auto* rowCell = table->elementAt(row, 0);
        rowCell->addStyleClass("fd-row-click");
        rowCell->clicked().connect(
            [this, driver, l]() { showLoadToast(driver, l); });
    }
    if (row == 0) {
        body_->addNew<Wt::WText>("<p class=\"text-muted\">No runs scheduled today.</p>");
    }
}

void BoardView::renderWeek() {
    const auto days = currentWeekDays();

    auto* box = body_->addNew<Wt::WContainerWidget>();
    box->addStyleClass("fd-tablecard");
    auto* table = box->addNew<Wt::WTable>();
    table->addStyleClass("fd-week-board");
    table->setHeaderCount(1);                            // header row
    table->setHeaderCount(1, Wt::Orientation::Vertical); // header column
    table->elementAt(0, 0)->addNew<Wt::WText>("Driver");
    // Sunday-first. Full weekday on desktop, single letter on tablet/phone —
    // CSS toggles .fd-day-full / .fd-day-short by viewport (≤1200px).
    static const char* dowFull[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* dowShort[7] = {"S", "M", "T", "W", "T", "F", "S"};
    for (int c = 0; c < 7; ++c) {
        auto* h = table->elementAt(0, c + 1);
        const std::string mmdd = days[c].size() >= 10 ? days[c].substr(5) : days[c];
        h->addNew<Wt::WText>(Wt::WString::fromUTF8(
            "<div class=\"fw-semibold\">"
            "<span class=\"fd-day-full\">" + std::string(dowFull[c]) + "</span>"
            "<span class=\"fd-day-short\">" + std::string(dowShort[c]) + "</span>"
            "</div>"
            "<div class=\"fd-day-date\">" + mmdd + "</div>"));
    }

    int row = 0;
    for (const auto& d : drivers_) {
        ++row;
        // Colour avatar + name, vertically centred in the row (see CSS
        // .fd-week-board td vertical-align + .fd-board-driver).
        table->elementAt(row, 0)->addNew<Wt::WText>(Wt::WString::fromUTF8(
            "<span class=\"fd-board-driver\">" +
            personAvatar(d.name, d.avatar_color_id) +
            "<span class=\"fd-board-driver-name\">" + d.name + "</span></span>"));
        for (int c = 0; c < 7; ++c) {
            auto* cell = table->elementAt(row, c + 1);
            for (const auto& l : loads_) {
                if (l.driver_id == d.id && l.pickup_date == days[c]) {
                    auto* chip = cell->addNew<Wt::WText>(
                        Wt::WString::fromUTF8(statusName(l.load_status_id) + " · " +
                                              money(l.currency, l.rate)));
                    chip->addStyleClass("fd-load-card d-block small");
                    // Click → bottom-right load toast (stacks on repeat). Capture
                    // the load + driver name by value (the vectors may re-render).
                    const std::string driverName = d.name;
                    chip->clicked().connect(
                        [this, driverName, l]() { showLoadToast(driverName, l); });
                }
            }
        }
    }
    if (drivers_.empty()) {
        body_->addNew<Wt::WText>("<p class=\"text-muted\">No drivers.</p>");
    }
}

}  // namespace fd

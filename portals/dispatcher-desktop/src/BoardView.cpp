#include "BoardView.h"

#include <array>
#include <cstdio>
#include <ctime>
#include <map>

#include <Wt/WGlobal.h>
#include <Wt/WString.h>
#include <Wt/WTable.h>
#include <Wt/WTableCell.h>

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

}  // namespace

BoardView::BoardView(ApiClient* api, BoardMode mode) : api_(api), mode_(mode) {
    addStyleClass("fd-board");
    status_ = addNew<Wt::WText>();
    status_->addStyleClass("text-muted");
    body_ = addNew<Wt::WContainerWidget>();
    reload();
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
                }
            }
        }
    }
    if (drivers_.empty()) {
        body_->addNew<Wt::WText>("<p class=\"text-muted\">No drivers.</p>");
    }
}

}  // namespace fd

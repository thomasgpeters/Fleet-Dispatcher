#include "LoadForm.h"

#include <Wt/WComboBox.h>
#include <Wt/WDateEdit.h>
#include <Wt/WDoubleSpinBox.h>
#include <Wt/WLabel.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WText.h>

namespace fd {

LoadForm::LoadForm(ApiClient* api, OnCreated onCreated)
    : api_(api), onCreated_(std::move(onCreated)) {
    addStyleClass("card");
    auto* body = addNew<Wt::WContainerWidget>();
    body->addStyleClass("card-body");
    body->addNew<Wt::WText>("<h2 class=\"h5\">New load</h2>");

    // Required references.
    addCombo("Dispatch week", dispatchWeek_);
    addCombo("Shipper", shipper_);
    addCombo("Receiver", receiver_);
    addCombo("Commodity", commodity_);
    addCombo("Pickup location", pickup_);
    addCombo("Drop-off location", dropoff_);
    addCombo("Run type", runType_);
    addCombo("Status", status_);

    // Assignment (optional).
    addCombo("Driver (optional)", driver_);
    addCombo("Equipment (optional)", equipment_);

    auto numberRow = [&](const std::string& label, double max, double step) {
        auto* row = addNew<Wt::WContainerWidget>();
        row->addStyleClass("mb-2");
        row->addNew<Wt::WLabel>(Wt::WString::fromUTF8(label))->addStyleClass("form-label d-block");
        auto* sb = row->addNew<Wt::WDoubleSpinBox>();
        sb->addStyleClass("form-control");
        sb->setRange(0, max);
        sb->setSingleStep(step);
        return sb;
    };
    rate_ = numberRow("Rate (post-broker)", 1e7, 50.0);
    deadhead_ = numberRow("Dead-head miles", 1e5, 5.0);
    loaded_ = numberRow("Loaded miles", 1e5, 5.0);

    auto dateRow = [&](const std::string& label) {
        auto* row = addNew<Wt::WContainerWidget>();
        row->addStyleClass("mb-2");
        row->addNew<Wt::WLabel>(Wt::WString::fromUTF8(label))->addStyleClass("form-label d-block");
        auto* de = row->addNew<Wt::WDateEdit>();
        de->addStyleClass("form-control");
        return de;
    };
    pickupDate_ = dateRow("Pickup date");
    deliveryDate_ = dateRow("Delivery date");

    auto* actions = addNew<Wt::WContainerWidget>();
    actions->addStyleClass("mt-3");
    auto* submit = actions->addNew<Wt::WPushButton>("Create load");
    submit->addStyleClass("btn btn-primary");
    submit->clicked().connect([this] { this->submit(); });

    message_ = addNew<Wt::WText>();
    message_->addStyleClass("d-block mt-2");

    // Populate combos (required ones get a leading blank; lookups default-select).
    populate("DispatchWeek", "week_start", dispatchWeek_, true);
    populate("Shipper", "name", shipper_, true);
    populate("Receiver", "name", receiver_, true);
    populate("Commodity", "description", commodity_, true);
    populate("Location", "city", pickup_, true);
    populate("Location", "city", dropoff_, true);
    populate("RunType", "name", runType_, false);
    populate("LoadStatus", "name", status_, false);
    populate("Driver", "name", driver_, true);
    populate("Equipment", "unit_number", equipment_, true);
}

Wt::WComboBox* LoadForm::addCombo(const std::string& label, Field& f) {
    auto* row = addNew<Wt::WContainerWidget>();
    row->addStyleClass("mb-2");
    row->addNew<Wt::WLabel>(Wt::WString::fromUTF8(label))
        ->addStyleClass("form-label d-block");
    f.combo = row->addNew<Wt::WComboBox>();
    f.combo->addStyleClass("form-select");
    return f.combo;
}

void LoadForm::populate(const std::string& resource, const std::string& labelAttr,
                        Field& f, bool blank) {
    f.hasBlank = blank;
    Field* fp = &f;
    api_->fetchOptions(
        resource, labelAttr,
        [this, fp, blank](std::vector<Option> opts) {
            fp->options = std::move(opts);
            fp->combo->clear();
            if (blank) fp->combo->addItem("— select —");
            for (const auto& o : fp->options)
                fp->combo->addItem(Wt::WString::fromUTF8(o.label));
        },
        [this, resource](std::string e) {
            message_->setText(Wt::WString::fromUTF8("Could not load " + resource +
                                                    " options: " + e));
        });
}

std::string LoadForm::selectedId(const Field& f) const {
    int i = f.combo ? f.combo->currentIndex() : -1;
    if (f.hasBlank) {
        if (i <= 0) return {};
        i -= 1;
    }
    if (i < 0 || i >= static_cast<int>(f.options.size())) return {};
    return f.options[static_cast<size_t>(i)].id;
}

void LoadForm::submit() {
    LoadDraft d;
    d.dispatch_week_id = selectedId(dispatchWeek_);
    d.shipper_id = selectedId(shipper_);
    d.receiver_id = selectedId(receiver_);
    d.commodity_id = selectedId(commodity_);
    d.pickup_id = selectedId(pickup_);
    d.dropoff_id = selectedId(dropoff_);
    d.driver_id = selectedId(driver_);
    d.equipment_id = selectedId(equipment_);
    d.run_type_id = std::atoi(selectedId(runType_).c_str());
    d.load_status_id = std::atoi(selectedId(status_).c_str());
    d.rate = rate_->value();
    d.deadhead_miles = deadhead_->value();
    d.loaded_miles = loaded_->value();
    if (pickupDate_->date().isValid())
        d.pickup_date = pickupDate_->date().toString("yyyy-MM-dd").toUTF8();
    if (deliveryDate_->date().isValid())
        d.delivery_date = deliveryDate_->date().toString("yyyy-MM-dd").toUTF8();

    // Validate required fields + the distinct pickup/drop-off invariant.
    if (d.dispatch_week_id.empty() || d.shipper_id.empty() ||
        d.receiver_id.empty() || d.commodity_id.empty() || d.pickup_id.empty() ||
        d.dropoff_id.empty() || d.run_type_id == 0 || d.load_status_id == 0) {
        message_->setText("Please complete all required fields.");
        return;
    }
    if (d.pickup_id == d.dropoff_id) {
        message_->setText("Pickup and drop-off locations must differ.");
        return;
    }

    message_->setText("Creating…");
    api_->createLoad(
        d,
        [this](Load l) {
            message_->setText(Wt::WString::fromUTF8("Created load " + l.id));
            if (onCreated_) onCreated_();
        },
        [this](std::string e) {
            message_->setText(Wt::WString::fromUTF8("Error: " + e));
        });
}

}  // namespace fd

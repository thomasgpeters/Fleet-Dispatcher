// Load intake form (control console): create a new load and assign a driver +
// equipment, posting to the JSON:API. Combos are populated from reference data.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include <Wt/WContainerWidget.h>

#include "ApiClient.h"

namespace Wt {
class WComboBox;
class WDateEdit;
class WDoubleSpinBox;
class WText;
}  // namespace Wt

namespace fd {

class LoadForm : public Wt::WContainerWidget {
public:
    using OnCreated = std::function<void()>;
    LoadForm(ApiClient* api, OnCreated onCreated);

private:
    // A combo bound to a set of reference Options (with an optional blank row).
    struct Field {
        Wt::WComboBox* combo = nullptr;
        std::vector<Option> options;
        bool hasBlank = false;
    };

    ApiClient* api_;
    OnCreated onCreated_;

    Field dispatchWeek_, shipper_, receiver_, commodity_, pickup_, dropoff_;
    Field driver_, equipment_, runType_, status_;
    Wt::WDoubleSpinBox* rate_ = nullptr;
    Wt::WDoubleSpinBox* deadhead_ = nullptr;
    Wt::WDoubleSpinBox* loaded_ = nullptr;
    Wt::WDateEdit* pickupDate_ = nullptr;
    Wt::WDateEdit* deliveryDate_ = nullptr;
    Wt::WText* message_ = nullptr;

    Wt::WComboBox* addCombo(const std::string& label, Field& f);
    void populate(const std::string& resource, const std::string& labelAttr,
                  Field& f, bool blank);
    std::string selectedId(const Field& f) const;
    void submit();
};

}  // namespace fd

// Plain data carriers for the dispatcher board, parsed from the JSON:API.
#pragma once

#include <string>

namespace fd {

struct Driver {
    std::string id;
    std::string name;
    int driver_type_id = 0;
    bool active = true;
};

struct Load {
    std::string id;
    std::string driver_id;        // may be empty (unassigned)
    std::string dispatch_week_id;
    std::string pickup_date;      // "YYYY-MM-DD" (may be empty)
    std::string delivery_date;    // "YYYY-MM-DD" (may be empty)
    std::string currency = "USD";
    int run_type_id = 0;
    int load_status_id = 0;
    double rate = 0.0;
    double deadhead_miles = 0.0;
    double loaded_miles = 0.0;
};

struct Position {
    std::string id;
    std::string equipment_id;
    std::string driver_id;
    std::string recorded_at;
    int location_source_id = 0;
    double lat = 0.0;
    double lng = 0.0;
    double speed_mph = 0.0;
};

}  // namespace fd

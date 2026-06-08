// Async JSON:API client for the dispatcher desktop, built on Wt::Http::Client
// and Wt::Json. Read-only fetches for the board (Driver, Load). The desktop
// never touches PostgreSQL — only the shared ApiLogicServer JSON:API.
//
// NOTE: not compiled in the dev sandbox (no Wt). Two spots are Wt-version
// sensitive and flagged in ApiClient.cpp: the Http done() error_code type and
// the Json::Type enum spelling.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "models.h"

namespace fd {

// A selectable reference option (id + display label) for form combos.
struct Option {
    std::string id;     // JSON:API id (string, even for integer-keyed lookups)
    std::string label;
};

// Field values for creating a load. FK ids are strings; lookups are ints.
struct LoadDraft {
    std::string dispatch_week_id;
    std::string shipper_id;
    std::string receiver_id;
    std::string commodity_id;
    std::string pickup_id;
    std::string dropoff_id;
    std::string driver_id;     // optional (assignment)
    std::string equipment_id;  // optional (assignment)
    int run_type_id = 0;
    int load_status_id = 0;
    double rate = 0.0;
    double deadhead_miles = 0.0;
    double loaded_miles = 0.0;
    std::string pickup_date;    // "YYYY-MM-DD" or empty
    std::string delivery_date;  // "YYYY-MM-DD" or empty
};

class ApiClient {
public:
    using DriversCallback = std::function<void(std::vector<Driver>)>;
    using LoadsCallback = std::function<void(std::vector<Load>)>;
    using PositionsCallback = std::function<void(std::vector<Position>)>;
    using OptionsCallback = std::function<void(std::vector<Option>)>;
    using LoadCallback = std::function<void(Load)>;
    using ErrorCallback = std::function<void(std::string)>;

    // baseUrl e.g. "http://localhost:5659/api" (no trailing slash).
    explicit ApiClient(std::string baseUrl);

    void fetchDrivers(DriversCallback onOk, ErrorCallback onErr);
    void fetchLoads(LoadsCallback onOk, ErrorCallback onErr);
    void fetchPositions(PositionsCallback onOk, ErrorCallback onErr);

    // Generic option fetch for form combos: GET /resource, label from labelAttr.
    void fetchOptions(const std::string& resource, const std::string& labelAttr,
                      OptionsCallback onOk, ErrorCallback onErr);

    // Create a load (POST /Load) and return the created resource.
    void createLoad(const LoadDraft& draft, LoadCallback onOk, ErrorCallback onErr);

    // Persist a HUD command (POST /HudCommand) for remote/distributed HUDs.
    // Best-effort; errors go to onErr (ignore for fire-and-forget).
    void postHudCommand(const std::string& commandType, const std::string& arg,
                        ErrorCallback onErr);

    const std::string& baseUrl() const { return baseUrl_; }

    // Reads FLEET_API_BASE_URL, default http://localhost:5659/api.
    static std::string baseUrlFromEnv();

private:
    std::string baseUrl_;
};

}  // namespace fd

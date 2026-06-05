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

class ApiClient {
public:
    using DriversCallback = std::function<void(std::vector<Driver>)>;
    using LoadsCallback = std::function<void(std::vector<Load>)>;
    using ErrorCallback = std::function<void(std::string)>;

    // baseUrl e.g. "http://localhost:5656/api" (no trailing slash).
    explicit ApiClient(std::string baseUrl);

    void fetchDrivers(DriversCallback onOk, ErrorCallback onErr);
    void fetchLoads(LoadsCallback onOk, ErrorCallback onErr);

    const std::string& baseUrl() const { return baseUrl_; }

    // Reads FLEET_API_BASE_URL, default http://localhost:5656/api.
    static std::string baseUrlFromEnv();

private:
    std::string baseUrl_;
};

}  // namespace fd

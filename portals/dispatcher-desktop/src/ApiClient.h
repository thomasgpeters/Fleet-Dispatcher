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

// Editable profile fields (PATCH /AppUser/{id}).
struct ProfileEdit {
    std::string full_name;
    std::string email;
    std::string phone;
    std::string title;
    std::string timezone;
};

class ApiClient {
public:
    using DriversCallback = std::function<void(std::vector<Driver>)>;
    using LoadsCallback = std::function<void(std::vector<Load>)>;
    using PositionsCallback = std::function<void(std::vector<Position>)>;
    using OptionsCallback = std::function<void(std::vector<Option>)>;
    using LoadCallback = std::function<void(Load)>;
    using ChannelsCallback = std::function<void(std::vector<Channel>)>;
    using MessagesCallback = std::function<void(std::vector<Message>)>;
    using MessageCallback = std::function<void(Message)>;
    using ChannelMembersCallback = std::function<void(std::vector<ChannelMember>)>;
    using TopicsCallback = std::function<void(std::vector<Topic>)>;
    using TopicCallback = std::function<void(Topic)>;
    using RawCallback = std::function<void(std::string)>;  // raw JSON:API doc text
    using TokenCallback = std::function<void(std::string)>;
    using UserCallback = std::function<void(AppUser)>;
    using ErrorCallback = std::function<void(std::string)>;

    // baseUrl e.g. "http://localhost:5659/api" (no trailing slash).
    explicit ApiClient(std::string baseUrl);

    // --- Authentication (ALS built-in JWT) -------------------------------
    // The bearer token (set after login) is sent on every subsequent request.
    void setAuthToken(std::string token) { authToken_ = std::move(token); }
    void clearAuthToken() { authToken_.clear(); }
    bool hasAuthToken() const { return !authToken_.empty(); }

    // POST /api/auth/login {username,password} -> access token.
    void login(const std::string& username, const std::string& password,
               TokenCallback onOk, ErrorCallback onErr);

    // Load the signed-in user's profile (GET /AppUser?filter[username]=).
    void fetchUserByUsername(const std::string& username, UserCallback onOk,
                             ErrorCallback onErr);

    // Update editable profile fields (PATCH /AppUser/{id}).
    void updateUser(const std::string& id, const ProfileEdit& edit,
                    UserCallback onOk, ErrorCallback onErr);

    void fetchDrivers(DriversCallback onOk, ErrorCallback onErr);
    void fetchLoads(LoadsCallback onOk, ErrorCallback onErr);
    void fetchPositions(PositionsCallback onOk, ErrorCallback onErr);

    // --- Communications (right panel) ---
    void fetchChannels(ChannelsCallback onOk, ErrorCallback onErr);
    void fetchMessages(const std::string& channelId, MessagesCallback onOk,
                       ErrorCallback onErr);
    // Members of a channel (role + standing) — drives composer gating (P1/P2).
    void fetchChannelMembers(const std::string& channelId,
                             ChannelMembersCallback onOk, ErrorCallback onErr);
    // The current user's memberships across channels — drives directory badges
    // (role/standing) + per-channel read state (last_read_at).
    void fetchMyMemberships(const std::string& userId,
                            ChannelMembersCallback onOk, ErrorCallback onErr);
    // Stamp a membership's last_read_at = now (mark a channel read).
    void markChannelRead(const std::string& membershipId, ErrorCallback onErr);
    // Forum topics in a channel (P3); create restricted to admins/dispatchers
    // server-side (LogicBank) — the UI also gates the action.
    void fetchTopics(const std::string& channelId, TopicsCallback onOk,
                     ErrorCallback onErr);
    void createTopic(const std::string& channelId, const std::string& name,
                     const std::string& createdBy, TopicCallback onOk,
                     ErrorCallback onErr);
    // Post a message; topicId empty = the channel's General stream.
    void createMessage(const std::string& channelId, const std::string& authorId,
                       const std::string& body, const std::string& topicId,
                       MessageCallback onOk, ErrorCallback onErr);

    // Generic option fetch for form combos: GET /resource, label from labelAttr.
    void fetchOptions(const std::string& resource, const std::string& labelAttr,
                      OptionsCallback onOk, ErrorCallback onErr);

    // Raw GET of a JSON:API path (e.g. "Message?filter%5Bchannel_id%5D=<id>");
    // delivers the response body verbatim. Used by the per-board export, which
    // bundles the unparsed documents for restore fidelity.
    void fetchRaw(const std::string& path, RawCallback onOk, ErrorCallback onErr);

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
    std::string authToken_;   // bearer token after login (empty = unauthenticated)
};

}  // namespace fd

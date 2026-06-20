// Plain data carriers for the dispatcher board, parsed from the JSON:API.
#pragma once

#include <string>

namespace fd {

// The signed-in user (from app_user), with editable profile fields.
struct AppUser {
    std::string id;
    std::string username;
    std::string full_name;
    std::string email;
    std::string phone;
    std::string title;
    std::string timezone;
    int app_role_id = 0;            // 1 dispatcher · 2 driver · 3 updater
    std::string avatar_document_id;
};

struct Driver {
    std::string id;
    std::string name;
    int driver_type_id = 0;
    bool active = true;
};

// Messaging (right-panel communications).
struct Channel {
    std::string id;
    std::string name;
    int channel_type_id = 0;   // 1=direct, 2=group, 3=broadcast
};

// A forum topic (thread) within a channel (Feature 4 P3).
struct Topic {
    std::string id;
    std::string channel_id;
    std::string name;
    bool is_closed = false;
};

// A user's membership in a channel: role + standing (Feature 4 P1/P2).
struct ChannelMember {
    std::string id;
    std::string channel_id;
    std::string user_id;
    int member_role_id = 0;     // 1=owner, 2=member, 3=admin
    int member_status_id = 1;   // 1=active, 2=muted, 3=banned
    std::string restricted_until;  // ISO8601 mute/ban expiry (empty = none/indefinite)
};

struct Message {
    std::string id;
    std::string channel_id;
    std::string topic_id;    // empty = General stream
    std::string author_id;
    std::string body;
    std::string posted_at;   // ISO8601
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

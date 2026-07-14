// Shared colour + person-avatar helpers for the console (board · fleet ·
// profile). People are tinted by the curated avatar palette; vehicles by their
// trailer type. Palette + trailer-type ids are STABLE seed lookups
// (database/seed_data.sql), so we map them here the same way statusName() /
// driverType() hardcode their lookups — no extra API round-trip. Keep these in
// sync with the `avatar_color` and `trailer_type.color_hex` seed.
#pragma once

#include <cctype>
#include <string>

namespace fd {

// avatar_color.hex by id (1..10) — a natural skin-tone spectrum (light → deep).
// 0/unknown → "" (caller derives a fallback). Keep in sync with the avatar_color
// seed (database/seed_data.sql).
inline std::string avatarHex(int colorId) {
    static const char* P[11] = {
        "",
        "#f7d8c0", "#efc6a6", "#e6b48f", "#d9a074", "#c88a5a",   // 1..5 ivory→tan
        "#b47444", "#9a5f37", "#7d4a2b", "#5b3420", "#3c2216"};  // 6..10 honey→espresso
    return (colorId >= 1 && colorId <= 10) ? std::string(P[colorId]) : std::string();
}

// Legible initials colour for a tone: dark ink on light tones, white on deep
// ones (by perceived luminance). hex is "#rrggbb".
inline std::string contrastText(const std::string& hex) {
    if (hex.size() < 7) return "#ffffff";
    auto hx = [&](int i) { return std::stoi(hex.substr(i, 2), nullptr, 16); };
    const double lum = 0.299 * hx(1) + 0.587 * hx(3) + 0.114 * hx(5);  // 0..255
    return (lum > 150.0) ? std::string("#2a1a0c") : std::string("#ffffff");
}

// Deterministic palette id (1..10) from a name (FNV-1a) — every person always
// has a colour even without one assigned (mirrors the driver.avatar_color →
// derived-from-name precedence documented in the schema).
inline int derivedColorId(const std::string& s) {
    unsigned h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return static_cast<int>(h % 10u) + 1;
}

// Resolve a person's colour: an explicit palette id wins; else derive from name.
inline std::string personHex(int colorId, const std::string& name) {
    std::string hex = avatarHex(colorId);
    return hex.empty() ? avatarHex(derivedColorId(name)) : hex;
}

// trailer_type.color_hex by id (matches seed). Unknown → steel.
inline std::string trailerHex(int trailerTypeId) {
    switch (trailerTypeId) {
        case 1: return "#3b82c4";  // step-deck
        case 2: return "#8a5cf6";  // RGN low-boy
        case 3: return "#3fa66a";  // flatbed
        case 4: return "#e07b39";  // car carrier
        default: return "#6b7a90"; // none / unknown
    }
}

// Up to two uppercase initials ("Pat Diesel" → "PD").
inline std::string initials(const std::string& name) {
    std::string out;
    bool atWordStart = true;
    for (char c : name) {
        if (c == ' ') { atWordStart = true; continue; }
        if (atWordStart && out.size() < 2)
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        atWordStart = false;
    }
    if (out.empty() && !name.empty())
        out += static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    return out;
}

// A tiny role glyph shown as a corner badge on the avatar (empty = none).
// app_role: 1 dispatcher · 2 driver · 3 updater.
inline std::string roleBadge(int appRoleId) {
    switch (appRoleId) {
        case 1: return "\xF0\x9F\x8E\xA7";  // 🎧 dispatcher
        case 2: return "\xF0\x9F\x9A\x9B";  // 🚛 driver
        case 3: return "\xE2\x9C\x8E";      // ✎ updater
        default: return "";
    }
}

// Inline HTML for a circular skin-tone avatar: initials (dark or white per the
// tone's luminance) on the person's tone, with an optional role badge. Safe to
// embed via WText / WString::fromUTF8 (raw HTML, like the rest of the board
// markup). `roleId`=0 omits the badge (fleet/board rows, where everyone's a driver).
inline std::string personAvatar(const std::string& name, int colorId,
                                int roleId = 0) {
    const std::string hex = personHex(colorId, name);
    const std::string ink = contrastText(hex);
    std::string badge;
    if (!roleBadge(roleId).empty())
        badge = "<span class=\"fd-avatar-badge\">" + roleBadge(roleId) + "</span>";
    return "<span class=\"fd-avatar\" style=\"background:" + hex + ";color:" + ink +
           "\"><span class=\"fd-avatar-txt\">" + initials(name) + "</span>" +
           badge + "</span>";
}

}  // namespace fd

# Fleet Dispatcher — TODO / Plan of approach

Working plan, sequenced. Check items off as they ship and add a dated entry to
[`DEVELOPMENT_LOG.md`](DEVELOPMENT_LOG.md). `[~]` = in progress, `[ ]` = queued,
`[x]` = done.

## Conventions (how we work)

- Domain language lives in [`domain-model.md`](domain-model.md); the schema
  (`database/schema.sql`) is the physical model ALS generates the API from.
- Lookup tables = sequential integer keys; domain objects = UUID/GUID keys;
  relationships = real FK constraints (ALS derives JSON:API relationships).
- Middleware is generated/run outside this repo (see
  [`MIDDLEWARE_SETUP.md`](MIDDLEWARE_SETUP.md)).
- Mobile is published to `Fleet-Dispatcher-Mobile` via `make publish-mobile`.
- Validate schema/seed against PostgreSQL 16; build the mobile app (`tsc` +
  Vite) before every push.
- **Desktop C++/Wt is not compiled in the dev sandbox (no Wt); it is built and
  run on the Linux box.** Standing decision — version-sensitive lines are flagged
  inline; no per-change "not compiled" disclaimer.

## Foundation  ✅

- [x] Three-tier scaffold, docs, architecture (+ SVG)
- [x] Schema: lookup tables + GUID domain keys + FK relationships
- [x] Desktop (C++/Wt) Bootstrap theme + full Wt `resources/` deployment
- [x] Mobile: self-contained, buildable TS app (React/Ionic/Capacitor/Vite)
- [x] Mobile publishing via git subtree (`make publish-mobile`, CMake target)
- [x] Middleware setup offloaded to `docs/MIDDLEWARE_SETUP.md`

## Feature 1 — Messaging & Content (CMS)  — Phase 1 CLOSED (2026-06-02)

Mobile messaging + CMS vertical is complete for what doesn't depend on auth /
realtime / the desktop portal. Those are carried over below to their owners.

Done:
- [x] Schema: `channel`, `channel_member`, `message`; CMS `document` + FK link
      tables (`message_document`, `load_document`)
- [x] Mobile: list → detail navigation (channels list + channel detail) with
      native transitions and back buttons (KISS); same pattern for Loads
- [x] Mobile: compose & send a message (write-side)
- [x] Mobile: attachment upload (create `document` + `message_document` link)
- [x] Mobile: attachment preview / download (bytea ↔ base64, data URL)
- [x] Optimize attachment loading (sparse fieldsets; lazy `data` on tap)
- [x] Unread counts via `channel_member.last_read_at` + mark-as-read (Badge)
- [x] Pull-to-refresh (`IonRefresher`) on channel list + detail

Phase 2 — pins & personal archive (2026-06-11):
- [x] Schema: `pin_scope` lookup + `message_pin` (user-selectable scope:
      self/channel/everyone) + `saved_message` (per-user archive, optional note).
      Verified on PostgreSQL 16 (40 tables in `fleet`).
- [x] Mobile: pin a message with a scope picker; visible-pins strip atop the
      channel; swipe to pin/unpin or save/unsave; inline pin/saved markers
- [x] Mobile: personal **Saved** archive view (cross-channel), reached from the
      Message Board header; remove items
- [x] **Emoji** in the composer: dependency-free picker (trucking-relevant set);
      storage/transport already UTF-8 (verified round-trip), so no schema change
- [x] **Reply to message**: swipe → Reply, composer quote banner, threaded quote
      snippet in the timeline (uses existing `message.reply_to_id`; no schema change)
- [ ] Emoji in the desktop messaging view (Wt WText/WLineEdit are UTF-8 native) →
      lands with that view
- [ ] Server-side pin-visibility enforcement (currently filtered client-side) →
      auth / LogicBank
- [ ] Desktop (dispatcher) pins & saved views → desktop portal work

Carried over (blocked / owned elsewhere):
- [ ] Direct (1:1) channel creation + membership management — needs a user
      directory (→ auth)
- [x] Current-user identity — replaced `src/currentUser.ts` placeholder with the
      authenticated user via `useAuth()` (Feature: Authentication)
- [ ] Clickable message **toasts** (`IonToast`, 2–3s, deep-link) → realtime
- [ ] Realtime delivery (websockets/push) — currently polling/pull-to-refresh
- [ ] Desktop (dispatcher) messaging view → desktop portal work
- [ ] Server-side `unread_count` (LogicBank) to replace client-side computation
- [ ] More CMS links as needed: `driver_document`, `equipment_document`,
      `inspection_document`

## Dispatcher Desktop Portal (C++/Wt)  — NEXT

Pivot here after Phase 1. Talks to the same JSON:API; Bootstrap theme + Wt
`resources/` deploy already wired. Design: [`DISPATCHER_DESKTOP.md`](DISPATCHER_DESKTOP.md).

- [x] Schema: `load.pickup_date` / `delivery_date` (drives Today vs Week)
- [x] **Shell**: app bar (API/health), nav (Board/Loads/Drivers/Messages),
      command bar, content outlet; owns the JSON:API client + mode state
      (not compiled in-sandbox — no Wt; see note below)
- [x] **Board** with a **Today | Week** toggle (today's runs vs Mon→Mon grid),
      fed by an async `ApiClient` (Wt::Http::Client + Wt::Json)
- [x] App-bar **user + role + Sign out** (auth); console gated by a Wt login
- [x] **App frame + design system** ([`DESIGN_SYSTEM.md`](DESIGN_SYSTEM.md)):
      full-width header (nav) + collapsible left/right panels + center work panel
      + full-width footer; subtle-blue/orange light + dark themes (toggle); HUD
      forced dark. Shared with mobile (Ionic vars) — same palette across clients
- [ ] Add app-bar week selector + health indicator
- [x] **Left menu** (Board · New Load · Drivers · Communications) + work-panel
      toggles (Today|Week, Compact); **right Communications rail** (`CommPanel`:
      channels + live conversation + composer); **toasts** (`Toaster`, top-right);
      panel toggles as disclosure arrows (▼/▶/◀); Communications menu item takes
      the full work area
- [x] **Views**: Fleet (drivers + equipment list), **Map** (geo-positioning of
      fleet locations, Leaflet + table), Settings (appearance + info) — center
      swaps per menu item
- [x] **Comms push/WebSockets**: `CommBus` + Wt server push delivers sent
      messages instantly; `wt_config.xml` enables WebSockets; 30 s reconcile poll
      bridges off-server messages
- [~] **Cross-client realtime** via a **WebSocket bridge over ALS → Kafka**
      (`realtime/`, [`REALTIME.md`](REALTIME.md)). DONE: bridge service
      (Kafka consumer → WS, JWT, reconnect) + mobile client (live messages/unread,
      reconcile fallback); topic-per-type + channel_id correlation id (key for
      ordering); ALS producer snippet + post-generate install automation in
      `als-extensions/` (`make als-extensions`); **desktop `RealtimeClient`**
      (Boost.Beast, opt-in `-DFD_REALTIME_CLIENT=ON`) → `CommBus` with id de-dup.
      config-driven multi-topic routing (`DEFAULT_ROUTES` / `KAFKA_TOPIC_ROUTES`)
      so new purposes (loads/trips/alerts…) are config, not code. Realtime data
      plane: mobile applies message events directly; desktop `PositionBus` drives
      live HUD/Map fleet locations. REMAINING: enable `KAFKA_CONNECT` in ALS;
      build/verify the desktop client on Linux
- [ ] Wire left-panel filters + a right-panel selected-load inspector to data
- [x] Load intake form (new load) + driver/equipment assignment (POST /Load)
- [x] **HUD** surface (`/hud`) + `HudControlBus` (Wt server push): the console's
      Today/Week toggle publishes `SetMode`; HUD sessions auto-switch
- [x] **Distributed HUD:** schema `hud_command` (+ POST from the console) so a
      remote HUD can be driven; `command_type`/`arg` event log
- [ ] Remote HUD: subscribe to `hud_command` (poll/stream) when run off-server
- [ ] Extend the command bus to more commands (`FocusDriver`, `HighlightLoad`)
      (enum + HUD handlers stubbed)
- [ ] Dispatcher messaging view (mirrors mobile Feature 1)
- [x] **HUD truck-locations panel**: latest position per rig (15s poll), fed by
      `position_report` (Feature 2)
- [ ] **HUD map tiles** (Leaflet/HERE) layered over the positions data

> Wt isn't installable in the dev sandbox, so the C++ above is **not compiled
> here**. Version-sensitive spots are flagged in `ApiClient.cpp` (Http `done()`
> error_code type; `Json::Type::Null` enum) and `main.cpp`/theme (`WBootstrap5Theme`
> needs Wt ≥ 4.5).

## Feature 2 — Truck Location & Dispatcher HUD

Powers the HUD's map. See "Planned" in `domain-model.md`.
**Decided (2026-06-02):** PostGIS spatial data is separated from the ALS APIs
(see [`SPATIAL_GIS_DATA_CONSIDERATIONS.md`](SPATIAL_GIS_DATA_CONSIDERATIONS.md)).

- [x] Schema: `location_source` lookup (`airtag`, `google_device`,
      `phone_push`) + `position_report` time series — **lat/lng numeric on ALS
      tables** (no geometry). Verified on PostgreSQL 16.
- [x] Mobile: driver phone-push location (Locate tab → POST `/PositionReport`)
- [x] Desktop HUD: truck-locations panel (latest per rig, 15s poll)
- [x] Schema: navigation — `trip`, `waypoint`, `point_of_interest`, `route` +
      lookups (`trip_status`, `stop_type`, `poi_category`), lat/lng. Verified.
- [x] Mobile Trips: list, start trip; **mutable per-trip route** — trip overview
      drills into an Edit-waypoints page to add (fuel/lunch/load-stop/…), remove,
      or **drag-reorder** stops; stop types `lunch`+`load_stop`; waypoint edits
      stream live. **Auto route recompute** in `geospatial/` (Kafka
      `waypoint`/`trip` consumer → upsert `route`; haversine now, HERE pluggable).
      **Manual edits set `trip.route_locked`** so the optimizer won't auto-reorder;
      a driver can **Unlock & re-optimize** (confirm dialog) to clear the lock and
      trigger immediate recompute/re-optimization via the trip event.
- [~] Trips: start/stop lifecycle, turn-by-turn navigate, POIs pending
- [~] **AI route optimization** (single + multi pickup/drop-off, shared-trailer
      capacity). INTERIM DONE: nearest-neighbor optimizer (`geospatial/optimize.py`,
      no API key) runs on recompute for non-`route_locked` trips — origin/dest
      anchored, distance-greedy, persists new seq. FOUNDATION: two capacity
      dimensions — `equipment.deck_length_ft` + `equipment.weight_capacity_lbs`
      (per tractor/trailer config) and `load.deck_feet` + `load.weight_lbs`.
      DEFERRED (user researching the engine): the full **capacitated
      pickup-and-delivery** solve (pickup-before-dropoff + both capacity
      dimensions) — OR-Tools (self-hosted) vs HERE Tour Planning vs LLM-as-caller.
      It replaces the NN heuristic and still skips `route_locked` trips.
- [x] `gis` bootstrap SQL (`database/gis_bootstrap.sql`): PostGIS into `gis` +
      derived geography views; **verified** `public` stays clean (ALS-safe).
      Full standup in [`DEPLOYMENT.md`](DEPLOYMENT.md).
- [x] Separate geospatial endpoint (FastAPI, `geospatial/`): /health,
      /truck-stops/nearest, /trucks/near, /positions/latest — **verified** live
      against PG16+PostGIS as `fleet_gis`. (view vs. mirror: using views for now)
- [ ] Ingestion adapters for AirTag / Google sources (phone-push done)
- [ ] HERE routing/maps integration (truck-legal routes, bridge heights, truck
      stops); persist as `route.polyline`
- [x] Desktop HUD **map** (`Wt::WLeafletMap`, OSM tiles) with a marker per rig's
      latest position (15s refresh)
- [ ] HUD map: overlay routes (`route.polyline`) and driver-focus/load-highlight
      commands

## Feature 3 — Hey Dispatch driver voice assistant

Hands-free assistant: driver says "Hey Dispatch", push-to-talk, on-device speech.
A FastAPI service (`assistant/`) calls a **pluggable AI provider** with tool use.
Design: [`AI_ASSISTANT.md`](AI_ASSISTANT.md).
**Decided (2026-06-11):** push-to-talk first; STT/TTS on-device (Web Speech API);
AI provider is admin-selectable (Anthropic default / OpenAI / Ollama).

- [x] Service scaffold (`assistant/`): FastAPI `/assist` + `/health`, config,
      Dockerfile, README
- [x] Tool schemas + handlers: `send_message_to_dispatcher` (→ ALS JSON:API),
      `get_eta`, `validate_route`, `alternate_routes` (→ HERE, with fallbacks),
      `tech_help` (built-in KB)
- [x] **Pluggable AI provider** (`app/providers/`): Anthropic (default; adaptive
      thinking, effort, prompt caching) + OpenAI-compatible adapter (OpenAI /
      Ollama via base URL); selected by `ASSISTANT_PROVIDER`, lazy SDK imports
- [x] Mobile: "Dispatch" tab — push-to-talk (Web Speech STT → `/assist` → TTS),
      pulls dispatcher channel + active-trip destination for context
- [ ] Wake-word ("Hey Dispatch") hands-free activation (push-to-talk shipped)
- [ ] Real tech-help knowledge base / retrieval (replace placeholder KB)
- [ ] Per-driver truck profile (height/weight) feeding route tools — needs auth
- [ ] Live end-to-end test against a configured provider + HERE key

## Cross-cutting

- [x] **DB schema separation (DECIDED + done):** shared instance with
      Smitty/Student-Onboarding → all Fleet app tables in the **`fleet`** schema
      (owned by `fleet`, ALS reflects it); PostGIS in shared **`gis`**
      (Fleet's `fleet_*` views owned by `fleet_gis`). See `MIDDLEWARE_SETUP.md`
      + `DEPLOYMENT.md`. Verified: 40 tables in `fleet`, 0 in `public`.
- [~] **AuthN/AuthZ** (roles: dispatcher, driver, updater). DONE: ALS built-in
      JWT auth against `app_user` (password_hash + profile fields + avatar FK);
      mobile login/profile/logout, bearer header on all calls, 401 auto-logout,
      `currentUser.ts` placeholder replaced by `useAuth()`; **desktop (Wt) console
      login + profile** (LoginView/ProfileView, token on all ApiClient calls,
      sign out). See [`AUTHENTICATION.md`](AUTHENTICATION.md). REMAINING:
      role-based grants (LogicBank/ALS security), desktop **avatar upload** + HUD
      service token, native secure token storage
- [ ] LogicBank rules in generated middleware (weekly cap, settlement math)
- [ ] Seed data growth for demos

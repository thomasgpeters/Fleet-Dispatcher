# Fleet Dispatcher — Development Log

Newest first. One entry per meaningful change set; pair with the checklist in
[`TODO.md`](TODO.md).

## 2026-06-11

### ALS Kafka producer snippet + post-generate install automation
- New `als-extensions/`: customizations to layer onto the generated ALS project.
  `logic_discovery/fleet_events.py` emits a Kafka event per Message/PositionReport
  insert (`Rule.after_flush_row_event` + `kafka_producer.send_kafka_message`, key =
  channel_id/equipment_id) — the producer side of the realtime bridge. Verified
  the ALS API against GenAI-Logic source (`send_kafka_message` signature).
- Automation: it installs into `logic/logic_discovery/`, which ALS **auto-discovers**
  and a `rebuild` **preserves** — so it survives rebuilds; a fresh `create` just
  needs `make als-extensions` (idempotent `install.sh`, target ALS_PROJECT).
  Documented chaining it onto the generate step. Tested the installer against a
  fake ALS project (copy + idempotent + rejects non-ALS dirs).
- Docs: als-extensions/README, MIDDLEWARE_SETUP (post-generate step), REALTIME
  (producer points here), CLAUDE golden rule + component table.

### Multi-topic ready: config-driven routing for new purposes
- Bridge routing is now **config-driven** (`realtime/app/config.py` →
  `DEFAULT_ROUTES`, overridable via `KAFKA_TOPIC_ROUTES` JSON env). Each topic maps
  to a `broadcast` key + optional `scopes` ({prefix, field}); `routing_keys()` is
  generic. **Unknown topics still work** (broadcast on their own name) so a
  producer can ship ahead of a route entry — no fan-out code change to add a
  purpose. Subscribed topics derive from the route map (or `KAFKA_TOPICS`).
- ALS producer made extensible: a `_PRODUCERS` list + `_emit()` helper; adding a
  type is one handler + one list entry (loads/trips/alerts… as the app evolves).
- Docs: REALTIME "Multiple topics, and adding one" guide; .env.example shows
  `KAFKA_TOPIC_ROUTES`. Bridge + snippet syntax-checked.

### Hide Kafka config — internal env files only
- Hardened `.gitignore`: ignore `.env` + `*.env`, keep only `*.env.example`
  placeholder templates (verified with `git check-ignore`). No real broker/secret
  is committed (audited: only `localhost:9092` / `change-me` placeholders).
- New `docs/REALTIME.md` → "Configuration & secrets": Kafka brokers/creds, the
  consumer group, and the JWT secret live ONLY in server-side env files
  (bridge `.env`, ALS project `.env`). **Clients never see Kafka** — they get only
  the bridge WebSocket URL + a JWT (the bridge is the encapsulation boundary).
- ALS producer config is env-driven (`KAFKA_CONNECT = os.getenv(...)`), not a
  hardcoded/committed broker; ALS project `.env` should be gitignored
  (als-extensions/README, MIDDLEWARE_SETUP updated).

### Desktop/HUD: realtime bridge WebSocket client → CommBus
- New `RealtimeClient` (`portals/dispatcher-desktop/src/`): one **server-wide**
  Boost.Beast WebSocket client that consumes the realtime bridge and publishes
  message events into `CommBus` (which already fans out to every session via Wt
  server push). Sync read loop on a worker thread with exponential-backoff
  reconnect; `stop()` closes the socket to unwind the blocking read.
- **Opt-in** via CMake `-DFD_REALTIME_CLIENT=ON` (needs Boost.Beast). Default OFF
  → Boost-less builds still work and the desktop falls back to intra-server
  `CommBus` + the 30 s reconcile poll. Configured by `FLEET_REALTIME_URL` +
  `FLEET_REALTIME_TOKEN` (a bridge service JWT); `main.cpp` uses start/wait/stop.
- `CommPanel` now **de-dups by message id** (a `std::set`), so a desktop user's
  own send (intra-`CommBus` + the bridge echo of the same row) renders once;
  toasts only for others.
- Not compiled in the sandbox; Beast client + cross-thread socket close are
  flagged for Linux build/verify. Positions→HUD/map is the next hook.

### Realtime: WebSocket bridge over ALS → Kafka
- New `realtime/` service (`docs/REALTIME.md`): a **confluent-kafka consumer →
  WebSocket relay**. ALS already produces domain events to Kafka
  (`Rule.after_flush_row_event` + mapper, per GenAI-Logic Sample-Integration);
  the bridge fans them out to clients. Stateless, JWT-authed (shared ALS secret),
  auto-reconnecting Kafka loop + WS heartbeats. Mirrors the `assistant/` layout
  (app/, requirements, Dockerfile, deploy/ systemd).
- **Topic strategy (decision):** one Kafka topic **per row type** (`message`,
  `position`) with **`channel_id` as a correlation id in the payload** and the
  Kafka **key = channel_id** for per-channel ordering — NOT a topic per channel
  (channels are dynamic UUID objects → topic explosion). Per-channel fan-out is
  done at the WebSocket layer (`channel:<id>` subscriptions).
- Mobile: `RealtimeClient` (exponential-backoff reconnect, re-subscribe on
  connect, status) + `RealtimeProvider`/`useRealtime`; wired into ChannelPage
  (live messages) and ChannelsPage (live unread). Accelerator only — initial
  load + pull-to-refresh remain the fallback. `npm run build` clean.
- Considered Postgres LISTEN/NOTIFY as the source but **reverted** it: ALS→Kafka
  is the canonical producer the team already uses; two sources = fragile.
- Robustness: DB/JSON:API stays canonical; Kafka durable; clients degrade to a
  reconcile poll if the bridge/Kafka is down.
- Desktop: still uses intra-server push (`CommBus` + Wt WebSockets); joining the
  external bridge via a WS client is the documented next step.

### Desktop: Fleet/Map/Settings views + comms WebSocket push
- Menu now: Board · Fleet · Map · New Load · Communications · Settings. The
  center work panel has no hide/show toggle — it swaps the visible view per menu
  item (left/right panels remain collapsible).
- `FleetView` — list of drivers (fetchDrivers) + equipment (unit numbers).
- `MapView` — geo-positioning of fleet locations: a Leaflet map (latest position
  per rig) + detail table, 15 s telemetry refresh (mirrors the HUD map).
- `SettingsView` — appearance (System/Light/Dark → data-fd-theme + localStorage,
  shared with the header toggle) + account/connection info.
- **Comms push/WebSockets**: new `CommBus` (in-process singleton like
  `HudControlBus`) broadcasts sent messages to every session via Wt server push;
  `CommPanel` now subscribes and renders pushed messages instantly (and toasts),
  publishes on send, and keeps only a 30 s reconcile poll for off-server (mobile)
  messages. New `wt_config.xml` enables `<web-sockets>true` for the push transport.
- Note: full cross-client realtime (mobile→desktop) needs the middleware to emit
  change events (SSE/WebSocket/broker) — backend follow-up (ALS is generated).
- **Not compiled in the sandbox**; new code reuses known-good Wt patterns
  (WLeafletMap from HudView, WServer::post bus from HudControlBus). CMake updated.

### Desktop dynamic shell: menu, communications rail, toasts
- Header: **logo top-left**; **profile (name) + logged-in role + Sign out
  top-right**, plus theme toggle and panel toggles. Panel toggles use the
  disclosure aesthetic — **▼ open**, **▶/◀ closed** (left/right).
- Left panel = **menuing system** (Board · New Load · Drivers · Communications)
  + **work-panel toggles** (Today | Week, Compact rows) that drive the center.
- Right panel = **Communications** (`CommPanel`): channel list + live conversation
  (10s `WTimer` poll) + composer. Selecting **Communications** in the menu makes
  comms take over the **full work area**.
- **Toaster** (`Toaster`): top-right transient alerts (new messages, save/create
  confirmations, errors). Single sweeper timer auto-dismisses; close marks for
  removal (no delete-during-callback). New-message toasts come from the rail only.
- ApiClient: `fetchChannels` / `fetchMessages` / `createMessage` (+ Channel,
  Message models). Server push (`enableUpdates`) already on → live feel.
- **Desktop not compiled in the sandbox**; new code uses plain Wt widgets +
  `WTimer` + `doJavaScript` (no version-sensitive APIs beyond those already
  flagged). Build on a Linux box to confirm.

### Shared design system across mobile, desktop, and HUD
- New `docs/DESIGN_SYSTEM.md` — one palette (subtle blues, sparse orange accent,
  white/light standard theme + complementary dark) as the source of truth,
  implemented as design tokens in each client. Default follows the OS
  (`prefers-color-scheme`); a manual override wins (persisted).
- Desktop (`docroot/css/fleet-dispatcher.css`): rewrote with `--fd-*` light/dark
  tokens, aligned Bootstrap primary to the brand blue, and added the **app frame**
  — full-width header (nav), three-column body (collapsible left panel · center
  work panel · collapsible right panel), full-width footer.
- Desktop `Shell` restructured into that frame: header with nav + theme toggle +
  panel show/hide toggles (`◧`/`◨`) + user·role + Sign out; left "Filters" panel;
  center command bar + content outlet; right "Details" inspector; footer
  (copyright + links). Active-nav gets the orange underline. Theme toggle flips
  `data-fd-theme` on `<html>` and persists to localStorage.
- HUD forces the dark theme (wall display) via `data-fd-theme="dark"`.
- Mobile: new `src/theme/variables.css` (Ionic vars from the same palette, light
  + dark) and `theme.ts` (System/Light/Dark, persisted, applied at startup);
  imported in `main.tsx`; **Appearance** picker added to Profile. Blue toolbars
  match the desktop nav. `npm run build` clean.
- **Desktop not compiled in the sandbox**; CSS is static, the Wt layout uses
  plain containers + `doJavaScript` for the theme toggle (no version-sensitive
  APIs added). Build on a Linux box to confirm.

### Desktop (Wt) console login + profile (same ALS JWT credentials)
- ApiClient: holds the bearer token (`setAuthToken`/`clearAuthToken`/`hasAuthToken`)
  and adds `Authorization: Bearer` to every GET/POST/PATCH; new `login()`
  (POST /api/auth/login via a plain-JSON helper), `fetchUserByUsername()`
  (GET /AppUser?filter[username]=), and `updateUser()` (PATCH /AppUser/{id} via a
  new patch helper). Added `parseUser`, `urlEncode`, `AppUser` model.
- New `LoginView` gates the console (DispatcherApp shows login → Shell on success;
  Shell's Sign out returns to login). New `ProfileView` (view/edit profile,
  PATCH). Shell now takes `(ApiClient*, AppUser, onLogout)`, shows name·role +
  Sign out in the app bar, and a Profile nav item. CMake gains the two sources.
- Token lives **server-side** in the Wt session (not in the browser). HUD entry
  point stays unauthenticated for now (wall display) — noted for a service token.
- **Not compiled in the sandbox** (no Wt). Version-sensitive spots flagged:
  `Http::Method::Patch` (older Wt may lack PATCH → PUT fallback) alongside the
  existing `done()` error_code / `Json::Type::Null` notes. Avatar upload deferred.

### Authentication + user profile (ALS built-in JWT, mobile-first)
- Schema: `app_user` gains `password_hash` (werkzeug pbkdf2:sha256) + profile
  fields (`phone`, `title`, `timezone`, `avatar_document_id` → CMS `document`,
  `last_login_at`). Avatar FK added after `document` (ordering). Seeded the three
  demo users with password `fleet123` (hashes verified) + titles/phones.
  **Verified** schema+seed on PostgreSQL 16; avatar FK present.
- Mobile: ALS JWT auth. New `auth/AuthContext` (`useAuth()` → user, driverId,
  equipmentId, token, login/logout), `api/auth.ts` login (POST /api/auth/login),
  bearer header on every client call + 401/403 auto-logout, `LoginPage` gate
  (shown until authed), `ProfilePage` (view/edit + avatar upload via CMS + sign
  out), profile button in the Loads header. Replaced the `currentUser.ts`
  identity placeholder with `useAuth()` across Channels/Channel/Saved/Locate/
  Trips/Assistant (only the `PHONE_PUSH_SOURCE_ID` lookup constant remains).
- Docs: new `AUTHENTICATION.md` (ALS add-auth + auth_provider sketch verifying
  the werkzeug hash, login endpoint, JWT secret, roles, token storage); TODO,
  domain-model. Token in localStorage for now (Capacitor Secure Storage = TODO).
- **Needs ALS regeneration** so `AppUser` exposes the new columns + the
  AppUser↔Document avatar relationship. Mobile `npm run build` clean.

### Feature 1 — reply-to-message (threaded quotes)
- Mobile: swipe a message → **Reply**; the composer shows a "Replying to …"
  banner with a one-line snippet of the original + cancel, and the sent message
  renders a quoted snippet (left-bar) above its body. Uses the existing
  `message.reply_to_id` self-reference (no schema change); `createMessage` gained
  an optional `replyToId`. Quotes resolve client-side from the loaded timeline
  (falls back to "quoted message" if the original isn't loaded).
- Verified: mobile `npm run build` clean.

### Feature 1 — emoji support in the messaging composer
- Mobile: a dependency-free `EmojiPicker` (curated set incl. trucking symbols:
  🚛 ⛽ 📍 🛣️ …) opened from a composer button (IonPopover); tapping appends to
  the draft. No emoji library — keeps the bundle lean; emojis are plain UTF-8
  text so display/storage need nothing special.
- Verified the storage/transport path round-trips a 4-byte emoji through
  `message.body` (TEXT) on a UTF8 PostgreSQL 16 cluster (24 chars / 32 bytes
  preserved); JSON:API passes UTF-8 through unchanged — no schema change.
- Desktop messaging view (not yet built) will get emoji for free: Wt's
  WText/WLineEdit are UTF-8 native.

### Feature 1 (Phase 2) — pinned messages + personal saved archive
- Schema: `pin_scope` lookup (`self`/`channel`/`everyone`) + `message_pin`
  (user-selectable visibility per pin; `channel_id` carried for direct filtering;
  one pin per (message, user)) + `saved_message` (per-user cross-channel archive
  with an optional note). +3 tables → **40** in `fleet`. Seeded a channel-scope
  pin and a saved message; **verified** schema+seed on PostgreSQL 16.
- Mobile: pin a message via a scope picker (Only me / Channel / Everyone), a
  "Pinned" strip atop the channel showing visible pins + scope, swipe actions to
  pin-unpin / save-unsave, and inline pin/saved markers. New personal **Saved**
  view (cross-channel archive) reached from the Message Board header.
- API client: `MessagePin`/`SavedMessage` types, `PIN_SCOPE` ids, pin/unpin/
  repin, visible-pins filter (self pins only for the pinner), save/unsave,
  single-message getter; added a JSON:API DELETE helper.
- Note: pin-visibility is filtered **client-side** for now — a server-side rule
  (auth/LogicBank) is the production approach. Mobile `npm run build` clean.

### Feature 3 — "Hey Dispatch" driver voice assistant (pluggable AI provider)
- New `assistant/` FastAPI service (port `5710`): `POST /assist` takes an
  on-device STT transcript + the driver's trusted context and returns a spoken
  reply + the actions it took; `GET /health` reports provider/model/config.
- Tool use (5 intents): `send_message_to_dispatcher` (→ ALS JSON:API `Message`),
  `get_eta`, `validate_route`, `alternate_routes` (→ HERE truck routing, with
  straight-line / "not configured" fallbacks), `tech_help` (built-in KB). Trusted
  context (driver, position, destination, truck profile) comes from the request,
  never the model.
- **Pluggable AI provider** (`app/providers/`): admin picks via
  `ASSISTANT_PROVIDER` — `anthropic` (default; adaptive thinking, effort,
  prompt-cached system+tools prefix), `openai`, or `ollama` (OpenAI-compatible
  adapter, local base URL). SDK imports are lazy, so only the chosen provider's
  package is loaded. The brain (`app/assistant.py`) owns the prompt + tool
  dispatch/action-recording and delegates the model loop to the provider.
- Mobile: new **Dispatch** tab (`AssistantPage`) — push-to-talk via Web Speech
  API (STT) → `/assist` → `speechSynthesis` (TTS); pulls the dispatcher channel +
  active-trip destination for context. New `VITE_ASSISTANT_BASE_URL`, assistant
  API client, and minimal `SpeechRecognition` typings.
- Verified: service `py_compile`s and imports without the AI SDKs (lazy);
  mobile `npm run build` clean. Docs: DEPLOYMENT (port table + step 8), TODO,
  new `AI_ASSISTANT.md`.

## 2026-06-02

### Shared-instance schema move + geospatial endpoint + one-shot (verified)
- Shared PostgreSQL instance (with Smitty / Student-Onboarding): moved all Fleet
  app objects out of `public` into a dedicated **`fleet`** schema (owned by
  `fleet`; ALS reflects it via `search_path = fleet, public`). `schema.sql` /
  `seed_data.sql` set the search_path; resource names unchanged (table-derived).
- PostGIS in a **shared `gis`** schema: Fleet owns only its `fleet_*` geography
  views via the new **`fleet_gis`** role (granted CREATE on gis, SELECT on the
  fleet lat/lng tables); extension objects stay superuser-owned (by design).
- `scripts/db-setup.sh` — one-shot for steps 1–4 (role+db, schema, seed, gis via
  admin since CREATE EXTENSION needs superuser).
- `geospatial/` — FastAPI endpoint (/health, /truck-stops/nearest, /trucks/near,
  /positions/latest) connecting as `fleet_gis`; Dockerfile + README.
- **Verified on PG16 + PostGIS 3.4:** one-shot creates `fleet` (37 tables) with
  `public` empty; `fleet_*` views owned by `fleet_gis`; the FastAPI endpoint
  served all four routes live (nearest truck stop, latest positions, trucks-near).
- Synced docs: DEPLOYMENT, MIDDLEWARE_SETUP (schema layout = DECIDED),
  SPATIAL_GIS_DATA_CONSIDERATIONS, TODO.

### Deployment runbook + PostGIS bootstrap (verified)
- `docs/DEPLOYMENT.md`: end-to-end standup — PostgreSQL, schema/seed, PostGIS
  (without breaking ALS), ALS create/run, geospatial-endpoint role, portals,
  verification + troubleshooting.
- `database/gis_bootstrap.sql`: installs PostGIS into a dedicated `gis` schema
  (+ `position_geog`/`poi_geog` geography views).
- Verified on PostgreSQL 16 + PostGIS 3.4: `public` ends with **0** PostGIS
  objects (so ALS reflection stays clean), `spatial_ref_sys` lives in `gis`, and
  spatial queries (nearest-POI KNN, geography distance) work with
  `search_path = gis, public`. docker-compose notes the `postgis/postgis` image.

### HUD: Leaflet map of truck positions
- HUD now renders a `Wt::WLeafletMap` (OSM tiles, no key) with a marker per
  rig's latest position, rebuilt on the 15s poll (markers tracked + removed/
  re-added); the detail table remains below. Leaflet JS/CSS via CDN (note for
  offline/self-contained deploy). Needs Wt ≥ 4.5.

### Feature 2: navigation schema + mobile Trips
- Schema: `trip`, `waypoint` (ordered, unique seq), `point_of_interest`
  (incl. truck stops via category), `route` (HERE polyline as text) + lookups
  `trip_status`/`stop_type`/`poi_category` — ALS-facing lat/lng, no PostGIS.
  Seed: a Lubbock→Denver trip with 3 waypoints, a truck-stop POI, a route.
  Verified on PostgreSQL 16 (ordered waypoints + unique-seq guard).
- Mobile: **Trips** tab — list driver trips, start a (planned) trip, trip
  detail with ordered waypoints and "add current location" (geolocation →
  addWaypoint). Client: tripsForDriver/getTrip/createTrip/waypointsForTrip/
  addWaypoint. `npm run build` passes.
- Pending: trip start/stop lifecycle, HERE routing/navigation, POIs browsing,
  HUD map tiles.

### Feature 2: truck-location backbone (telemetry)
- Schema: `location_source` lookup + `position_report` (lat/lng numeric,
  heading/speed/accuracy, recorded_at; equipment/driver FKs) per the geospatial
  decision — ALS-facing, no PostGIS. Seed: 3 sources + a short track. Verified on
  PostgreSQL 16 (latest-per-rig query + lat range CHECK).
- Mobile: **Locate** tab — `navigator.geolocation` → `api.postPosition` (POST
  `/PositionReport`, phone_push). Placeholder driver/equipment ids in
  `currentUser.ts` pending auth. `npm run build` passes.
- Desktop HUD: `ApiClient::fetchPositions` + a **truck-locations panel** (latest
  position per rig, refreshed every 15s via `WTimer`).
- PostGIS `gis` schema, the geospatial endpoint, HERE routing, and HUD map tiles
  remain queued.

### Dispatcher desktop: /hud surface + HudControlBus
- Schema: `hud_command` append-only command channel (free-text `command_type` +
  `arg`) for distributed/remote HUDs. Verified on PostgreSQL 16.
- C++/Wt: `HudControlBus` (server-push singleton: subscribe/unsubscribe/publish
  via `WServer::post`), `HudView` (read-only board at `/hud` that reacts to
  `SetMode`), and a second WServer entry point (`/hud`) alongside the console.
- The console's Today/Week toggle now publishes `SetMode` to the bus (instant,
  in-process) and records it via `ApiClient::postHudCommand` (POST /HudCommand)
  for remote HUDs. `FocusDriver`/`HighlightLoad` enum + handlers stubbed.
- README: HUD served at `/hud`.

### Dispatcher desktop: load intake form + assignment
- `LoadForm` (Wt): combos for dispatch week, shipper, receiver, commodity,
  pickup/drop-off locations, run type, status, plus optional driver + equipment
  (assignment); rate/miles spin boxes; pickup/delivery date edits. Validates
  required fields and distinct pickup/drop-off, then POSTs.
- `ApiClient`: `Option`/`LoadDraft`, `fetchOptions` (generic reference combos),
  and `createLoad` (POST /Load, hand-built JSON:API body via `postJson`).
- Shell: "New Load" nav → form; on success returns to the board (which reloads).
  CMake builds `LoadForm.cpp`.

### Dispatcher desktop: shell + Today/Week board (build start)
- Schema: added `load.pickup_date` / `delivery_date` (+ `delivery_after_pickup`
  CHECK, pickup-date index) to drive the board; seed updated. Verified on
  PostgreSQL 16. Synced the mobile `Load` type.
- C++/Wt: `Shell` (app bar, nav, Today|Week command bar, content outlet),
  `BoardView` (Today list vs Mon→Mon grid), and an async `ApiClient`
  (Wt::Http::Client + Wt::Json) reading `Driver`/`Load`. `main.cpp` enables
  server push; CMake builds the new sources.
- Decision: HUD will be **distributed-capable** — commands also persisted as a
  `hud_command` JSON:API resource (built with the bus, later).
- NOT compiled here (no Wt in the sandbox); version-sensitive lines flagged in
  `ApiClient.cpp` and the Bootstrap-5 theme.

### Dispatcher Desktop & HUD design
- Added `docs/DISPATCHER_DESKTOP.md`: the app **shell**, the **Today | Week**
  board modes, and the **control → command → HUD** mechanism — a Wt server-push
  `HudControlBus` where the control console publishes commands and HUD sessions
  auto-react (generalizable to `FocusDriver`, `HighlightLoad`, …). Notes the
  same-server (in-process bus) vs distributed (`hud_command` JSON:API) options.
- TODO desktop section refined accordingly.

### Desktop build/run procedure + systemd
- Expanded the dispatcher-desktop README with Linux dependency install
  (Debian/Ubuntu `libwt-dev`/`libwthttp-dev`, Fedora `wt-devel`), a Wt ≥ 4.5
  note for the Bootstrap 5 theme, configure/build/run, install-prefix run, and a
  systemd service procedure.
- Added `deploy/fleet-dispatcher-desktop.service` (hardened unit, runs with the
  install-prefix docroot so Wt `resources/` resolve) and `deploy/desktop.env.example`.

### Messaging Phase 1 CLOSED
- Finished the unblocked Phase-1 items: **unread counts + mark-as-read**
  (`channel_member.last_read_at`, `IonBadge`), **pull-to-refresh**
  (`IonRefresher`) on channel list + detail, and **attachment-load optimization**
  (sparse fieldsets via `getDocumentMeta`; full `data` fetched lazily on tap).
- Extracted the placeholder identity to `src/currentUser.ts`.
- Carried over to their owners: channel/DM creation + membership and current-user
  (→ auth), clickable toasts + realtime delivery (→ realtime), desktop messaging
  view (→ desktop), server-side `unread_count` (→ LogicBank).
- TODO: added the **Dispatcher Desktop Portal** section as the next pivot.
- Verified: mobile `npm run build` (tsc + Vite) passes.

### Messaging: attachment upload + list→detail navigation
- Restructured mobile nav to **list → detail** with native transitions and
  `IonBackButton` (KISS): `ChannelsPage` + `ChannelPage` (replacing the combined
  `MessagesPage`), and `LoadsPage` + `LoadDetailPage`; routes nested per tab.
- Attachment **upload**: file picker → base64 → create message + CMS `document`
  + `message_document` link. Attachments render as chips; tapping opens a
  base64 data URL (preview/download).
- Client: `getOne`, `getLoad`, `getChannel`, `getDocument`, `createDocument`,
  `linkMessageDocument`, `attachmentsForMessage`.
- Added `docs/MOBILE_UI_WIDGETS.md` — Ionic 8 widget vocabulary for UX language.
- Captured the planned **clickable message toast** pattern (deep-links to a
  channel; needs realtime) in TODO + the widget doc.
- Verified: mobile `npm run build` (tsc + Vite) passes.

### Merged the geospatial docs into one
- Combined the former `GEOSPATIAL.md` (decision + strategy) into
  `SPATIAL_GIS_DATA_CONSIDERATIONS.md` as the single source; removed
  `GEOSPATIAL.md`; repointed references in `domain-model.md` and `TODO.md`.

### GIS considerations doc
- Added `docs/SPATIAL_GIS_DATA_CONSIDERATIONS.md` — detailed considerations
  (ALS/PostGIS conflict internals, data model, indexing, geospatial endpoint,
  HERE, ingestion, ops/security, bootstrap), originally a companion to the
  geospatial strategy doc (later merged in).

### Decision ACCEPTED: separate PostGIS spatial data from ALS APIs
- Locked in the two-path approach: relational map data via ALS, spatial (PostGIS)
  via a dedicated endpoint over a `gis` schema. Accepted the added standup cost.
- Flipped `SPATIAL_GIS_DATA_CONSIDERATIONS.md` status to DECIDED; noted standup cost; marked the
  Feature 2 TODO section accordingly.

### Decision captured: geospatial / PostGIS strategy
- Added `docs/SPATIAL_GIS_DATA_CONSIDERATIONS.md`: to avoid the ALS↔PostGIS conflicts (PostGIS
  metadata reflected as junk resources; geometry types unmappable), use two
  access paths — relational map data (trips, waypoints, POIs, truck-stops,
  routes, position_report) via ALS with **plain lat/lng numeric** columns, and a
  **separate geospatial endpoint** for PostGIS `ST_*` over a dedicated `gis`
  schema (PostGIS installed there; excluded from ALS reflection). HERE handles
  truck-legal routing. Linked from `domain-model.md`; TODO items refined.

### Decision captured: multi-schema database layout
- Noted ALS multi-schema support. Documented a candidate split (peel `cms`
  out from `app`/ops; `telemetry` later) in `MIDDLEWARE_SETUP.md` and added a
  decision item to `TODO.md`. No DDL change yet — to apply when the DB is
  configured. Cross-schema FKs are supported, so relationships still hold.

### Messaging: send-message composer
- Mobile `MessagesPage` gains a composer (input + send) backed by a new
  `createMessage` JSON:API client helper (POST `/Message`).
- Current author is a hardcoded placeholder pending auth (tracked in TODO).
- Verified: mobile `npm run build` (tsc + Vite) passes.

### Project tracking
- Added `docs/TODO.md` (sequenced plan) and `docs/DEVELOPMENT_LOG.md` (this
  file), mirroring the workflow used in the sibling projects.

### Messaging & Content (CMS) — feature 1, slice 1
- Schema + seed: `channel`, `channel_member`, `message`; lookups `channel_type`,
  `channel_member_role`, `document_type`.
- Attachments modeled as a reusable **CMS `document`** (bytea + metadata) linked
  via FK join tables (`message_document`, `load_document`); the seed reuses one
  BOL document from both a message and a load.
- Docs: Messaging + CMS contexts; "Planned: Truck Location & Dispatcher HUD"
  (source-agnostic position ingestion — AirTag / Google / phone-push, PostGIS).
- Mobile: Channel/Message/Document types + client methods, `MessagesPage`,
  tabbed nav (Loads · Messages).
- Verified on PostgreSQL 16: schema + seed apply; document round-trips as text,
  linked to 1 message + 1 load.

### Middleware offloaded
- Moved the middleware README into `docs/MIDDLEWARE_SETUP.md`; removed the
  `middleware/` folder; trimmed `docker-compose.yml` to the `db` service.

### Mobile module publishing
- `scripts/publish-mobile.sh` + `Makefile` (`publish-mobile`, `pull-mobile`,
  `mobile-build`) for git-subtree publishing to `Fleet-Dispatcher-Mobile`.
- Exposed `publish-mobile` as a CMake target (+ opt-in `-DFLEET_PUBLISH_MOBILE`);
  default remote URL set to HTTPS.

## 2026-06-01

### SQL restructure for ALS
- Replaced Postgres ENUM types with lookup tables (sequential int keys); domain
  objects use GUID keys; all relationships are FK constraints. `driver_type`
  carries `default_percent` / `owner_bears_costs`. Renamed seed → `seed_data.sql`.
- Validated end-to-end on PostgreSQL 16.

### Mobile portal made buildable
- Added the app shell (index.html, main.tsx, App.tsx, LoadsPage), env typing,
  and a self-contained config; aligned `api/types.ts` to the lookup schema.

### Desktop (C++/Wt) theming + resources
- Applied `WBootstrap5Theme`; CMake auto-detects and deploys the entire Wt
  `resources/` tree beside the binary (+ install rules).

### Initial scaffold
- Three-tier DDD project: docs (architecture, domain model), schema + seed,
  middleware setup notes, and both portal scaffolds. (The standalone `domain/`
  Python package was later dropped in favor of schema + docs as the model.)

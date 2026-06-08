# Fleet Dispatcher — Development Log

Newest first. One entry per meaningful change set; pair with the checklist in
[`TODO.md`](TODO.md).

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

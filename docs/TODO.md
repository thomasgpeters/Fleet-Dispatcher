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

Carried over (blocked / owned elsewhere):
- [ ] Direct (1:1) channel creation + membership management — needs a user
      directory (→ auth)
- [ ] Current-user identity (replace `src/currentUser.ts` placeholder) → auth
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
- [ ] Add app-bar week selector + user/role/health (with auth)
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
- [~] Mobile Trips: list, start trip, add waypoint (done); trip start/stop
      lifecycle, navigate, and POIs pending
- [x] `gis` bootstrap SQL (`database/gis_bootstrap.sql`): PostGIS into `gis` +
      derived geography views; **verified** `public` stays clean (ALS-safe).
      Full standup in [`DEPLOYMENT.md`](DEPLOYMENT.md).
- [ ] Stand up the separate geospatial endpoint (PostGIS `ST_*`, not ALS);
      decide view vs. trigger-maintained geometry mirror.
- [ ] Ingestion adapters for AirTag / Google sources (phone-push done)
- [ ] HERE routing/maps integration (truck-legal routes, bridge heights, truck
      stops); persist as `route.polyline`
- [x] Desktop HUD **map** (`Wt::WLeafletMap`, OSM tiles) with a marker per rig's
      latest position (15s refresh)
- [ ] HUD map: overlay routes (`route.polyline`) and driver-focus/load-highlight
      commands

## Cross-cutting

- [ ] **Decide DB schema separation** (ALS multi-schema): peel `cms` (content)
      out from `app` (ops); `telemetry` later. Cross-schema FKs are fine. Apply
      when configuring the DB — see `MIDDLEWARE_SETUP.md` → "Database schema
      layout".
- [ ] AuthN/AuthZ (roles: dispatcher, driver, updater) — gates current-user,
      messaging, and HUD
- [ ] LogicBank rules in generated middleware (weekly cap, settlement math)
- [ ] Seed data growth for demos

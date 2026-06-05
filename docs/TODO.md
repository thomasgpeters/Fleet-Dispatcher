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
- [ ] Load intake form (new load) + driver/equipment assignment
- [ ] **HUD** surface (`/hud`) + `HudControlBus` (Wt server push): controls on the
      console publish commands (`SetMode`, …); HUD auto-switches Today/Week
- [ ] **Distributed HUD (decided):** also persist commands as a `hud_command`
      JSON:API resource so a remote HUD can subscribe (audit/replay)
- [ ] Extend the command bus to more commands (`FocusDriver`, `HighlightLoad`)
- [ ] Dispatcher messaging view (mirrors mobile Feature 1)
- [ ] **HUD map**: fleet **truck locations** — depends on Feature 2 telemetry +
      the geospatial endpoint; until then the HUD renders board/load/status data

> Wt isn't installable in the dev sandbox, so the C++ above is **not compiled
> here**. Version-sensitive spots are flagged in `ApiClient.cpp` (Http `done()`
> error_code type; `Json::Type::Null` enum) and `main.cpp`/theme (`WBootstrap5Theme`
> needs Wt ≥ 4.5).

## Feature 2 — Truck Location & Dispatcher HUD

Powers the HUD's map. See "Planned" in `domain-model.md`.
**Decided (2026-06-02):** PostGIS spatial data is separated from the ALS APIs
(see [`SPATIAL_GIS_DATA_CONSIDERATIONS.md`](SPATIAL_GIS_DATA_CONSIDERATIONS.md)).
**Decided (2026-06-02):** PostGIS spatial data is separated from the ALS APIs
(see [`SPATIAL_GIS_DATA_CONSIDERATIONS.md`](SPATIAL_GIS_DATA_CONSIDERATIONS.md)).

- [ ] Schema: `location_source` lookup (`airtag`, `google_device`,
      `phone_push`), `position_report` time series — **lat/lng numeric on ALS
      tables** (no geometry); geometry lives in a `gis` schema. See
      [`SPATIAL_GIS_DATA_CONSIDERATIONS.md`](SPATIAL_GIS_DATA_CONSIDERATIONS.md).
- [ ] Install PostGIS into its own `gis` schema; exclude it (and
      `spatial_ref_sys`) from ALS reflection.
- [ ] Stand up the separate geospatial endpoint (PostGIS `ST_*`, not ALS);
      decide view vs. trigger-maintained geometry mirror.
- [ ] Ingestion endpoints/adapters for AirTag / Google / phone-push sources
- [ ] HERE routing/maps integration (trips, waypoints, truck-legal routes,
      bridge heights, truck stops)
- [ ] Mobile: trip start/stop, add-to-trip, navigate, waypoints, POIs
- [ ] Desktop: Dispatcher HUD — fleet truck locations on a map + fleet data

## Cross-cutting

- [ ] **Decide DB schema separation** (ALS multi-schema): peel `cms` (content)
      out from `app` (ops); `telemetry` later. Cross-schema FKs are fine. Apply
      when configuring the DB — see `MIDDLEWARE_SETUP.md` → "Database schema
      layout".
- [ ] AuthN/AuthZ (roles: dispatcher, driver, updater) — gates current-user,
      messaging, and HUD
- [ ] LogicBank rules in generated middleware (weekly cap, settlement math)
- [ ] Seed data growth for demos

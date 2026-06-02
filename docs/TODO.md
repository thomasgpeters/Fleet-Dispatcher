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

## Feature 1 — Messaging & Content (CMS)

Schema/CMS foundation is in. Building out the experience next.

- [x] Schema: `channel`, `channel_member`, `message`; CMS `document` + FK link
      tables (`message_document`, `load_document`)
- [x] Mobile: Messages tab — list channels, view a channel's messages
- [x] Mobile: compose & send a message (write-side)
- [ ] Mobile: attachment upload (create `document` + `message_document` link)
- [ ] Mobile: attachment preview / download (bytea ↔ base64)
- [ ] Unread counts via `channel_member.last_read_at` + mark-as-read
- [ ] Direct (1:1) channel creation + membership management
- [ ] Current-user identity (replace hardcoded placeholder) — depends on auth
- [ ] Realtime delivery (websockets/push) — currently JSON:API polling
- [ ] Desktop (dispatcher) messaging view
- [ ] More CMS links as needed: `driver_document`, `equipment_document`,
      `inspection_document`

## Feature 2 — Truck Location & Dispatcher HUD

Planned; pivot here after messaging. See "Planned" in `domain-model.md`.

- [ ] Schema: `location_source` lookup (`airtag`, `google_device`,
      `phone_push`), `position_report` time series (PostGIS)
- [ ] Decide PostGIS reflection approach for ALS (geography vs lat/lng columns)
- [ ] Ingestion endpoints/adapters for AirTag / Google / phone-push sources
- [ ] HERE routing/maps integration (trips, waypoints, truck-legal routes,
      bridge heights, truck stops)
- [ ] Mobile: trip start/stop, add-to-trip, navigate, waypoints, POIs
- [ ] Desktop: Dispatcher HUD — fleet truck locations on a map + fleet data

## Cross-cutting

- [ ] AuthN/AuthZ (roles: dispatcher, driver, updater) — gates current-user,
      messaging, and HUD
- [ ] LogicBank rules in generated middleware (weekly cap, settlement math)
- [ ] Seed data growth for demos

# Fleet Dispatcher — Development Log

Newest first. One entry per meaningful change set; pair with the checklist in
[`TODO.md`](TODO.md).

## 2026-06-02

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

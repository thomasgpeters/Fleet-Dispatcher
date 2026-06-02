# Fleet Dispatcher — Development Log

Newest first. One entry per meaningful change set; pair with the checklist in
[`TODO.md`](TODO.md).

## 2026-06-02

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

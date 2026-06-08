# Fleet Dispatcher

Fleet Dispatcher lets organizations manage a fleet of drivers running varied
truck/trailer configurations, dispatch loads across a Monday-to-Monday week, and
settle driver pay based on contract type.

It is a **three-tier application**. We use a **consistent domain language** (the
ubiquitous terms in [`docs/domain-model.md`](docs/domain-model.md)) across the
schema, the API, and both portals to keep design and discussion aligned:

```
┌──────────────────────────────┐   ┌──────────────────────────────┐
│   Dispatcher Portal           │   │   Driver / Updater Mobile     │
│   Desktop — C++ / Wt          │   │   React 18 / Ionic 8 /        │
│                               │   │   Capacitor 6 / TS 5.5 / Vite │
└──────────────┬───────────────┘   └───────────────┬──────────────┘
               │      JSON:API (SAFRS)              │
               └────────────────┬───────────────────┘
                                │
                  ┌─────────────▼─────────────┐
                  │   ApiLogicServer          │   middleware  (port 5659)
                  │   JSON:API / SAFRS        │
                  │   LogicBank business rules│
                  └─────────────┬─────────────┘
                                │ SQLAlchemy
                  ┌─────────────▼─────────────┐
                  │   PostgreSQL              │   backend     (port 5432)
                  └───────────────────────────┘
```

> Sibling stack to the **Student-Onboarding** and **Smitty-Services** apps:
> ApiLogicServer (JSON:API / SAFRS) over PostgreSQL, with multiple portals
> sharing one middleware and one backend.

## Project structure

This repo is a **monorepo and the source of truth** for the whole system, but it
holds **several independently-built components** (different languages, different
build tools, different deploy targets). It is *not* a single buildable app — each
subtree below builds and deploys on its own.

```
Fleet-Dispatcher/                  ← this monorepo (source of truth)
├── database/                      backend — PostgreSQL (SQL, applied by a DBA)
│   ├── schema.sql                 app tables in the `fleet` schema
│   ├── seed_data.sql              reference + demo data
│   └── gis_bootstrap.sql          PostGIS into the shared `gis` schema
├── geospatial/                    spatial endpoint — Python / FastAPI
│   ├── app/main.py                /health, /truck-stops/nearest, /trucks/near …
│   ├── requirements.txt · Dockerfile · README.md
├── portals/
│   ├── dispatcher-desktop/        Dispatcher portal — C++ / Wt      ◀ VCP builds
│   │   ├── src/                   shell, board, load form, HUD, API client
│   │   ├── CMakeLists.txt         build (also deploys the Wt resources/ tree)
│   │   └── deploy/                systemd unit + env
│   └── mobile/                    Driver/Updater app — TS / Ionic   ◀ VCP builds
│       ├── src/                   pages + JSON:API client
│       └── package.json · vite.config.ts · capacitor.config.ts
├── scripts/                       db-setup.sh · publish-mobile.sh
├── docs/                          architecture, domain model, DEPLOYMENT, TODO, log
├── Makefile                       publish-mobile / pull-mobile / mobile-build
├── docker-compose.yml             local PostgreSQL
└── .env.example
```

> The **middleware (ApiLogicServer)** is **not** in this repo — it is *generated*
> from `database/schema.sql` and run separately (see
> [`docs/MIDDLEWARE_SETUP.md`](docs/MIDDLEWARE_SETUP.md)).

### Components — how each one is built & deployed

| Component            | Source                          | Build                                            | Produces              | Deployed by |
| -------------------- | ------------------------------- | ------------------------------------------------ | --------------------- | ----------- |
| **Mobile** (TS)      | `portals/mobile` → published to the `Fleet-Dispatcher-Mobile` repo | `npm ci && npm run build` | static SPA (`dist/`)  | **VCP**     |
| **Dispatcher desktop** (C++) | `portals/dispatcher-desktop`            | `cmake -S . -B build && cmake --build build`      | Wt HTTP server binary | **VCP**     |
| ALS middleware       | generated from `database/schema.sql`     | `ApiLogicServer create` / `run`                   | JSON:API on `:5659`   | separately (not VCP) |
| Geospatial endpoint  | `geospatial/`                            | `pip install -r requirements.txt`                 | FastAPI on `:5701`    | own Dockerfile / systemd |
| PostgreSQL           | `database/*.sql`                         | `scripts/db-setup.sh`                             | `fleet` + `gis` schemas | DBA / psql |

### Building with VCP

VCP builds container images **from source** for the **C++ and TS apps**. Two
components here are VCP-built:

- **Mobile (TS/Ionic).** VCP builds the standalone **`Fleet-Dispatcher-Mobile`**
  repo (a TS app at its repo root). We develop it here under `portals/mobile`
  and publish it to that repo with `make publish-mobile` (git subtree — see
  [Mobile module](#mobile-module-fleet-dispatcher-mobile) below). VCP runs the
  npm build and serves the static `dist/`.
- **Dispatcher desktop (C++/Wt).** VCP builds `portals/dispatcher-desktop` from
  source against your **pre-staged Wt template image**; `CMakeLists.txt` compiles
  the binary and deploys the Wt `resources/` tree beside it. Run it as a Wt HTTP
  server (`--docroot … --http-port …`); see the portal README + `deploy/` unit.

The remaining components are **not** VCP-built (they aren't C++/TS):
- **ALS** is generated and run on its own (`docs/MIDDLEWARE_SETUP.md`).
- **Geospatial endpoint** is Python/FastAPI — deploy via its `Dockerfile` or a
  systemd unit (`geospatial/README.md`).
- **PostgreSQL** is provisioned with SQL (`scripts/db-setup.sh`, `docs/DEPLOYMENT.md`).

> **Note on the desktop + VCP:** the mobile app gets its own repo because VCP
> builds from a repo root. If VCP also wants the **desktop** as its own repo
> root, we can mirror the mobile approach with a `publish-desktop` git-subtree of
> `portals/dispatcher-desktop` — say the word and I'll add it.

## Key documents

- [`docs/architecture.md`](docs/architecture.md) — three-tier topology and ports.
- [`docs/domain-model.md`](docs/domain-model.md) — the shared domain language:
  ubiquitous terms, bounded contexts, aggregates, and invariants. This is the
  vocabulary we use to keep design and conversation consistent.
- [`docs/MIDDLEWARE_SETUP.md`](docs/MIDDLEWARE_SETUP.md) — generating and running
  the ApiLogicServer middleware from the schema (managed outside this repo).
- [`docs/DEPLOYMENT.md`](docs/DEPLOYMENT.md) — full standup runbook: DB, schema,
  PostGIS (without breaking ALS), ALS, and portals.
- [`docs/TODO.md`](docs/TODO.md) — sequenced plan of approach (checklist).
- [`docs/DEVELOPMENT_LOG.md`](docs/DEVELOPMENT_LOG.md) — dated record of changes.

## Getting started

```bash
cp .env.example .env          # adjust ports / credentials if needed
docker compose up -d db       # start PostgreSQL on :5432
# load the schema + seed data
psql "$DATABASE_URL" -f database/schema.sql
psql "$DATABASE_URL" -f database/seed_data.sql
```

See [`docs/MIDDLEWARE_SETUP.md`](docs/MIDDLEWARE_SETUP.md) for the middleware,
and each portal's own `README.md` for portal setup.

## Mobile module (`Fleet-Dispatcher-Mobile`)

The mobile app is developed here in `portals/mobile`, but also lives in its own
repository, **`Fleet-Dispatcher-Mobile`**, which VCP builds and deploys. This
monorepo is the source of truth; we publish the folder to the standalone repo
with **git subtree** (the `portals/mobile` prefix is stripped, so `package.json`
lands at that repo's root).

```bash
# Publish portals/mobile -> Fleet-Dispatcher-Mobile
# (defaults to https://github.com/thomasgpeters/Fleet-Dispatcher-Mobile.git)
make publish-mobile

# Or call the script directly:
MOBILE_REMOTE_URL=... scripts/publish-mobile.sh

# If anyone commits directly in the standalone repo, pull it back (squashed):
make pull-mobile
```

The URL/branch/remote-name are configurable via `MOBILE_REMOTE_URL`,
`MOBILE_BRANCH`, and `MOBILE_REMOTE_NAME` (see [`Makefile`](Makefile) and
[`scripts/publish-mobile.sh`](scripts/publish-mobile.sh)).

> If `git subtree push` is ever rejected because the standalone repo has
> diverged, force a clean overwrite with a split:
> ```bash
> git push mobile "$(git subtree split --prefix=portals/mobile HEAD)":main --force
> ```

## Domain at a glance

- **Drivers** run **Equipment** (tractor + trailer configs: step-deck w/ ramps,
  RGN low-boy, 52' flatbed, RAM 3500/4500 w/ car carrier, …).
- **Loads** are dispatched with a pickup/drop-off **Location**, **Shipper**,
  **Receiver**, **Commodity**, dead-head + loaded distance, and a post-broker
  **Rate**. Runs are **Long-haul** or **Regional**.
- A **Dispatch Week** runs Monday→Monday; a driver takes up to **3–4 loads**.
- **Settlement** pays the driver a contract percentage of the rate:
  **Company Driver 30%**, **Owner-Operator 70%**.
- **Roles:** Dispatcher (desktop), Driver (mobile/ELD), Updater (mobile).

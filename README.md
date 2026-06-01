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
                  │   ApiLogicServer          │   middleware  (port 5656)
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

## Repository layout

| Path                          | Tier       | Description                                                            |
| ----------------------------- | ---------- | ---------------------------------------------------------------------- |
| `docs/MIDDLEWARE_SETUP.md`    | middleware | How to generate/run the ApiLogicServer middleware (managed outside this repo) on `:5656`. |
| `database/`                   | backend    | PostgreSQL schema (`schema.sql`) and seed data (`seed_data.sql`).      |
| `portals/dispatcher-desktop/` | client     | Desktop dispatcher portal — C++ / Wt.                                  |
| `portals/mobile/`             | client     | Driver & Updater mobile app — React / Ionic / Capacitor.              |
| `docs/`                       | —          | Architecture, domain model, and DDD pattern notes.                     |

## Key documents

- [`docs/architecture.md`](docs/architecture.md) — three-tier topology and ports.
- [`docs/domain-model.md`](docs/domain-model.md) — the shared domain language:
  ubiquitous terms, bounded contexts, aggregates, and invariants. This is the
  vocabulary we use to keep design and conversation consistent.
- [`docs/MIDDLEWARE_SETUP.md`](docs/MIDDLEWARE_SETUP.md) — generating and running
  the ApiLogicServer middleware from the schema (managed outside this repo).

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

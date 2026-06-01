# Fleet Dispatcher

Fleet Dispatcher lets organizations manage a fleet of drivers running varied
truck/trailer configurations, dispatch loads across a Monday-to-Monday week, and
settle driver pay based on contract type.

It is a **three-tier application** built around **Domain-Driven Design (DDD)**:

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
| `domain/`                     | core       | Pure-Python DDD model — the ubiquitous language expressed in code.     |
| `middleware/`                 | middleware | ApiLogicServer project (JSON:API / SAFRS, LogicBank rules) on `:5656`. |
| `database/`                   | backend    | PostgreSQL schema (`schema.sql`) and seed data (`seed.sql`).           |
| `portals/dispatcher-desktop/` | client     | Desktop dispatcher portal — C++ / Wt.                                  |
| `portals/mobile/`             | client     | Driver & Updater mobile app — React / Ionic / Capacitor.              |
| `docs/`                       | —          | Architecture, domain model, and DDD pattern notes.                     |

## Key documents

- [`docs/architecture.md`](docs/architecture.md) — three-tier topology and ports.
- [`docs/domain-model.md`](docs/domain-model.md) — ubiquitous language, bounded
  contexts, aggregates, and invariants.
- [`docs/ddd-patterns.md`](docs/ddd-patterns.md) — how the tactical DDD patterns
  are realized in this codebase.

## Getting started

```bash
cp .env.example .env          # adjust ports / credentials if needed
docker compose up -d db       # start PostgreSQL on :5432
# load the schema + seed data
psql "$DATABASE_URL" -f database/schema.sql
psql "$DATABASE_URL" -f database/seed.sql
```

See each tier's own `README.md` for middleware and portal setup.

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

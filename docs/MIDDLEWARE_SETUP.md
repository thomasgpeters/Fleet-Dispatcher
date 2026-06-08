# Middleware setup — ApiLogicServer

The application + API tier. [ApiLogicServer](https://apilogicserver.github.io/Docs/)
exposes the PostgreSQL domain as a **JSON:API** (via **SAFRS**) on **port 5656**
and enforces business rules declaratively with **LogicBank**.

> The middleware is **generated and managed outside this repository** — no
> middleware code lives here. This repo provides the database schema it's built
> from (`database/schema.sql`); generate the ApiLogicServer project wherever you
> manage it. This follows the database-first approach used by the
> Student-Onboarding and Smitty-Services apps.

## Create the project (one-time)

With PostgreSQL up and the schema loaded (run from the repo root):

```bash
# 1. Bring up the database and load schema + seed
docker compose up -d db
psql "$DATABASE_URL" -f database/schema.sql
psql "$DATABASE_URL" -f database/seed_data.sql

# 2. Generate the ApiLogicServer project from the schema
#    (into a project directory you manage — e.g. fleet-dispatcher-api)
pip install ApiLogicServer
ApiLogicServer create \
    --project_name=fleet-dispatcher-api \
    --db_url="$DATABASE_URL"

# 3. Run it (serves JSON:API + Swagger on :5656)
cd fleet-dispatcher-api
ApiLogicServer run --port "${API_PORT:-5656}"
```

Generated project layout (after step 2):

```
fleet-dispatcher-api/
  api/              SAFRS JSON:API endpoints (generated from models)
  database/         SQLAlchemy models reflected from PostgreSQL
  logic/            LogicBank declarative rules  ← business invariants live here
  ui/               admin app (optional)
  config/
```

## Business rules

The structural invariants from [`domain-model.md`](domain-model.md) are already
enforced by the schema (FK relationships, CHECK constraints, the Monday-only
`week_start`), so the generated API honors them out of the box.

The cross-row / computed invariants — weekly load cap (≤ 4), `driver_pay =
rate × contract_percent`, copying the percentage and cost-responsibility from
`driver_type` — are the kind of thing ApiLogicServer expresses as declarative
LogicBank rules in `logic/declare_logic.py` if/when you want them enforced at
the API tier. They're listed under "Policies / business rules" in the domain
model for reference.

## Configuration

Port and database come from the environment (see [`../.env.example`](../.env.example)):
`API_PORT` (default 5656), `DATABASE_URL`. The `db` service in
[`../docker-compose.yml`](../docker-compose.yml) brings up PostgreSQL with the
schema + seed applied for local development.

## Database schema layout (DECIDED)

This PostgreSQL instance is **shared** with the Smitty-Services and
Student-Onboarding apps, so Fleet Dispatcher is **logically separated into its
own schema** rather than living in `public`:

| Postgres schema | Owner       | Contents                                                       | ALS |
| --------------- | ----------- | ------------------------------------------------------------- | --- |
| `fleet`         | `fleet`     | **all** Fleet app tables + lookups (fleet/dispatch/settlement/messaging/CMS/telemetry/navigation) | ✅ reflected |
| `gis` (shared)  | (shared)    | PostGIS + Fleet's `fleet_*` geography views (owned by `fleet_gis`) | ❌ never |
| `public`        | —           | left to the shared instance; Fleet puts nothing here          | ❌ |

- ALS reflects the **`fleet`** schema: the `fleet` role's `search_path` is pinned
  to `fleet, public`, so `ApiLogicServer create` sees exactly Fleet's tables.
- Resource/endpoint names come from **table names**, so a named schema does not
  change the JSON:API surface — the portals are unaffected.
- All Fleet objects sit in one schema (no intra-app cross-schema FKs needed);
  the only cross-schema reference is the `gis` views reading `fleet.*` (one-way,
  read-only). See [`DEPLOYMENT.md`](DEPLOYMENT.md) and
  [`SPATIAL_GIS_DATA_CONSIDERATIONS.md`](SPATIAL_GIS_DATA_CONSIDERATIONS.md).

## Dependencies

`ApiLogicServer` (installed via `pip install ApiLogicServer`) transitively
provides SAFRS, LogicBank, Flask, and SQLAlchemy. Pin exact versions in the
generated project's own `requirements.txt` once a working set is confirmed.

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

## Dependencies

`ApiLogicServer` (installed via `pip install ApiLogicServer`) transitively
provides SAFRS, LogicBank, Flask, and SQLAlchemy. Pin exact versions in the
generated project's own `requirements.txt` once a working set is confirmed.

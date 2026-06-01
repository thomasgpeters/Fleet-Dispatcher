# Middleware — ApiLogicServer

The application + API tier. [ApiLogicServer](https://apilogicserver.github.io/Docs/)
exposes the PostgreSQL domain as a **JSON:API** (via **SAFRS**) on **port 5656**
and enforces business rules declaratively with **LogicBank**.

This directory holds the generated ApiLogicServer project. It is created
**from the database** (`database/schema.sql`), matching the database-first
approach used by the Student-Onboarding and Smitty-Services apps.

## Create the project (one-time)

With PostgreSQL up and the schema loaded:

```bash
# 1. Bring up the database and load schema + seed
docker compose up -d db
psql "$DATABASE_URL" -f ../database/schema.sql
psql "$DATABASE_URL" -f ../database/seed_data.sql

# 2. Generate the ApiLogicServer project from the schema, into this directory
pip install ApiLogicServer
ApiLogicServer create \
    --project_name=. \
    --db_url="$DATABASE_URL"

# 3. Run it (serves JSON:API + Swagger on :5656)
ApiLogicServer run --port "${API_PORT:-5656}"
```

Generated layout (after step 2):

```
middleware/
  api/              SAFRS JSON:API endpoints (generated from models)
  database/         SQLAlchemy models reflected from PostgreSQL
  logic/            LogicBank declarative rules  ← business invariants live here
  ui/               admin app (optional)
  config/
```

## Business rules

The structural invariants from [`../docs/domain-model.md`](../docs/domain-model.md)
are already enforced by the schema (FK relationships, CHECK constraints, the
Monday-only `week_start`), so the generated API honors them out of the box.

The cross-row / computed invariants — weekly load cap (≤ 4), `driver_pay =
rate × contract_percent`, copying the percentage and cost-responsibility from
`driver_type` — are the kind of thing ApiLogicServer expresses as declarative
LogicBank rules in `logic/declare_logic.py` if/when you want them enforced at
the API tier. They're listed under "Policies / business rules" in the domain
model for reference.

## Configuration

Port and database come from the environment (see [`../.env.example`](../.env.example)):
`API_PORT` (default 5656), `DATABASE_URL`. The `api` service in
[`../docker-compose.yml`](../docker-compose.yml) wires these for container runs
(enable with the `full` profile once this project exists).

## requirements.txt

Pinned middleware dependencies live in `requirements.txt`. ApiLogicServer pulls
in SAFRS, LogicBank, Flask, and SQLAlchemy.

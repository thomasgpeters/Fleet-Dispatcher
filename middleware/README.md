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
psql "$DATABASE_URL" -f ../database/seed.sql

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

## Business rules to declare in `logic/`

These re-state, at the transaction boundary, the invariants from
[`../docs/domain-model.md`](../docs/domain-model.md) and the
[`../domain/`](../domain) package. Sketch (LogicBank):

```python
from logic_bank.logic_bank import Rule

def declare_logic():
    # Driver pay = gross_rate × contract_percent / 100, rounded to cents.
    Rule.formula(derive=models.Settlement.driver_pay,
                 as_expression=lambda row: round(row.gross_rate * row.contract_percent / 100, 2))

    # Settlement percentage must match the driver's contract percentage.
    Rule.copy(derive=models.Settlement.contract_percent,
              from_parent=models.Driver.contract_percent)

    # Weekly load cap: ≤ 4 non-cancelled loads per driver per dispatch week.
    Rule.count(derive=models.DispatchWeekDriver.load_count,
               as_count_of=models.Load)            # via an association/derived view
    Rule.constraint(validate=models.DispatchWeekDriver,
                    as_condition=lambda row: row.load_count <= 4,
                    error_msg="Driver exceeds 4 loads in dispatch week")

    # Structural guards mirrored from CHECK constraints.
    Rule.constraint(validate=models.Load,
                    as_condition=lambda row: row.rate >= 0 and row.pickup_id != row.dropoff_id,
                    error_msg="Invalid load: negative rate or same pickup/dropoff")
```

> The exact model/attribute names depend on the generated SQLAlchemy classes;
> adjust after generation. Keep the rule set aligned with the `domain/` tests —
> those tests are the executable specification.

## Configuration

Port and database come from the environment (see [`../.env.example`](../.env.example)):
`API_PORT` (default 5656), `DATABASE_URL`. The `api` service in
[`../docker-compose.yml`](../docker-compose.yml) wires these for container runs
(enable with the `full` profile once this project exists).

## requirements.txt

Pinned middleware dependencies live in `requirements.txt`. ApiLogicServer pulls
in SAFRS, LogicBank, Flask, and SQLAlchemy.

# Deployment runbook

End-to-end standup on a Linux host: PostgreSQL → schema/seed → **PostGIS
(without breaking ALS)** → ApiLogicServer → portals. Companion to
[`MIDDLEWARE_SETUP.md`](MIDDLEWARE_SETUP.md) (ALS detail) and
[`SPATIAL_GIS_DATA_CONSIDERATIONS.md`](SPATIAL_GIS_DATA_CONSIDERATIONS.md) (the
PostGIS strategy and rationale).

> All steps below were verified on PostgreSQL 16 + PostGIS 3.4 (schema, seed, and
> the `gis` bootstrap apply cleanly; `public` ends up with **zero** PostGIS
> objects, so ALS reflection stays clean).

## Order of operations

```
1. PostgreSQL up + role + database
2. Apply database/schema.sql        (app tables in the `fleet` schema)
3. Apply database/seed_data.sql     (reference + demo data)
4. Apply database/gis_bootstrap.sql (PostGIS into shared `gis` — isolated from ALS)
5. ApiLogicServer create + run      (reflects `fleet`, never `gis`/`public`)
6. Geospatial endpoint role         (search_path = gis, fleet, public)  [Feature 2]
7. Portals: desktop (Wt) + mobile (VCP)
8. Hey Dispatch assistant service   (pluggable AI provider)            [Feature 3]
```

Steps 1–3 give a working ALS API. Step 4 adds spatial support safely. Steps 6–8
come online as those components are built.

**Quick path:** `scripts/db-setup.sh` runs steps 1–4 (creates the role + DB,
applies `schema.sql`, `seed_data.sql`, and `gis_bootstrap.sql`). It runs the
PostGIS step via the admin connection because `CREATE EXTENSION postgis` needs a
superuser. Flags: `--no-create`, `--no-seed`, `--no-gis`.

```bash
# uses .env (DB_*) + an admin psql for role/db + PostGIS:
FLEET_ADMIN_PSQL='sudo -u postgres psql' scripts/db-setup.sh
```

---

## Ports on the shared host

This host also runs sibling apps, so Fleet Dispatcher's listeners avoid their
ports:

| Service                              | Port   | Notes                              |
| ------------------------------------ | ------ | ---------------------------------- |
| Student-Onboarding (sibling)         | `8080` | in use — do not reuse              |
| Smitty-Services (sibling)            | `8085` | in use — do not reuse              |
| **Fleet — Dispatcher desktop (Wt)**  | `8089` | console `/` + HUD `/hud`           |
| **Fleet — ApiLogicServer (JSON:API)**| `5659` | `API_PORT`                         |
| **Fleet — Geospatial endpoint**      | `5701` | `GIS_PORT`                         |
| **Fleet — Hey Dispatch assistant**   | `5710` | `ASSISTANT_PORT`                   |
| PostgreSQL (shared)                  | `5432` | one instance, schema-separated     |

Change the desktop port with `HTTP_PORT=… ./run.sh` (dev) or the unit's
`--http-port` (prod).

## 0. Prerequisites

```bash
# Debian/Ubuntu
sudo apt-get update
sudo apt-get install -y postgresql-16 postgresql-16-postgis-3 python3-pip
```

PostGIS is a **separate package** — the base `postgres` install (and the plain
`postgres:16` Docker image) does **not** include it. For containers use the
`postgis/postgis:16-3.4` image instead of `postgres:16`.

Copy and edit environment:

```bash
cp .env.example .env       # set DB_*, API_PORT; DATABASE_URL is derived
```

---

## 1. PostgreSQL: role + database

```bash
sudo -u postgres psql <<'SQL'
CREATE ROLE fleet LOGIN PASSWORD 'fleet';
CREATE DATABASE fleet_dispatcher OWNER fleet;
SQL
```

Connection string used below (matches `.env`):

```bash
export DATABASE_URL="postgresql://fleet:fleet@localhost:5432/fleet_dispatcher"
```

> Local-dev shortcut: `docker compose up -d db` brings up PostgreSQL with the
> schema + seed auto-applied — but swap the image to `postgis/postgis:16-3.4` in
> `docker-compose.yml` first if you want PostGIS in the container, then run
> step 4 manually.

## 2. Apply the schema (app tables → `fleet`)

```bash
psql "$DATABASE_URL" -f database/schema.sql
```

## 3. Apply seed data

```bash
psql "$DATABASE_URL" -f database/seed_data.sql
```

At this point the app is fully usable through ALS (steps 5).

## 4. PostGIS — installed into `gis`, isolated from ALS

This is the **"PostGIS without breaking ALS"** step. The trick: install the
extension into its **own schema** so its metadata (`spatial_ref_sys`,
`geometry_columns`, `geography_columns`) never lands in `public`.

`CREATE EXTENSION postgis` needs a **superuser**, so run it as admin (or via
`scripts/db-setup.sh`, which does):

```bash
sudo -u postgres psql -d fleet_dispatcher -f database/gis_bootstrap.sql
```

[`database/gis_bootstrap.sql`](../database/gis_bootstrap.sql) does:

```sql
CREATE ROLE fleet_gis LOGIN PASSWORD '…';              -- the gis owner (dev pw)
CREATE SCHEMA IF NOT EXISTS gis;                       -- shared schema (not hijacked)
CREATE EXTENSION IF NOT EXISTS postgis SCHEMA gis;     -- → gis.spatial_ref_sys, …
GRANT USAGE, CREATE ON SCHEMA gis TO fleet_gis;        -- fleet_gis creates its views
-- + derived geography VIEWS (gis.fleet_position_geog, gis.fleet_poi_geog), owned
--   by fleet_gis, over the fleet.* lat/lng tables (no duplicated data, in sync).
ALTER ROLE fleet_gis SET search_path = gis, fleet, public;
```

**Shared instance:** `gis` is a shared schema (Smitty / Student-Onboarding may
use it too), so we don't take it over — `fleet_gis` is granted `CREATE` on it and
owns only Fleet's `fleet_*` views (prefixed to avoid collisions). The PostGIS
*extension* objects (`spatial_ref_sys`, `ST_*` functions) stay superuser-owned —
by PostGIS design. Verified: the `fleet_*` views report owner `fleet_gis`.

Verify `public` is clean (this is what keeps ALS happy):

```bash
psql "$DATABASE_URL" -tAc "SELECT count(*) FROM information_schema.tables
  WHERE table_schema='public'
    AND table_name IN ('spatial_ref_sys','geometry_columns','geography_columns');"
# → 0
```

**search_path gotcha (important):** PostGIS *functions* can be schema-qualified
(`gis.ST_MakePoint(...)`), but *operators* (`<->`, `&&`) cannot — they only
resolve when `gis` is on the `search_path`. So any spatial query (and the
geospatial endpoint's connection) must:

```sql
SET search_path = gis, fleet, public;   -- then <-> etc. resolve
```

ALS does **not** need this (it never runs spatial operators), which is the whole
point — keep `gis` off the ALS connection's path.

> Re-applying after schema changes: `schema.sql` is a full create (not
> migrations), so re-run cleanly with `scripts/db-setup.sh --no-create --reset`
> (drops & rebuilds the `fleet` schema, then re-seeds — destroys fleet data).

## 5. ApiLogicServer (reflects `fleet`, never `gis`)

The `fleet` role's `search_path` is set to `fleet, public` (by `db-setup.sh`), so
ALS reflects the **`fleet`** schema. PostGIS lives in `gis` (off the path), and
the app tables are in `fleet` (not `public`), so nothing spatial — and nothing
from the other apps in this shared instance — gets reflected.

```bash
pip install ApiLogicServer

# Generate from the database (database-first). Reflect the fleet schema:
ApiLogicServer create --project_name=fleet-dispatcher-api \
    --db_url="$DATABASE_URL" --from_git ""   # see note on --schema below

cd fleet-dispatcher-api
# ALS defaults to 5656; Fleet Dispatcher expects 5659. Set it as a RUN setting —
# don't regenerate for the port (a fresh create wipes logic/ + add-auth):
python api_logic_server_run.py 0.0.0.0 5659      # positional host port
# permanent: set the port in config/config.py (APILOGICSERVER_PORT / PORT = 5659)
# or:        export APILOGICSERVER_PORT=5659 && python api_logic_server_run.py
```

> ALS reflects the schema(s) on the connection's `search_path`; because the
> `fleet` role is pinned to `fleet, public`, you get exactly Fleet's tables. If
> your ALS version exposes an explicit schema flag, point it at `fleet`. Resource
> names come from table names, so the portals are unaffected.

- JSON:API + Swagger serve on `:5659`. See [`MIDDLEWARE_SETUP.md`](MIDDLEWARE_SETUP.md).
- If anything spatial ever *does* get reflected (e.g. you installed PostGIS into
  `public` by mistake), the fix is to reinstall it into `gis` — or, as a
  fallback, remove the unwanted classes from the generated `database/models.py`
  and `api/expose_api_models.py` and restart.
- The ALS DB role needs rights on `public` only; grant it **no** access to `gis`.

## 6. Geospatial endpoint  *(Feature 2)*

The `fleet_gis` role (created by step 4) owns the `gis` objects and already has
`search_path = gis, public` and SELECT on the public lat/lng tables — so the
endpoint just connects as `fleet_gis`. See [`../geospatial/README.md`](../geospatial/README.md).

```bash
cd geospatial
pip install -r requirements.txt
GIS_DATABASE_URL="postgresql://fleet_gis:fleet_gis@localhost:5432/fleet_dispatcher" \
  uvicorn app.main:app --host 0.0.0.0 --port "${GIS_PORT:-5701}"
```

Example query the endpoint runs (verified):

```sql
-- as fleet_gis (search_path preset)
SELECT name FROM gis.fleet_poi_geog
ORDER BY geog <-> ST_SetSRID(ST_MakePoint(:lng, :lat), 4326)::geography
LIMIT 5;
```

## 7. Portals

- **Dispatcher desktop (C++/Wt):** build, run, and systemd in
  [`../portals/dispatcher-desktop/README.md`](../portals/dispatcher-desktop/README.md)
  (`deploy/` has the unit file). Point it at the API with `FLEET_API_BASE_URL`.
- **Mobile (TS/Ionic):** built/deployed by VCP from the
  `Fleet-Dispatcher-Mobile` repo (`make publish-mobile`); set
  `VITE_API_BASE_URL` and `VITE_ASSISTANT_BASE_URL`.

## 8. Hey Dispatch assistant  *(Feature 3)*

The driver voice assistant — a small FastAPI service that calls a **pluggable AI
provider** (Anthropic / OpenAI / Ollama) with tool use. The backend admin picks
the provider and supplies its key via env; the mobile app does push-to-talk and
on-device speech (Web Speech API). See [`../assistant/README.md`](../assistant/README.md).

```bash
cd assistant
cp .env.example .env            # set ASSISTANT_PROVIDER + its credential
pip install -r requirements.txt # only the SDK for your provider is loaded (lazy)
set -a; . ./.env; set +a
uvicorn app.main:app --host 0.0.0.0 --port "${ASSISTANT_PORT:-5710}"
```

| `ASSISTANT_PROVIDER` | Credential / endpoint                  | Default model     |
| -------------------- | -------------------------------------- | ----------------- |
| `anthropic` (default)| `ANTHROPIC_API_KEY`                    | `claude-opus-4-8` |
| `openai`             | `OPENAI_API_KEY` (+ `OPENAI_BASE_URL`) | `gpt-4o`          |
| `ollama`             | `OLLAMA_BASE_URL` (`…:11434/v1`)       | `llama3.1`        |

It calls the ALS JSON:API (`FLEET_API_BASE_URL`) to post dispatcher messages and,
optionally, HERE (`HERE_API_KEY`) for ETA/route tools — without HERE it falls
back to straight-line estimates. Point the mobile app at it with
`VITE_ASSISTANT_BASE_URL`.

```bash
curl -s localhost:5710/health
# {"status":"ok","provider":"anthropic","model":"claude-opus-4-8","ai_configured":true,"here_configured":false}
```

For a long-running install, use the systemd unit in
[`../assistant/deploy/`](../assistant/deploy/) (`fleet-dispatcher-assistant.service`
+ `assistant.env.example`) — runs uvicorn from a venv as the `fleet` user, with
the provider key in a `chmod 600` env file. Install steps are in
[`../assistant/README.md`](../assistant/README.md#production-systemd).

---

## Verification checklist

```bash
# API up?
curl -s "http://localhost:${API_PORT:-5659}/api/Driver" | head -c 200

# app tables live in fleet (not public)?  (fleet > 0, public app tables = 0)
psql "$DATABASE_URL" -tAc "SELECT count(*) FROM information_schema.tables WHERE table_schema='fleet';"

# public clean of PostGIS? (must be 0)
psql "$DATABASE_URL" -tAc "SELECT count(*) FROM information_schema.tables
  WHERE table_schema='public' AND table_name='spatial_ref_sys';"

# spatial query works with the right path?
psql "$DATABASE_URL" -c "SET search_path = gis, fleet, public;
  SELECT count(*) FROM gis.fleet_position_geog;"
```

## Troubleshooting

| Symptom                                             | Cause / fix                                                            |
| --------------------------------------------------- | --------------------------------------------------------------------- |
| ALS generates `SpatialRefSys`/`geometry_columns`    | PostGIS was installed into `public`. Reinstall into `gis` (step 4).    |
| `operator does not exist: geography <-> geography`  | `gis` not on `search_path`. `SET search_path = gis, fleet, public;`.  |
| `permission denied for table <fleet lookup>`        | grant `fleet_gis` SELECT on the table its views/queries need.         |
| `could not open extension control file … postgis`   | PostGIS package not installed (step 0) / wrong Docker image.           |
| ALS can't connect                                   | Check `DATABASE_URL`, role/password, `pg_hba.conf`.                   |

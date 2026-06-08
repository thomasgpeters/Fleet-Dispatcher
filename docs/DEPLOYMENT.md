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
2. Apply database/schema.sql        (app tables in `public`)
3. Apply database/seed_data.sql     (reference + demo data)
4. Apply database/gis_bootstrap.sql (PostGIS into `gis` — isolated from ALS)
5. ApiLogicServer create + run      (reflects `public`, never `gis`)
6. Geospatial endpoint role         (search_path = gis, public)  [Feature 2]
7. Portals: desktop (Wt) + mobile (VCP)
```

Steps 1–3 give a working ALS API. Step 4 adds spatial support safely. Steps 6–7
come online as those components are built.

---

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

## 2. Apply the schema (app tables → `public`)

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

```bash
psql "$DATABASE_URL" -f database/gis_bootstrap.sql
```

[`database/gis_bootstrap.sql`](../database/gis_bootstrap.sql) does:

```sql
CREATE SCHEMA IF NOT EXISTS gis;
CREATE EXTENSION IF NOT EXISTS postgis SCHEMA gis;     -- → gis.spatial_ref_sys, …
-- + derived geography VIEWS (gis.position_geog, gis.poi_geog) over the
--   public lat/lng tables (no duplicated data, always in sync).
```

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
SET search_path = gis, public;   -- then <-> etc. resolve
```

ALS does **not** need this (it never runs spatial operators), which is the whole
point — keep `gis` off the ALS connection's path.

## 5. ApiLogicServer (reflects `public`, never `gis`)

```bash
pip install ApiLogicServer

# Generate the project from the database (database-first). It reflects the
# `public` schema; because PostGIS lives in `gis`, none of its objects appear.
ApiLogicServer create --project_name=fleet-dispatcher-api --db_url="$DATABASE_URL"

cd fleet-dispatcher-api
ApiLogicServer run --port "${API_PORT:-5656}"
```

- JSON:API + Swagger serve on `:5656`. See [`MIDDLEWARE_SETUP.md`](MIDDLEWARE_SETUP.md).
- If anything spatial ever *does* get reflected (e.g. you installed PostGIS into
  `public` by mistake), the fix is to reinstall it into `gis` — or, as a
  fallback, remove the unwanted classes from the generated `database/models.py`
  and `api/expose_api_models.py` and restart.
- The ALS DB role needs rights on `public` only; grant it **no** access to `gis`.

## 6. Geospatial endpoint role  *(Feature 2 — when built)*

The spatial endpoint is a separate process with its own least-privilege role
that *does* use `gis`:

```sql
CREATE ROLE fleet_gis LOGIN PASSWORD '...';
GRANT USAGE ON SCHEMA gis, public TO fleet_gis;
GRANT SELECT ON ALL TABLES IN SCHEMA gis TO fleet_gis;
GRANT SELECT ON public.position_report, public.point_of_interest TO fleet_gis;
ALTER ROLE fleet_gis SET search_path = gis, public;   -- so <-> resolves
```

Example query the endpoint runs (verified):

```sql
SET search_path = gis, public;
SELECT name FROM gis.poi_geog
ORDER BY geog <-> ST_SetSRID(ST_MakePoint(:lng, :lat), 4326)::geography
LIMIT 5;
```

## 7. Portals

- **Dispatcher desktop (C++/Wt):** build, run, and systemd in
  [`../portals/dispatcher-desktop/README.md`](../portals/dispatcher-desktop/README.md)
  (`deploy/` has the unit file). Point it at the API with `FLEET_API_BASE_URL`.
- **Mobile (TS/Ionic):** built/deployed by VCP from the
  `Fleet-Dispatcher-Mobile` repo (`make publish-mobile`); set
  `VITE_API_BASE_URL`.

---

## Verification checklist

```bash
# API up?
curl -s "http://localhost:${API_PORT:-5656}/api/Driver" | head -c 200

# public clean of PostGIS? (must be 0)
psql "$DATABASE_URL" -tAc "SELECT count(*) FROM information_schema.tables
  WHERE table_schema='public' AND table_name='spatial_ref_sys';"

# spatial query works with the right path?
psql "$DATABASE_URL" -c "SET search_path = gis, public;
  SELECT count(*) FROM gis.position_geog;"
```

## Troubleshooting

| Symptom                                             | Cause / fix                                                            |
| --------------------------------------------------- | --------------------------------------------------------------------- |
| ALS generates `SpatialRefSys`/`geometry_columns`    | PostGIS was installed into `public`. Reinstall into `gis` (step 4).    |
| `operator does not exist: geography <-> geography`  | `gis` not on `search_path`. `SET search_path = gis, public;`.         |
| `could not open extension control file … postgis`   | PostGIS package not installed (step 0) / wrong Docker image.           |
| ALS can't connect                                   | Check `DATABASE_URL`, role/password, `pg_hba.conf`.                   |

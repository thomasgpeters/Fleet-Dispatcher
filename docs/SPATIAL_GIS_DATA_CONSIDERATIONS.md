# Spatial / GIS Data Considerations

The single source for the Fleet Dispatcher geospatial strategy — both the
**decision** and the **detailed considerations** behind separating PostGIS
spatial data from the ApiLogicServer (ALS) APIs. Pairs with
[`MIDDLEWARE_SETUP.md`](MIDDLEWARE_SETUP.md) → "Database schema layout".

> **Status: DECIDED (2026-06-02).** Spatial (PostGIS) data is separated from the
> ALS APIs — relational map data flows through ALS (plain lat/lng), spatial
> queries through a dedicated endpoint over a `gis` schema. Accepted that this is
> more to stand up; it's the right long-term shape. Remaining items are the
> sub-choices under "Open questions".

---

## 1. Decision & principle: two access paths

```
                 ┌──────────────────────────── clients ───────────────────────────┐
                 │ mobile (Ionic)                         desktop HUD (C++/Wt)      │
                 └───────────┬───────────────────────────────────┬─────────────────┘
                             │ relational map data (JSON:API)     │ spatial queries
                             ▼                                    ▼
                  ┌────────────────────┐              ┌──────────────────────────┐
                  │  ApiLogicServer    │              │  Geospatial endpoint      │
                  │  (JSON:API/SAFRS)  │              │  (PostGIS ST_* — NOT ALS) │
                  └─────────┬──────────┘              └─────────────┬────────────┘
                            │ fleet schema                         │ gis schema
                            ▼                                       ▼
                  ┌─────────────────────────── PostgreSQL ──────────────────────────┐
                  │ lat/lng numeric columns on ALS tables   │  PostGIS geometry +    │
                  │ (trips, waypoints, POIs, truck-stops,   │  GiST indexes in `gis` │
                  │  routes, position_report)               │  (views/mirror tables) │
                  └──────────────────────────────────────────────────────────────────┘
              External: HERE for truck-legal routing (bridge heights, restrictions).
```

- **ALS path (JSON:API):** all relational/CRUD map data, stored with plain
  numeric `lat` / `lng` (`double precision`). **No geometry columns on
  ALS-managed tables.** ALS serves these with full relationships.
- **Spatial path (not ALS):** a dedicated geospatial endpoint runs PostGIS
  `ST_*` queries against geometry in the `gis` schema. Clients call it for
  nearest / within / route-geometry questions. Routing itself (truck-legal
  roads, bridge clearances, recommended routes) comes from **HERE**.

---

## 2. Why ALS and PostGIS conflict (in detail)

### 2.1 PostGIS metadata pollutes ALS reflection
`CREATE EXTENSION postgis` installs, into its target schema:
- `spatial_ref_sys` — a real **table** (~8500 SRID rows).
- `geometry_columns`, `geography_columns` — **views**.
- `raster_columns`, `raster_overviews` — views (if raster is enabled).

ApiLogicServer's `create` reflects tables/views and generates a JSON:API
resource per object. Left in `public`, these become junk endpoints and are the
**naming conflicts** seen in past projects. Mitigation: install PostGIS in its
own `gis` schema and **do not** include `gis` in ALS reflection.

### 2.2 Geometry types have no JSON:API mapping
`geometry` / `geography` columns are not understood by plain SQLAlchemy/SAFRS;
they need **GeoAlchemy2**, and even then the value is WKB/EWKB — not something
JSON:API should emit. A geometry column on an ALS-managed table therefore breaks
reflection or serialization. Mitigation: **no geometry columns on ALS tables**;
store `lat`/`lng` as `double precision` and derive geometry in `gis`.

### 2.3 Spatial work is functional, not CRUD
`ST_DWithin`, `ST_Distance`, KNN nearest-neighbor, route geometry, geofencing —
these are query operations, not the row CRUD ALS auto-generates. They belong
behind a purpose-built endpoint.

---

## 3. Database layout

- Install PostGIS into its **own schema**, so its metadata never reaches the app:
  ```sql
  CREATE SCHEMA IF NOT EXISTS gis;
  CREATE EXTENSION IF NOT EXISTS postgis SCHEMA gis;
  ```
- **ALS reflects `fleet` only — never `gis`.** (Pass the
  schema list to `ApiLogicServer create`; keep `gis` and `spatial_ref_sys` out.)

### What goes where

| Resource                         | Schema      | Via ALS? | Geometry?                         |
| -------------------------------- | ----------- | -------- | --------------------------------- |
| `trip`, `waypoint`               | `fleet`     | ✅ yes   | lat/lng numeric                   |
| `point_of_interest` (truck stops)| `fleet`     | ✅ yes   | lat/lng numeric                   |
| `route` (metadata, HERE polyline)| `fleet`     | ✅ yes   | encoded polyline as text          |
| `position_report`                | `fleet`     | ✅ yes   | lat/lng numeric                   |
| `location_source` (lookup)       | `fleet`     | ✅ yes   | —                                 |
| `fleet_*` geography views, indexes| `gis` (shared) | ❌ no | PostGIS geometry/geography + GiST |
| PostGIS extension + metadata     | `gis` (shared) | ❌ no | (engine internals)                |
| Truck-legal routing              | external    | —        | HERE API                          |

---

## 4. Data model considerations

- **Coordinates on ALS tables:** `lat double precision`, `lng double precision`,
  WGS84 (SRID 4326). Keep a consistent column naming convention everywhere
  (`lat`, `lng`).
- **`position_report` is a time series** — expect high write volume (many rigs ×
  frequent pings). Consider:
  - index `(equipment_id, recorded_at DESC)` for "latest per rig";
  - **BRIN** index on `recorded_at` for cheap time-range scans;
  - **partitioning** by time (monthly) and a **retention policy** (e.g. keep raw
    points N days, downsample older);
  - "latest known position per rig" served as a view or a small current-position
    table the HUD reads via JSON:API.
- **SRID/projection:** use `geography(Point,4326)` for correct earth-distance
  math (meters) without choosing a projection; switch to projected `geometry`
  only if a specific local CRS is needed for performance.

---

## 5. Geometry derivation & indexing (in `gis`)

Geometry is **derived from the ALS lat/lng — never hand-duplicated**:

| Approach                         | Sync        | Indexable | Use when                                   |
| -------------------------------- | ----------- | --------- | ------------------------------------------ |
| `gis` **view** (`ST_MakePoint`)  | always live | no        | simple reads, low volume                   |
| **materialized view**            | on refresh  | yes       | periodic refresh acceptable                |
| **mirror table + trigger**       | live        | yes (GiST)| indexed nearest-neighbor at fleet scale    |

- Example view: `gis.fleet_position_geog AS SELECT id, equipment_id, recorded_at,
  ST_SetSRID(ST_MakePoint(lng, lat), 4326)::geography AS geog FROM
  fleet.position_report;` — zero storage, always in sync, cannot be indexed.
- `ST_MakePoint` / `ST_SetSRID` are immutable, so a `STORED` generated geometry
  column is possible — but only on a `gis`-schema table, never an ALS one.
- Index geometry with **GiST** (or SP-GiST); use the `<->` KNN operator for
  "nearest truck stop / trucks near X".

---

## 6. The geospatial endpoint

- **Responsibility:** all PostGIS `ST_*` queries (nearest, within, route
  geometry, geofence). The only component that imports GeoAlchemy2 / raw PostGIS.
- **Host:** a small **FastAPI** service over the `gis` schema — implemented in
  [`../geospatial/`](../geospatial) (connects as `fleet_gis`). Returns plain
  JSON/REST (not JSON:API — that's ALS's job).
- **DB access:** its **own read-mostly DB role**, granted on `gis` (+ read on the
  `fleet` lat/lng tables it derives from). ALS keeps its own role with **no**
  rights on `gis`. Least privilege both ways.
- **Contract:** returns plain JSON (GeoJSON where useful) so clients don't need a
  spatial client library for simple cases.

---

## 7. Routing provider (HERE) considerations

- Truck routing inputs: height, weight, length, axle count, hazmat, trailer
  count → drives bridge-clearance and truck-legal routing.
- **Store** the returned route as an encoded **polyline (text)** on an ALS
  `route` row (JSON:API-safe); optionally materialize geometry in `gis` for
  spatial ops (e.g. "is truck off-route").
- **Caching / cost:** cache routes by (origin, destination, truck profile);
  watch rate limits and per-request cost.
- **Secrets:** HERE API keys are configuration/secrets — never in the repo;
  injected via environment (VCP/`.env`), used by the client and/or the endpoint.

---

## 8. Ingestion (source-agnostic positions)

- Sources are interchangeable: **Apple AirTag, Google devices, driver phone
  push** → all normalized into `position_report` (lookup `location_source`).
- Considerations: per-source **auth**, **dedup** (same fix arriving twice),
  **clock skew** (trust `recorded_at` vs server receipt time), out-of-order
  arrivals, accuracy/heading/speed fields, and privacy/retention for driver
  location.
- Ingestion is a write path; whether it lands via ALS (simple `position_report`
  POST) or a dedicated intake worker is an open question (high-rate ingest may
  bypass ALS).

---

## 9. Ops, security, backup

- **PostGIS version pinning:** the DB image must include PostGIS (the plain
  `postgres:16` image does **not**); use `postgis/postgis` or install the
  extension. CI/test runners that spin up Postgres need it too.
- **Extension upgrades:** `ALTER EXTENSION postgis UPDATE` on version bumps.
- **Backup/restore:** `spatial_ref_sys` and `gis` objects are part of the dump;
  large `bytea` documents (CMS) and high-volume `position_report` affect backup
  size — consider separate backup cadences per schema.
- **Search path:** set `search_path` so unqualified calls resolve; or always
  schema-qualify (`gis.ST_*`).

---

## 10. Consequences & accepted standup cost

- ALS stays clean: no geometry types, no PostGIS internals, no naming clashes.
- The spatial endpoint is the only place that needs GeoAlchemy2 / raw PostGIS.
- Clients use two endpoints for maps: JSON:API (data) + the spatial endpoint
  (queries). The HUD reads latest `position_report` per rig via JSON:API and, if
  it needs "trucks near X", asks the spatial endpoint.
- **Standup cost (accepted):** one extra deployable (the geospatial endpoint)
  and a `gis` bootstrap. More to stand up, deliberately chosen for the cleaner
  long-term separation.

---

## 11. Standing it up (bootstrap, to be scripted with Feature 2)

```sql
CREATE SCHEMA IF NOT EXISTS gis;
CREATE EXTENSION IF NOT EXISTS postgis SCHEMA gis;
-- fleet tables (lat/lng) created via the normal schema.sql (ALS-facing)
-- gis views / mirror tables + GiST indexes created here (NOT reflected by ALS)
-- ALS: reflect the fleet schema (search_path = fleet, public), excluding gis
```
A compose/VCP entry for the geospatial endpoint and the `gis` bootstrap SQL will
be added when Feature 2 is built, so a fresh environment comes up repeatably.

---

## 12. Open questions (to settle before building Feature 2)

- Geospatial endpoint host: FastAPI vs PostgREST vs a module beside ALS.
- `position_report` geometry: live view vs. trigger-maintained mirror (driven by
  indexed nearest-neighbor needs at fleet scale).
- Truck-stops: imported reference data (and from where) vs. HERE POI lookups at
  request time.
- High-rate position ingest: via ALS vs. a dedicated intake worker.

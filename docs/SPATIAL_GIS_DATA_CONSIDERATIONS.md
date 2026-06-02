# Spatial / GIS Data Considerations

Detailed considerations behind the **decision to separate PostGIS spatial data
from the ALS APIs**. This is the "why / details / gotchas" companion to
[`GEOSPATIAL.md`](GEOSPATIAL.md) (the concise decision + strategy). Read that
first for the summary and the two-path diagram.

> Decision: **separate** spatial (PostGIS) data from ALS — relational map data
> via ALS (plain lat/lng), spatial queries via a dedicated endpoint over a `gis`
> schema. Accepted as more to stand up, but the right long-term shape.

---

## 1. Why ALS and PostGIS conflict (in detail)

### 1.1 PostGIS metadata pollutes ALS reflection
`CREATE EXTENSION postgis` installs, into its target schema:
- `spatial_ref_sys` — a real **table** (~8500 SRID rows).
- `geometry_columns`, `geography_columns` — **views**.
- `raster_columns`, `raster_overviews` — views (if raster is enabled).

ApiLogicServer's `create` reflects tables/views and generates a JSON:API
resource per object. Left in `public`, these become junk endpoints and are the
**naming conflicts** seen in past projects. Mitigation: install PostGIS in its
own `gis` schema and **do not** include `gis` in ALS reflection.

### 1.2 Geometry types have no JSON:API mapping
`geometry` / `geography` columns are not understood by plain SQLAlchemy/SAFRS;
they need **GeoAlchemy2**, and even then the value is WKB/EWKB — not something
JSON:API should emit. A geometry column on an ALS-managed table therefore breaks
reflection or serialization. Mitigation: **no geometry columns on ALS tables**;
store `lat`/`lng` as `double precision` and derive geometry in `gis`.

### 1.3 Spatial work is functional, not CRUD
`ST_DWithin`, `ST_Distance`, KNN nearest-neighbor, route geometry, geofencing —
these are query operations, not the row CRUD ALS auto-generates. They belong
behind a purpose-built endpoint.

---

## 2. Data model considerations

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

## 3. Geometry derivation & indexing (in `gis`)

Geometry is **derived from the ALS lat/lng — never hand-duplicated**:

| Approach                         | Sync        | Indexable | Use when                                   |
| -------------------------------- | ----------- | --------- | ------------------------------------------ |
| `gis` **view** (`ST_MakePoint`)  | always live | no        | simple reads, low volume                   |
| **materialized view**            | on refresh  | yes       | periodic refresh acceptable                |
| **mirror table + trigger**       | live        | yes (GiST)| indexed nearest-neighbor at fleet scale    |

- `ST_MakePoint` / `ST_SetSRID` are immutable, so a `STORED` generated geometry
  column is possible — but only on a `gis`-schema table, never an ALS one.
- Index geometry with **GiST** (or SP-GiST); use the `<->` KNN operator for
  "nearest truck stop / trucks near X".

---

## 4. The geospatial endpoint

- **Responsibility:** all PostGIS `ST_*` queries (nearest, within, route
  geometry, geofence). The only component that imports GeoAlchemy2 / raw PostGIS.
- **Host (leaning):** a small **FastAPI** service over the `gis` schema;
  alternatives: PostgREST, or a thin module beside ALS.
- **DB access:** its **own read-mostly DB role**, granted on `gis` (+ read on the
  `telemetry` lat/lng tables it derives from). ALS keeps its own role with **no**
  rights on `gis`. Least privilege both ways.
- **Contract:** returns plain JSON (GeoJSON where useful) so clients don't need a
  spatial client library for simple cases.

---

## 5. Routing provider (HERE) considerations

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

## 6. Ingestion (source-agnostic positions)

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

## 7. Ops, security, backup

- **PostGIS version pinning:** the DB image must include PostGIS (the plain
  `postgres:16` image does **not**); use `postgis/postgis` or install the
  extension. CI/test runners that spin up Postgres need it too.
- **Extension upgrades:** `ALTER EXTENSION postgis UPDATE` on version bumps.
- **Backup/restore:** `spatial_ref_sys` and `gis` objects are part of the dump;
  large `bytea` documents (CMS) and high-volume `position_report` affect backup
  size — consider separate backup cadences per schema.
- **Search path:** set `search_path` so unqualified calls resolve; or always
  schema-qualify (`gis.ST_*` is not needed if `gis` is on the path).

---

## 8. Standing it up (bootstrap, to be scripted with Feature 2)

```sql
CREATE SCHEMA IF NOT EXISTS gis;
CREATE EXTENSION IF NOT EXISTS postgis SCHEMA gis;
-- telemetry tables (lat/lng) created via the normal schema.sql (ALS-facing)
-- gis views / mirror tables + GiST indexes created here (NOT reflected by ALS)
-- ALS: generate with the app/cms/telemetry schemas only, excluding gis
```
A compose/VCP entry for the geospatial endpoint and the `gis` bootstrap SQL will
be added when Feature 2 is built, so a fresh environment comes up repeatably.

---

## 9. Open questions (carried from GEOSPATIAL.md)

- Geospatial endpoint host: FastAPI vs PostgREST vs module beside ALS.
- `position_report` geometry: live view vs. trigger-maintained mirror (driven by
  indexed nearest-neighbor needs at fleet scale).
- Truck-stops: imported reference data (and from where) vs. HERE POI lookups at
  request time.
- High-rate position ingest: via ALS vs. a dedicated intake worker.

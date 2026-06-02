# Geospatial strategy — ALS ↔ PostGIS

> Status: **DECIDED (2026-06-02).** Spatial (PostGIS) data is separated from the
> ALS APIs — relational map data flows through ALS, spatial queries through a
> dedicated endpoint over a `gis` schema. Accepted that this is more to stand up;
> it's the right long-term shape. Pairs with `MIDDLEWARE_SETUP.md` → "Database
> schema layout". Remaining items are the sub-choices under "Open questions".

## The concern

ApiLogicServer reflects the database (SQLAlchemy → SAFRS) to auto-generate the
JSON:API. PostGIS fights that in three ways:

1. **PostGIS metadata pollutes reflection.** `CREATE EXTENSION postgis` creates
   `spatial_ref_sys` (table) and `geometry_columns` / `geography_columns`
   (views). If they live in a schema ALS reflects, ALS generates junk resources
   for them — the naming/endpoint conflicts seen before.
2. **Geometry types don't map.** `geometry` / `geography` columns have no native
   SQLAlchemy/JSON:API representation (would need GeoAlchemy2); a geometry column
   on an ALS-managed table breaks reflection or serialization.
3. **Spatial ops are functional, not CRUD.** `ST_DWithin`, nearest-truck-stop,
   route geometry, geofencing — these are queries, not the row CRUD ALS emits.

## Principle: two access paths

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
                            │ app/cms/telemetry schemas             │ gis schema
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

## Database layout

- Install PostGIS into its **own schema**, so its metadata never reaches the app:
  ```sql
  CREATE SCHEMA IF NOT EXISTS gis;
  CREATE EXTENSION IF NOT EXISTS postgis SCHEMA gis;
  ```
- **ALS reflects `app` / `cms` / `telemetry` only — never `gis`.** (Pass the
  schema list to `ApiLogicServer create`; keep `gis` and `spatial_ref_sys` out.)
- Geometry is **derived** from the lat/lng on the ALS tables — never duplicated
  by hand:
  - **View (simplest):** `gis.position_geog AS SELECT id, equipment_id,
    recorded_at, ST_SetSRID(ST_MakePoint(lng, lat), 4326)::geography AS geog
    FROM telemetry.position_report;` — zero storage, always in sync; cannot be
    indexed.
  - **Mirror table + trigger (when you need a GiST index):** a `gis.*` table
    with a `geography(Point,4326)` column and a GiST index, kept current by a
    trigger on insert/update. Use this for nearest-neighbor (nearest truck stop,
    trucks within radius).
  - `ST_MakePoint`/`ST_SetSRID` are immutable, so a `STORED` generated geometry
    column is technically possible — but only on a `gis`-schema table, never on
    an ALS-reflected one.

## What goes where

| Resource                         | Schema      | Via ALS? | Geometry?                         |
| -------------------------------- | ----------- | -------- | --------------------------------- |
| `trip`, `trip_stop` / waypoint   | telemetry   | ✅ yes   | lat/lng numeric                   |
| `point_of_interest`, `truck_stop`| telemetry   | ✅ yes   | lat/lng numeric                   |
| `route` (metadata, HERE polyline)| telemetry   | ✅ yes   | encoded polyline as text          |
| `position_report`                | telemetry   | ✅ yes   | lat/lng numeric                   |
| `location_source` (lookup)       | telemetry   | ✅ yes   | —                                 |
| spatial mirrors / views, indexes | `gis`       | ❌ no    | PostGIS geometry/geography + GiST |
| PostGIS extension + metadata     | `gis`       | ❌ no    | (engine internals)                |
| Truck-legal routing              | external    | —        | HERE API                          |

## Consequences

- ALS stays clean: no geometry types, no PostGIS internals, no naming clashes.
- The spatial endpoint is the only place that needs GeoAlchemy2 / raw PostGIS.
- A polyline/route from HERE is stored as text on an ALS `route` row (fine), and
  optionally materialized as geometry in `gis` for spatial ops.
- Clients use two endpoints for maps: JSON:API (data) + the spatial endpoint
  (queries). The HUD reads latest `position_report` per rig via JSON:API and, if
  it needs "trucks near X", asks the spatial endpoint.
- **Standup cost (accepted):** one extra deployable (the geospatial endpoint)
  and a `gis` schema bootstrap (`CREATE EXTENSION postgis SCHEMA gis;` + views/
  mirror + GiST). The bootstrap SQL and a compose/VCP entry for the endpoint
  will be added when Feature 2 is built, so a fresh environment comes up
  repeatably.

## Open questions (to settle before building Feature 2)

- Geospatial endpoint host: a small FastAPI/Flask service, PostgREST, or a
  thin extension alongside ALS? (Leaning: minimal FastAPI service over the `gis`
  schema.)
- View vs. trigger-maintained mirror for `position_report` geometry — driven by
  whether we need indexed nearest-neighbor at fleet scale.
- Whether truck-stops are reference data we import (and from where) vs. HERE POI
  lookups at request time.

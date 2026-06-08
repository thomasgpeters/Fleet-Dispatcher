-- Fleet Dispatcher — PostGIS (gis) bootstrap
--
-- Installs PostGIS into its OWN schema so its metadata never lands in the schema
-- ApiLogicServer reflects. This is the "PostGIS without breaking ALS" step (see
-- docs/SPATIAL_GIS_DATA_CONSIDERATIONS.md and docs/DEPLOYMENT.md).
--
-- Run AFTER schema.sql (it derives geometry from telemetry.position_report,
-- which here lives in `public`). Requires the PostGIS package, e.g.
--   sudo apt-get install postgresql-16-postgis-3
--
--   psql "$DATABASE_URL" -f database/gis_bootstrap.sql

BEGIN;

-- PostGIS in its own schema → gis.spatial_ref_sys, gis.geometry_columns, etc.
-- (NOT in public, so ALS won't reflect them).
CREATE SCHEMA IF NOT EXISTS gis;
CREATE EXTENSION IF NOT EXISTS postgis SCHEMA gis;

-- Derived geometry for truck positions: a live view (zero storage, always in
-- sync). Swap to a trigger-maintained mirror table + GiST index if you need
-- indexed nearest-neighbour at fleet scale (see the considerations doc).
-- gis.* functions are schema-qualified so this works regardless of search_path.
CREATE OR REPLACE VIEW gis.position_geog AS
    SELECT
        pr.id,
        pr.equipment_id,
        pr.driver_id,
        pr.recorded_at,
        gis.ST_SetSRID(gis.ST_MakePoint(pr.lng, pr.lat), 4326)::gis.geography AS geog
    FROM public.position_report pr;

-- Example POI geometry view (nearest-truck-stop queries hit this).
CREATE OR REPLACE VIEW gis.poi_geog AS
    SELECT
        p.id,
        p.name,
        p.poi_category_id,
        gis.ST_SetSRID(gis.ST_MakePoint(p.lng, p.lat), 4326)::gis.geography AS geog
    FROM public.point_of_interest p;

COMMIT;

-- The geospatial endpoint (NOT ALS) queries these views, e.g.:
--   SELECT id FROM gis.poi_geog
--   ORDER BY geog <-> gis.ST_SetSRID(gis.ST_MakePoint(:lng,:lat),4326)::gis.geography
--   LIMIT 5;

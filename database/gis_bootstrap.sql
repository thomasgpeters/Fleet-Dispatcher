-- Fleet Dispatcher — PostGIS (gis) bootstrap
--
-- Installs PostGIS into a `gis` schema so its metadata never lands in a schema
-- ApiLogicServer reflects ("PostGIS without breaking ALS" — see
-- docs/SPATIAL_GIS_DATA_CONSIDERATIONS.md and docs/DEPLOYMENT.md).
--
-- Shared instance: this database also hosts Smitty-Services and
-- Student-Onboarding. `gis` is a SHARED schema, so we do NOT take it over — we
-- just ensure it exists, are granted CREATE on it, and OWN our own derived
-- views via the `fleet_gis` role. The PostGIS *extension* objects
-- (gis.spatial_ref_sys, gis.ST_* functions) are owned by the installing
-- superuser (by PostGIS design); fleet_gis uses them (EXECUTE is granted to
-- PUBLIC).
--
-- Fleet's app tables live in the `fleet` schema (see schema.sql); these views
-- derive geometry from fleet.position_report / fleet.point_of_interest.
--
-- MUST be run by a SUPERUSER (CREATE EXTENSION postgis requires it):
--   sudo -u postgres psql -d <db> -f database/gis_bootstrap.sql
--   (or via scripts/db-setup.sh, which runs this step as admin)
-- Run AFTER schema.sql.

BEGIN;

-- The gis owner role for Fleet's spatial objects (LOGIN; dev password — change
-- it for real deployments).
DO $do$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'fleet_gis') THEN
        CREATE ROLE fleet_gis LOGIN PASSWORD 'fleet_gis';
    END IF;
END
$do$;

-- Shared gis schema + PostGIS. Don't reassign ownership (other apps may share
-- it); just make sure fleet_gis can create its views there.
CREATE SCHEMA IF NOT EXISTS gis;
CREATE EXTENSION IF NOT EXISTS postgis SCHEMA gis;
GRANT USAGE, CREATE ON SCHEMA gis TO fleet_gis;

-- fleet_gis reads Fleet's lat/lng tables and resolves PostGIS operators
-- (<->, &&) by having gis on its search_path.
GRANT USAGE ON SCHEMA fleet TO fleet_gis;
GRANT SELECT ON fleet.position_report, fleet.point_of_interest,
                fleet.poi_category TO fleet_gis;
ALTER ROLE fleet_gis SET search_path = gis, fleet, public;

-- Create the derived views AS fleet_gis so it owns them. Geometry is derived
-- from Fleet's lat/lng tables (no duplicated data, always in sync). Swap to a
-- trigger-maintained mirror table + GiST index for indexed nearest-neighbour at
-- fleet scale (see the considerations doc).
SET ROLE fleet_gis;

CREATE OR REPLACE VIEW gis.fleet_position_geog AS
    SELECT
        pr.id,
        pr.equipment_id,
        pr.driver_id,
        pr.recorded_at,
        gis.ST_SetSRID(gis.ST_MakePoint(pr.lng, pr.lat), 4326)::gis.geography AS geog
    FROM fleet.position_report pr;

CREATE OR REPLACE VIEW gis.fleet_poi_geog AS
    SELECT
        p.id,
        p.name,
        p.poi_category_id,
        gis.ST_SetSRID(gis.ST_MakePoint(p.lng, p.lat), 4326)::gis.geography AS geog
    FROM fleet.point_of_interest p;

RESET ROLE;

COMMIT;

-- The geospatial endpoint connects AS fleet_gis (search_path preset) and queries
-- these views, e.g.:
--   SELECT id FROM gis.fleet_poi_geog
--   ORDER BY geog <-> ST_SetSRID(ST_MakePoint(:lng,:lat),4326)::geography
--   LIMIT 5;

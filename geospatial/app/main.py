"""Fleet Dispatcher — geospatial endpoint.

The spatial half of Feature 2: PostGIS ``ST_*`` queries over the ``gis`` schema,
kept OFF the ApiLogicServer path (see docs/SPATIAL_GIS_DATA_CONSIDERATIONS.md).
Connects as the ``fleet_gis`` role, which owns the gis views and has
``search_path = gis, fleet, public`` preset (so operators like ``<->`` resolve).

Run:
    GIS_DATABASE_URL=postgresql://fleet_gis:fleet_gis@localhost:5432/fleet_dispatcher \\
        uvicorn app.main:app --host 0.0.0.0 --port 5701
"""

from __future__ import annotations

import os
from contextlib import contextmanager
from typing import Any, Iterator

import psycopg
from psycopg.rows import dict_row
from fastapi import FastAPI, Query

GIS_DATABASE_URL = os.environ.get(
    "GIS_DATABASE_URL",
    "postgresql://fleet_gis:fleet_gis@localhost:5432/fleet_dispatcher",
)

app = FastAPI(title="Fleet Dispatcher — Geospatial endpoint")


@contextmanager
def cursor() -> Iterator[psycopg.Cursor[dict[str, Any]]]:
    """A short-lived connection/cursor as fleet_gis (search_path preset by role;
    re-asserted here for safety)."""
    with psycopg.connect(GIS_DATABASE_URL, autocommit=True, row_factory=dict_row) as conn:
        with conn.cursor() as cur:
            cur.execute("SET search_path = gis, fleet, public")
            yield cur


@app.get("/health")
def health() -> dict[str, Any]:
    try:
        with cursor() as cur:
            cur.execute("SELECT gis.postgis_version() AS postgis")
            row = cur.fetchone() or {}
        return {"status": "ok", "postgis": row.get("postgis")}
    except Exception as exc:  # surface DB/connection problems plainly
        return {"status": "error", "detail": str(exc)}


@app.get("/truck-stops/nearest")
def nearest_truck_stops(
    lat: float = Query(..., ge=-90, le=90),
    lng: float = Query(..., ge=-180, le=180),
    limit: int = Query(5, ge=1, le=50),
) -> list[dict[str, Any]]:
    """Nearest truck-stop POIs to a point (KNN via the ``<->`` operator)."""
    sql = """
        SELECT g.id, g.name,
               round(ST_Distance(
                   g.geog,
                   ST_SetSRID(ST_MakePoint(%(lng)s, %(lat)s), 4326)::geography
               ) / 1609.34) AS miles
        FROM gis.fleet_poi_geog g
        JOIN fleet.poi_category c ON c.id = g.poi_category_id
        WHERE c.code = 'truck_stop'
        ORDER BY g.geog <-> ST_SetSRID(ST_MakePoint(%(lng)s, %(lat)s), 4326)::geography
        LIMIT %(limit)s
    """
    with cursor() as cur:
        cur.execute(sql, {"lat": lat, "lng": lng, "limit": limit})
        return cur.fetchall()


@app.get("/trucks/near")
def trucks_near(
    lat: float = Query(..., ge=-90, le=90),
    lng: float = Query(..., ge=-180, le=180),
    radius_m: float = Query(50000, gt=0),
) -> list[dict[str, Any]]:
    """Latest position per rig within ``radius_m`` metres of a point."""
    sql = """
        SELECT DISTINCT ON (equipment_id)
               equipment_id,
               recorded_at,
               ST_Y(geog::geometry) AS lat,
               ST_X(geog::geometry) AS lng,
               round(ST_Distance(
                   geog,
                   ST_SetSRID(ST_MakePoint(%(lng)s, %(lat)s), 4326)::geography
               )) AS meters
        FROM gis.fleet_position_geog
        WHERE equipment_id IS NOT NULL
          AND ST_DWithin(
                  geog,
                  ST_SetSRID(ST_MakePoint(%(lng)s, %(lat)s), 4326)::geography,
                  %(radius_m)s)
        ORDER BY equipment_id, recorded_at DESC
    """
    with cursor() as cur:
        cur.execute(sql, {"lat": lat, "lng": lng, "radius_m": radius_m})
        return cur.fetchall()


@app.get("/positions/latest")
def latest_positions() -> list[dict[str, Any]]:
    """Latest known position per rig (fleet-wide)."""
    sql = """
        SELECT DISTINCT ON (equipment_id)
               equipment_id,
               recorded_at,
               ST_Y(geog::geometry) AS lat,
               ST_X(geog::geometry) AS lng
        FROM gis.fleet_position_geog
        WHERE equipment_id IS NOT NULL
        ORDER BY equipment_id, recorded_at DESC
    """
    with cursor() as cur:
        cur.execute(sql)
        return cur.fetchall()

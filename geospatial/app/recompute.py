"""Recompute a trip's route from its (ordered) waypoints and upsert fleet.route.

Reads/writes the `fleet` schema, so it uses FLEET_DATABASE_URL (the fleet app
role with write on fleet.route) — distinct from the read-only fleet_gis role the
spatial endpoints use.
"""

from __future__ import annotations

import logging
import os
from typing import Optional

import psycopg
from psycopg.rows import dict_row

from .routing import compute

log = logging.getLogger("fleet.geospatial.recompute")

FLEET_DATABASE_URL = os.environ.get(
    "FLEET_DATABASE_URL",
    "postgresql://fleet:fleet@localhost:5432/fleet_dispatcher",
)


def recompute_route(trip_id: str) -> Optional[dict]:
    """Recompute and persist the route for one trip. Returns the route dict or
    None when the trip has fewer than two waypoints."""
    with psycopg.connect(FLEET_DATABASE_URL, autocommit=True, row_factory=dict_row) as conn:
        with conn.cursor() as cur:
            cur.execute("SET search_path = fleet, public")
            cur.execute(
                "SELECT lat, lng FROM waypoint WHERE trip_id = %s ORDER BY seq",
                (trip_id,),
            )
            rows = cur.fetchall()
            if len(rows) < 2:
                return None
            stops = [(r["lat"], r["lng"]) for r in rows]
            r = compute(stops)

            # One route per trip: replace any existing.
            cur.execute("DELETE FROM route WHERE trip_id = %s", (trip_id,))
            cur.execute(
                """
                INSERT INTO route (trip_id, origin_lat, origin_lng, dest_lat,
                                   dest_lng, polyline, distance_mi, drive_minutes,
                                   provider)
                VALUES (%(trip_id)s, %(olat)s, %(olng)s, %(dlat)s, %(dlng)s,
                        %(poly)s, %(dist)s, %(mins)s, %(prov)s)
                """,
                {
                    "trip_id": trip_id,
                    "olat": stops[0][0], "olng": stops[0][1],
                    "dlat": stops[-1][0], "dlng": stops[-1][1],
                    "poly": r["polyline"],
                    "dist": r["distance_mi"],
                    "mins": r["drive_minutes"],
                    "prov": r["provider"],
                },
            )
    log.info("recomputed route for trip %s: %s mi", trip_id, r["distance_mi"])
    return r

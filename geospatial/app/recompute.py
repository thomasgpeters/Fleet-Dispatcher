"""Recompute a trip's route from its waypoints and upsert fleet.route.

If the trip is NOT route_locked, first reorder the stops with the interim
nearest-neighbor optimizer (persisting the new waypoint seq); a locked trip keeps
the driver's manual order (geometry-only recompute). Reads/writes the `fleet`
schema, so it uses FLEET_DATABASE_URL (the fleet app role) — distinct from the
read-only fleet_gis role the spatial endpoints use.

Writes go straight to the DB (psycopg), not through ALS, so reordering does NOT
emit Kafka events — no recompute feedback loop.
"""

from __future__ import annotations

import logging
import os
from typing import Optional

import psycopg
from psycopg.rows import dict_row

from .optimize import nearest_neighbor_order
from .routing import compute

log = logging.getLogger("fleet.geospatial.recompute")

FLEET_DATABASE_URL = os.environ.get(
    "FLEET_DATABASE_URL",
    "postgresql://fleet:fleet@localhost:5432/fleet_dispatcher",
)


def _persist_order(cur, ordered) -> None:
    """Write the new seq (1..N) two-phase to respect UNIQUE(trip_id, seq)."""
    for i, w in enumerate(ordered):
        cur.execute("UPDATE waypoint SET seq = %s WHERE id = %s", (1000 + i, w["id"]))
    for i, w in enumerate(ordered):
        cur.execute("UPDATE waypoint SET seq = %s WHERE id = %s", (i + 1, w["id"]))


def recompute_route(trip_id: str) -> Optional[dict]:
    """Recompute and persist the route for one trip. Returns the route dict, or
    None when the trip has fewer than two waypoints."""
    with psycopg.connect(FLEET_DATABASE_URL, row_factory=dict_row) as conn:
        with conn.cursor() as cur:
            cur.execute("SET search_path = fleet, public")

            cur.execute("SELECT route_locked FROM trip WHERE id = %s", (trip_id,))
            trip = cur.fetchone()
            locked = bool(trip and trip["route_locked"])

            cur.execute(
                "SELECT id, stop_type_id, lat, lng FROM waypoint "
                "WHERE trip_id = %s ORDER BY seq",
                (trip_id,),
            )
            rows = cur.fetchall()
            if len(rows) < 2:
                conn.commit()
                return None

            ordered = rows
            if not locked:
                # Interim auto-optimize (nearest neighbor; no key needed). The
                # full capacitated PDP solver replaces this when its engine lands.
                optimized = nearest_neighbor_order(rows)
                if [w["id"] for w in optimized] != [w["id"] for w in rows]:
                    _persist_order(cur, optimized)
                    ordered = optimized

            stops = [(w["lat"], w["lng"]) for w in ordered]
            r = compute(stops)

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
        conn.commit()
    log.info(
        "recomputed route for trip %s: %s mi%s",
        trip_id, r["distance_mi"], "" if locked else " (auto-ordered)",
    )
    return r

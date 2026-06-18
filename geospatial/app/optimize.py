"""Interim route optimizer: nearest-neighbor stop ordering.

A no-dependency, no-API-key heuristic so "auto-optimize" works out of the box
(e.g. when the client hasn't provisioned a HERE/routing key). It anchors the
fixed endpoints — origin first, destination last — and greedily visits the
nearest remaining intermediate stop.

Scope/limits (intentional, for the interim): minimizes great-circle distance
only; it does NOT enforce multi-load pickup-before-dropoff precedence or trailer
capacity (deck feet / weight). Those are the full capacitated pickup-and-delivery
solver — the deferred engine (OR-Tools / HERE Tour Planning). See docs/TODO.md.
"""

from __future__ import annotations

from typing import Any, Dict, List

from .routing import haversine_miles

ORIGIN = 1
DESTINATION = 2


def nearest_neighbor_order(stops: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """Reorder waypoint dicts (need id, stop_type_id, lat, lng) by nearest
    neighbor, keeping origin first and destination last. Returns a new list."""
    if len(stops) < 3:
        return list(stops)  # nothing meaningful to reorder

    origin = next((s for s in stops if s["stop_type_id"] == ORIGIN), stops[0])
    dest = next(
        (s for s in reversed(stops) if s["stop_type_id"] == DESTINATION), stops[-1]
    )
    if dest is origin:
        return list(stops)

    middle = [s for s in stops if s is not origin and s is not dest]
    ordered = [origin]
    cur = origin
    remaining = list(middle)
    while remaining:
        nxt = min(
            remaining,
            key=lambda s: haversine_miles((cur["lat"], cur["lng"]), (s["lat"], s["lng"])),
        )
        ordered.append(nxt)
        remaining.remove(nxt)
        cur = nxt
    ordered.append(dest)
    return ordered

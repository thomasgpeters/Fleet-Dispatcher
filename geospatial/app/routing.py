"""Route computation for a sequence of stops.

Dev provider: great-circle (haversine) distance summed over the ordered stops,
with a nominal average speed for drive time. Swap in a real road-network router
(HERE — the `route.provider` the schema already names) by setting
ROUTING_PROVIDER=here and implementing `_here` (needs HERE_API_KEY); the rest of
the pipeline (recompute → upsert route) is unchanged.
"""

from __future__ import annotations

import math
import os
from typing import List, Tuple

Stop = Tuple[float, float]  # (lat, lng)

_EARTH_MI = 3958.7613
_AVG_MPH = float(os.environ.get("ROUTING_AVG_MPH", "50"))
PROVIDER = os.environ.get("ROUTING_PROVIDER", "haversine")


def haversine_miles(a: Stop, b: Stop) -> float:
    (lat1, lng1), (lat2, lng2) = a, b
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlam = math.radians(lng2 - lng1)
    h = math.sin(dphi / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlam / 2) ** 2
    return 2 * _EARTH_MI * math.asin(math.sqrt(h))


def _haversine_route(stops: List[Stop]) -> dict:
    distance = sum(haversine_miles(stops[i], stops[i + 1]) for i in range(len(stops) - 1))
    minutes = int(round(distance / _AVG_MPH * 60)) if _AVG_MPH > 0 else 0
    # Placeholder polyline: the ordered stop coords. A road router returns an
    # encoded polyline here instead.
    polyline = ";".join(f"{lat:.5f},{lng:.5f}" for lat, lng in stops)
    return {
        "distance_mi": round(distance, 1),
        "drive_minutes": minutes,
        "polyline": polyline,
        "provider": "haversine",
    }


def compute(stops: List[Stop]) -> dict:
    """Distance/time/polyline for an ordered list of stops."""
    if len(stops) < 2:
        return {"distance_mi": 0.0, "drive_minutes": 0, "polyline": "", "provider": PROVIDER}
    # if PROVIDER == "here": return _here(stops)   # TODO: real road routing
    return _haversine_route(stops)

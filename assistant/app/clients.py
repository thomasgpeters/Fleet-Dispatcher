"""Thin HTTP clients the assistant tools call: ALS JSON:API and HERE routing."""

from __future__ import annotations

import math
from typing import Any

import httpx

from .config import config


# --- ApiLogicServer (messaging) ---------------------------------------------

def post_message(channel_id: str, author_id: str, body: str) -> dict[str, Any]:
    """Post a message to the dispatcher channel via the JSON:API."""
    url = f"{config.fleet_api_base_url}/Message"
    payload = {
        "data": {
            "type": "Message",
            "attributes": {
                "channel_id": channel_id,
                "author_id": author_id,
                "body": body,
            },
        }
    }
    resp = httpx.post(
        url,
        json=payload,
        headers={
            "Content-Type": "application/vnd.api+json",
            "Accept": "application/vnd.api+json",
        },
        timeout=15,
    )
    resp.raise_for_status()
    return resp.json()


# --- HERE truck routing ------------------------------------------------------

def here_routes(
    origin: tuple[float, float],
    destination: tuple[float, float],
    *,
    truck: dict[str, float] | None = None,
    alternatives: int = 0,
) -> list[dict[str, Any]] | None:
    """Truck-aware routes from HERE. Returns a list of
    {duration_s, length_m, notices[]} or None if HERE isn't configured.

    NOTE: HERE Routing v8. truck[height] is centimetres, truck[grossWeight] is
    kilograms. Not exercised in the dev sandbox — verify against your HERE plan.
    """
    if not config.here_api_key:
        return None
    params: dict[str, str] = {
        "transportMode": "truck",
        "origin": f"{origin[0]},{origin[1]}",
        "destination": f"{destination[0]},{destination[1]}",
        "return": "summary",
        "apiKey": config.here_api_key,
    }
    if alternatives:
        params["alternatives"] = str(alternatives)
    if truck:
        if truck.get("height_m"):
            params["truck[height]"] = str(int(truck["height_m"] * 100))
        if truck.get("weight_kg"):
            params["truck[grossWeight]"] = str(int(truck["weight_kg"]))

    resp = httpx.get("https://router.hereapi.com/v8/routes", params=params, timeout=15)
    resp.raise_for_status()
    data = resp.json()

    out: list[dict[str, Any]] = []
    for route in data.get("routes", []):
        duration = 0
        length = 0
        notices: list[str] = []
        for section in route.get("sections", []):
            summ = section.get("summary", {})
            duration += summ.get("duration", 0)
            length += summ.get("length", 0)
            for n in section.get("notices", []):
                notices.append(n.get("title") or n.get("code") or "restriction")
        out.append({"duration_s": duration, "length_m": length, "notices": notices})
    return out


# --- helpers -----------------------------------------------------------------

def haversine_miles(a: tuple[float, float], b: tuple[float, float]) -> float:
    """Great-circle distance in miles (fallback when HERE isn't configured)."""
    r = 3958.8
    lat1, lng1, lat2, lng2 = map(math.radians, [a[0], a[1], b[0], b[1]])
    dlat, dlng = lat2 - lat1, lng2 - lng1
    h = math.sin(dlat / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlng / 2) ** 2
    return r * 2 * math.asin(math.sqrt(h))


def human_duration(seconds: float) -> str:
    minutes = int(round(seconds / 60))
    h, m = divmod(minutes, 60)
    if h and m:
        return f"{h} hour{'s' if h != 1 else ''} {m} minute{'s' if m != 1 else ''}"
    if h:
        return f"{h} hour{'s' if h != 1 else ''}"
    return f"{m} minute{'s' if m != 1 else ''}"

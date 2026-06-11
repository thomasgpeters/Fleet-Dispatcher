"""Tool schemas + server-side handlers for the Hey Dispatch assistant.

Tool *inputs* come from Claude; the driver/channel/position/truck context comes
from the request (never from the model). Each handler returns
(content_for_model, action_record). The tool list order is stable so it caches.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable

from . import clients
from . import knowledge


@dataclass
class AssistContext:
    """Server-trusted context for one assist call (not model-controlled)."""

    driver_id: str | None = None
    driver_name: str | None = None
    author_user_id: str | None = None       # app_user id for posting messages
    dispatcher_channel_id: str | None = None
    lat: float | None = None
    lng: float | None = None
    dest_lat: float | None = None
    dest_lng: float | None = None
    dest_label: str | None = None
    truck_height_m: float | None = None
    truck_weight_kg: float | None = None
    extra: dict[str, Any] = field(default_factory=dict)


# JSON:API resource type names → tool schemas (stable order for prompt caching).
TOOLS: list[dict[str, Any]] = [
    {
        "name": "send_message_to_dispatcher",
        "description": "Send a short message to the driver's dispatcher on their "
                       "behalf. Use for status updates, ETAs, problems, or requests.",
        "input_schema": {
            "type": "object",
            "properties": {
                "text": {"type": "string", "description": "The message to send."}
            },
            "required": ["text"],
        },
    },
    {
        "name": "get_eta",
        "description": "Estimate time of arrival and distance to the destination or "
                       "next stop from the driver's current position.",
        "input_schema": {
            "type": "object",
            "properties": {
                "destination": {
                    "type": "string",
                    "description": "Optional override; defaults to the active stop.",
                }
            },
        },
    },
    {
        "name": "validate_route",
        "description": "Check that the current route to the destination is legal for "
                       "this truck (height, weight, bridge clearances).",
        "input_schema": {"type": "object", "properties": {}},
    },
    {
        "name": "alternate_routes",
        "description": "Suggest alternate routes to the destination, with drive times.",
        "input_schema": {
            "type": "object",
            "properties": {
                "count": {"type": "integer", "minimum": 1, "maximum": 3, "default": 2}
            },
        },
    },
    {
        "name": "tech_help",
        "description": "Answer an equipment or app question (DEF, regen, tires, ELD, "
                       "inspection, login, etc.).",
        "input_schema": {
            "type": "object",
            "properties": {
                "topic": {"type": "string", "description": "What the driver asked about."}
            },
            "required": ["topic"],
        },
    },
]


# --- handlers ----------------------------------------------------------------

def _ok(tool: str, detail: str) -> dict[str, Any]:
    return {"tool": tool, "ok": True, "detail": detail}


def _err(tool: str, detail: str) -> dict[str, Any]:
    return {"tool": tool, "ok": False, "detail": detail}


def _send_message(args: dict, ctx: AssistContext):
    text = (args.get("text") or "").strip()
    if not text:
        return "No message text provided.", _err("send_message_to_dispatcher", "empty text")
    if not ctx.dispatcher_channel_id or not ctx.author_user_id:
        return ("No dispatcher channel is configured for this driver.",
                _err("send_message_to_dispatcher", "missing channel/author"))
    try:
        clients.post_message(ctx.dispatcher_channel_id, ctx.author_user_id, text)
        return "Message delivered to dispatch.", _ok("send_message_to_dispatcher", text)
    except Exception as e:  # surface to the model so it can recover verbally
        return f"Failed to send: {e}", _err("send_message_to_dispatcher", str(e))


def _dest(ctx: AssistContext) -> tuple[float, float] | None:
    if ctx.dest_lat is not None and ctx.dest_lng is not None:
        return (ctx.dest_lat, ctx.dest_lng)
    return None


def _origin(ctx: AssistContext) -> tuple[float, float] | None:
    if ctx.lat is not None and ctx.lng is not None:
        return (ctx.lat, ctx.lng)
    return None


def _truck(ctx: AssistContext) -> dict[str, float]:
    t: dict[str, float] = {}
    if ctx.truck_height_m:
        t["height_m"] = ctx.truck_height_m
    if ctx.truck_weight_kg:
        t["weight_kg"] = ctx.truck_weight_kg
    return t


def _get_eta(args: dict, ctx: AssistContext):
    origin, dest = _origin(ctx), _dest(ctx)
    if not origin:
        return "I don't have your current location yet.", _err("get_eta", "no origin")
    if not dest:
        return "I don't have a destination set.", _err("get_eta", "no destination")
    routes = clients.here_routes(origin, dest, truck=_truck(ctx))
    if routes:
        r = routes[0]
        miles = r["length_m"] / 1609.34
        msg = f"About {clients.human_duration(r['duration_s'])}, {miles:.0f} miles."
        return msg, _ok("get_eta", msg)
    # Fallback estimate (no HERE configured).
    miles = clients.haversine_miles(origin, dest)
    minutes = miles / 55.0 * 60.0
    msg = f"Roughly {clients.human_duration(minutes * 60)}, about {miles:.0f} miles (estimate)."
    return msg, _ok("get_eta", msg)


def _validate_route(args: dict, ctx: AssistContext):
    origin, dest = _origin(ctx), _dest(ctx)
    if not origin or not dest:
        return "I need your position and destination to check the route.", _err("validate_route", "missing coords")
    routes = clients.here_routes(origin, dest, truck=_truck(ctx))
    if routes is None:
        return ("Route validation needs the routing provider, which isn't set up yet.",
                _err("validate_route", "HERE not configured"))
    notices = routes[0]["notices"] if routes else []
    if notices:
        msg = "Heads up — this route has restrictions: " + "; ".join(notices[:3]) + "."
        return msg, _ok("validate_route", msg)
    return "Route looks clear for your truck.", _ok("validate_route", "clear")


def _alternate_routes(args: dict, ctx: AssistContext):
    origin, dest = _origin(ctx), _dest(ctx)
    if not origin or not dest:
        return "I need your position and destination to find alternates.", _err("alternate_routes", "missing coords")
    count = int(args.get("count", 2))
    routes = clients.here_routes(origin, dest, truck=_truck(ctx), alternatives=count)
    if not routes:
        return ("Alternate routing needs the routing provider, which isn't set up yet.",
                _err("alternate_routes", "HERE not configured"))
    parts = [
        f"Option {i + 1}: {clients.human_duration(r['duration_s'])}, {r['length_m'] / 1609.34:.0f} miles"
        for i, r in enumerate(routes[: count + 1])
    ]
    msg = "; ".join(parts) + "."
    return msg, _ok("alternate_routes", msg)


def _tech_help(args: dict, ctx: AssistContext):
    answer = knowledge.lookup(args.get("topic", ""))
    return answer, _ok("tech_help", args.get("topic", ""))


_HANDLERS: dict[str, Callable[[dict, AssistContext], tuple[str, dict]]] = {
    "send_message_to_dispatcher": _send_message,
    "get_eta": _get_eta,
    "validate_route": _validate_route,
    "alternate_routes": _alternate_routes,
    "tech_help": _tech_help,
}


def dispatch(name: str, args: dict, ctx: AssistContext) -> tuple[str, dict]:
    handler = _HANDLERS.get(name)
    if handler is None:
        return f"Unknown tool {name}.", _err(name, "unknown tool")
    return handler(args or {}, ctx)

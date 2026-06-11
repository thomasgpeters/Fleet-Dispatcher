"""A tiny tech-help knowledge base (placeholder for a real KB / retrieval)."""

from __future__ import annotations

# topic-keyword -> spoken-style guidance
_KB: dict[str, str] = {
    "def": "DEF is diesel exhaust fluid. If you're low, top it off at the next "
           "truck stop; running out can put the truck into limp mode.",
    "regen": "A regen burns off the diesel particulate filter. Let it finish — "
             "keep the truck running, ideally at highway speed, until the light clears.",
    "oil": "Check oil on level ground with the engine off and warm. Add the grade "
           "in your equipment file if it's below the add mark.",
    "tire": "If a tire looks low or you feel a pull, stop somewhere safe and check "
            "pressures cold. Don't run on a flat — call it in.",
    "inspection": "For the post-trip inspection, walk the rig: lights, tires, "
                  "brakes, coupling, and load securement. Log it in the app.",
    "login": "If the app won't sign you in, check your connection, then confirm "
             "your driver username with your dispatcher.",
    "eld": "For ELD issues, note the time and what happened, switch to paper logs "
           "if required, and tell your dispatcher so they can flag it.",
}


def lookup(topic: str) -> str:
    """Best-effort keyword match; falls back to a safe generic answer."""
    t = (topic or "").lower()
    for key, answer in _KB.items():
        if key in t:
            return answer
    # No match — keep the driver moving and route to a human.
    return ("I don't have that one handy. I can pass the question to your "
            "dispatcher or your shop if you'd like.")

"""FastAPI surface for the Hey Dispatch assistant.

The mobile app does push-to-talk: browser STT produces a transcript, POSTs it
here with the driver's trusted context, and speaks back the `reply`. STT/TTS stay
on-device (Web Speech API); this service only does the thinking + acting.
"""

from __future__ import annotations

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from .assistant import run_assist
from .config import config
from .tools import AssistContext

app = FastAPI(title="Hey Dispatch", version="0.1.0")

# The mobile app is served from a different origin (and from Capacitor's
# capacitor:// / ionic:// schemes). Allow all in dev; tighten for production.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["POST", "GET", "OPTIONS"],
    allow_headers=["*"],
)


class AssistRequest(BaseModel):
    """One push-to-talk turn. `transcript` is the on-device STT output; the rest
    is server-trusted driver context the mobile app already holds (never the
    model's to choose)."""

    transcript: str
    driver_id: str | None = None
    driver_name: str | None = None
    author_user_id: str | None = None
    dispatcher_channel_id: str | None = None
    lat: float | None = None
    lng: float | None = None
    dest_lat: float | None = None
    dest_lng: float | None = None
    dest_label: str | None = None
    truck_height_m: float | None = None
    truck_weight_kg: float | None = None


class AssistResponse(BaseModel):
    reply: str
    actions: list[dict]
    cache_read_tokens: int = 0
    stopped: str | None = None


@app.get("/health")
def health() -> dict[str, object]:
    return {
        "status": "ok",
        "provider": config.provider,
        "model": config.model,
        "ai_configured": config.ai_configured,
        "here_configured": bool(config.here_api_key),
    }


@app.post("/assist", response_model=AssistResponse)
def assist(req: AssistRequest) -> AssistResponse:
    ctx = AssistContext(
        driver_id=req.driver_id,
        driver_name=req.driver_name,
        author_user_id=req.author_user_id,
        dispatcher_channel_id=req.dispatcher_channel_id,
        lat=req.lat,
        lng=req.lng,
        dest_lat=req.dest_lat,
        dest_lng=req.dest_lng,
        dest_label=req.dest_label,
        truck_height_m=req.truck_height_m,
        truck_weight_kg=req.truck_weight_kg,
    )
    result = run_assist(req.transcript, ctx)
    return AssistResponse(**result)

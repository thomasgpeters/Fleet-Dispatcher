"""Configuration for the realtime WebSocket bridge (env-driven).

Kafka settings follow ApiLogicServer / confluent-kafka conventions
(`bootstrap.servers`, `group.id`) so this consumes the same brokers ALS produces
to. See docs/REALTIME.md.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from typing import Dict, List


def _split(csv: str) -> List[str]:
    return [s.strip() for s in csv.split(",") if s.strip()]


# Topic routing registry: maps each Kafka topic (one per purpose/row type) to how
# the bridge derives WebSocket "keys" clients subscribe to. Adding a new topic is
# CONFIG, not code — and an unknown topic still works (generic fallback in
# bridge.py broadcasts it on its own name). Override the whole map with the env
# var KAFKA_TOPIC_ROUTES (JSON) as the app evolves.
#
#   "broadcast": a key every subscriber to this stream uses (e.g. "messages")
#   "scopes":    [{"prefix","field"}] -> targeted keys "<prefix>:<payload field>"
DEFAULT_ROUTES: Dict[str, dict] = {
    "message": {
        "broadcast": "messages",
        "scopes": [{"prefix": "channel", "field": "channel_id"}],
    },
    "position": {
        "broadcast": "fleet",
        "scopes": [{"prefix": "equipment", "field": "equipment_id"}],
    },
    # Future purposes are just more entries, e.g.:
    #   "load":  {"broadcast": "loads",  "scopes": [{"prefix": "driver", "field": "driver_id"}]},
    #   "alert": {"broadcast": "alerts", "scopes": [{"prefix": "driver", "field": "driver_id"}]},
}


def _routes() -> Dict[str, dict]:
    raw = os.environ.get("KAFKA_TOPIC_ROUTES")
    if raw:
        try:
            return json.loads(raw)
        except ValueError:
            pass  # fall back to defaults on bad JSON
    return dict(DEFAULT_ROUTES)


@dataclass(frozen=True)
class Config:
    # --- Kafka (consumer) ---
    # Brokers: KAFKA_BOOTSTRAP_SERVERS or KAFKA_SERVER (ALS uses KAFKA_SERVER).
    bootstrap_servers: str = (
        os.environ.get("KAFKA_BOOTSTRAP_SERVERS")
        or os.environ.get("KAFKA_SERVER")
        or "localhost:9092"
    )
    group_id: str = os.environ.get("KAFKA_GROUP_ID", "fleet-realtime-bridge")
    auto_offset_reset: str = os.environ.get("KAFKA_AUTO_OFFSET_RESET", "latest")
    # Topic -> routing registry (config-driven; see DEFAULT_ROUTES).
    routes: Dict[str, dict] = field(default_factory=_routes)

    # --- WebSocket server ---
    host: str = os.environ.get("BRIDGE_HOST", "0.0.0.0")
    port: int = int(os.environ.get("BRIDGE_PORT", "8765"))
    ping_interval: float = float(os.environ.get("WS_PING_INTERVAL", "20"))
    ping_timeout: float = float(os.environ.get("WS_PING_TIMEOUT", "20"))
    send_timeout: float = float(os.environ.get("WS_SEND_TIMEOUT", "5"))

    # --- Auth (reuse the ALS JWT) ---
    # HS256 secret shared with ALS (FLEET_JWT_SECRET / SECRET_KEY). If empty and
    # auth_required is true, the bridge refuses to start (fail safe).
    jwt_secret: str = os.environ.get("FLEET_JWT_SECRET") or os.environ.get("SECRET_KEY", "")
    jwt_algorithm: str = os.environ.get("FLEET_JWT_ALGORITHM", "HS256")
    auth_required: bool = os.environ.get("BRIDGE_AUTH_REQUIRED", "true").lower() != "false"

    @property
    def topics(self) -> List[str]:
        """Topics to subscribe to: KAFKA_TOPICS override, else every routed topic."""
        env = os.environ.get("KAFKA_TOPICS")
        return _split(env) if env else list(self.routes.keys())

    def route_for(self, topic: str) -> dict:
        """Routing spec for a topic; generic fallback keeps unknown topics working."""
        return self.routes.get(topic, {"broadcast": topic, "scopes": []})

    def kafka_conf(self) -> dict:
        return {
            "bootstrap.servers": self.bootstrap_servers,
            "group.id": self.group_id,
            "auto.offset.reset": self.auto_offset_reset,
            "enable.auto.commit": True,
        }


config = Config()

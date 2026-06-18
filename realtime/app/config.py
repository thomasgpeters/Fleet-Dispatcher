"""Configuration for the realtime WebSocket bridge (env-driven).

Kafka settings follow ApiLogicServer / confluent-kafka conventions
(`bootstrap.servers`, `group.id`) so this consumes the same brokers ALS produces
to. See docs/REALTIME.md.
"""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from typing import List


def _split(csv: str) -> List[str]:
    return [s.strip() for s in csv.split(",") if s.strip()]


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
    # Topics ALS produces to (one per row type). topic -> event type mapping.
    topics: List[str] = field(
        default_factory=lambda: _split(os.environ.get("KAFKA_TOPICS", "message,position"))
    )

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

    def kafka_conf(self) -> dict:
        return {
            "bootstrap.servers": self.bootstrap_servers,
            "group.id": self.group_id,
            "auto.offset.reset": self.auto_offset_reset,
            "enable.auto.commit": True,
        }


config = Config()

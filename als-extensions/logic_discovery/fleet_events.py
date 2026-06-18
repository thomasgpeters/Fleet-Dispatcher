"""Fleet Dispatcher — ALS Kafka event producer (auto-discovered logic).

Emits a Kafka event on every Message / PositionReport insert so the realtime
WebSocket bridge (realtime/) can fan changes out to clients. This is the
PRODUCER side of docs/REALTIME.md.

Why this file is robust against regen: ApiLogicServer auto-discovers every module
under `logic/logic_discovery/` and calls its `declare_logic()`. A `rebuild`
preserves `logic/`, so once installed this survives rebuilds; after a fresh
`create`, re-run `als-extensions/install.sh` (or `make als-extensions`).

Topic strategy (see docs/REALTIME.md): ONE topic per row type ("message",
"position"); the comms channel is a CORRELATION ID in the payload and the Kafka
KEY (kafka_key), which keeps per-channel ordering without a topic per channel.

NOTE (ALS version-sensitive): class names come from ALS model generation
(`message` -> Message, `position_report` -> PositionReport); the producer helper
import path / `send_kafka_message` signature and the producer config key
(KAFKA_CONNECT, older: KAFKA_PRODUCER) can vary by ALS version — adjust if your
generated project differs.
"""

from __future__ import annotations

import logging

from logic_bank.exec_row_logic.logic_row import LogicRow
from logic_bank.rule_bank.rule_bank import Rule

import integration.kafka.kafka_producer as kafka_producer
from database import models

log = logging.getLogger(__name__)

TOPIC_MESSAGE = "message"
TOPIC_POSITION = "position"


def _send_message_event(row, old_row, logic_row: LogicRow) -> None:
    if not logic_row.is_inserted():
        return  # inserts only (new chat messages)
    kafka_producer.send_kafka_message(
        logic_row=logic_row,
        kafka_topic=TOPIC_MESSAGE,
        kafka_key=str(row.channel_id),  # correlation id -> partition + ordering
        payload={
            "type": "message",
            "id": str(row.id),
            "channel_id": str(row.channel_id),
            "author_id": str(row.author_id),
            "posted_at": str(row.posted_at),
            "body": (row.body or "")[:240],
        },
    )


def _send_position_event(row, old_row, logic_row: LogicRow) -> None:
    if not logic_row.is_inserted():
        return  # inserts only (new telemetry fixes)
    kafka_producer.send_kafka_message(
        logic_row=logic_row,
        kafka_topic=TOPIC_POSITION,
        kafka_key=str(row.equipment_id or ""),
        payload={
            "type": "position",
            "id": str(row.id),
            "equipment_id": str(row.equipment_id) if row.equipment_id else None,
            "driver_id": str(row.driver_id) if row.driver_id else None,
            "lat": row.lat,
            "lng": row.lng,
            "recorded_at": str(row.recorded_at),
        },
    )


def declare_logic() -> None:
    """Registered by ALS logic discovery on server start."""
    Rule.after_flush_row_event(on_class=models.Message, calling=_send_message_event)
    Rule.after_flush_row_event(
        on_class=models.PositionReport, calling=_send_position_event
    )
    log.info("Fleet Dispatcher Kafka producers registered (message, position)")

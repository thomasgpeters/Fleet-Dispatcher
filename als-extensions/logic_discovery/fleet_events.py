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
TOPIC_LOAD = "load"
TOPIC_TRIP = "trip"


def _emit(logic_row: LogicRow, topic: str, key: str, payload: dict) -> None:
    """One topic per purpose; `key` is the correlation id (partition + ordering)."""
    kafka_producer.send_kafka_message(
        logic_row=logic_row, kafka_topic=topic, kafka_key=key, payload=payload
    )


def _is_change(logic_row: LogicRow) -> bool:
    """Insert or update (status transitions matter for loads/trips).
    NOTE (ALS version): is_updated() — adjust if your LogicRow differs."""
    return logic_row.is_inserted() or logic_row.is_updated()


def _send_message_event(row, old_row, logic_row: LogicRow) -> None:
    if not logic_row.is_inserted():
        return  # inserts only (new chat messages)
    _emit(logic_row, TOPIC_MESSAGE, str(row.channel_id), {
        "type": "message",
        "id": str(row.id),
        "channel_id": str(row.channel_id),
        "author_id": str(row.author_id),
        "posted_at": str(row.posted_at),
        "body": (row.body or "")[:240],
    })


def _send_position_event(row, old_row, logic_row: LogicRow) -> None:
    if not logic_row.is_inserted():
        return  # inserts only (new telemetry fixes)
    _emit(logic_row, TOPIC_POSITION, str(row.equipment_id or ""), {
        "type": "position",
        "id": str(row.id),
        "equipment_id": str(row.equipment_id) if row.equipment_id else None,
        "driver_id": str(row.driver_id) if row.driver_id else None,
        "lat": row.lat,
        "lng": row.lng,
        "recorded_at": str(row.recorded_at),
    })


def _send_load_event(row, old_row, logic_row: LogicRow) -> None:
    if not _is_change(logic_row):
        return  # insert or status/assignment change
    _emit(logic_row, TOPIC_LOAD, str(row.id), {
        "type": "load",
        "id": str(row.id),
        "driver_id": str(row.driver_id) if row.driver_id else None,
        "dispatch_week_id": str(row.dispatch_week_id),
        "load_status_id": row.load_status_id,
        "pickup_date": str(row.pickup_date) if row.pickup_date else None,
        "delivery_date": str(row.delivery_date) if row.delivery_date else None,
    })


def _send_trip_event(row, old_row, logic_row: LogicRow) -> None:
    if not _is_change(logic_row):
        return
    _emit(logic_row, TOPIC_TRIP, str(row.id), {
        "type": "trip",
        "id": str(row.id),
        "driver_id": str(row.driver_id) if row.driver_id else None,
        "equipment_id": str(row.equipment_id) if row.equipment_id else None,
        "load_id": str(row.load_id) if row.load_id else None,
        "trip_status_id": row.trip_status_id,
    })


# Add new realtime purposes here as the app evolves — one topic per purpose, with
# a correlation id as the kafka_key. Add a matching route in the bridge
# (DEFAULT_ROUTES / KAFKA_TOPIC_ROUTES) and have clients subscribe. e.g. an
# "alert" producer (route already seeded) once an alert source/table exists.

# (model class, handler) pairs registered on startup. Extend this list to add types.
_PRODUCERS = [
    (models.Message, _send_message_event),
    (models.PositionReport, _send_position_event),
    (models.Load, _send_load_event),
    (models.Trip, _send_trip_event),
]


def declare_logic() -> None:
    """Registered by ALS logic discovery on server start."""
    for model_cls, handler in _PRODUCERS:
        Rule.after_flush_row_event(on_class=model_cls, calling=handler)
    log.info(
        "Fleet Dispatcher Kafka producers registered: %d type(s)", len(_PRODUCERS)
    )

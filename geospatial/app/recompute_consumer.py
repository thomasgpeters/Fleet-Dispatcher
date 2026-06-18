"""Auto route recompute: a Kafka consumer that recomputes a trip's route whenever
its waypoints change.

ALS already streams `waypoint` (and `trip`) events to Kafka (als-extensions/);
this subscribes and calls recompute_route on each change — so add/remove/reorder
from ANY client triggers an automatic recompute, with no client coordination.
Pub/sub: a unique group per process (every geospatial instance recomputes).

Optional — only runs if Kafka is configured (KAFKA_BOOTSTRAP_SERVERS/KAFKA_SERVER).
"""

from __future__ import annotations

import json
import logging
import os
import socket
import threading
import uuid

from .recompute import recompute_route

log = logging.getLogger("fleet.geospatial.recompute")

_TOPICS = [
    t.strip()
    for t in os.environ.get("RECOMPUTE_TOPICS", "waypoint,trip").split(",")
    if t.strip()
]


def _brokers() -> str:
    return os.environ.get("KAFKA_BOOTSTRAP_SERVERS") or os.environ.get("KAFKA_SERVER", "")


def _trip_id(value: bytes) -> str | None:
    try:
        row = json.loads(value.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return None
    for key in ("payload", "args", "row", "data"):
        inner = row.get(key) if isinstance(row, dict) else None
        if isinstance(inner, dict):
            row = inner
            break
    # waypoint events carry trip_id; trip events carry the trip's own id.
    return row.get("trip_id") or row.get("id")


def _run(stop: threading.Event) -> None:
    from confluent_kafka import Consumer, KafkaError

    conf = {
        "bootstrap.servers": _brokers(),
        # Unique group → this instance sees every change (pub/sub fan-in).
        "group.id": f"fleet-geospatial-recompute-{socket.gethostname()}-{os.getpid()}-{uuid.uuid4().hex[:8]}",
        "auto.offset.reset": "latest",
        "enable.auto.commit": False,
    }
    backoff = 1.0
    while not stop.is_set():
        consumer = None
        try:
            consumer = Consumer(conf)
            consumer.subscribe(_TOPICS)
            log.info("recompute consumer up: %s topics=%s", conf["bootstrap.servers"], _TOPICS)
            backoff = 1.0
            while not stop.is_set():
                rec = consumer.poll(1.0)
                if rec is None:
                    continue
                if rec.error():
                    if rec.error().code() != KafkaError._PARTITION_EOF:
                        log.warning("kafka error: %s", rec.error())
                    continue
                trip_id = _trip_id(rec.value())
                if trip_id:
                    try:
                        recompute_route(trip_id)
                    except Exception as e:  # one bad event shouldn't kill the loop
                        log.error("recompute failed for trip %s: %s", trip_id, e)
        except Exception as e:
            log.error("recompute consumer error (retry in %.0fs): %s", backoff, e)
            stop.wait(backoff)
            backoff = min(backoff * 2, 30.0)
        finally:
            if consumer is not None:
                try:
                    consumer.close()
                except Exception:
                    pass


def start_auto_recompute() -> threading.Event | None:
    """Start the background consumer if Kafka is configured; returns its stop
    event (or None if disabled)."""
    if not _brokers():
        log.info("auto-recompute disabled (no KAFKA_BOOTSTRAP_SERVERS/KAFKA_SERVER)")
        return None
    stop = threading.Event()
    threading.Thread(target=_run, args=(stop,), name="recompute-consumer", daemon=True).start()
    return stop

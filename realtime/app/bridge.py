"""Realtime WebSocket bridge: Kafka consumer -> WebSocket fan-out.

Architecture (docs/REALTIME.md):

    ALS events --(Kafka producer)--> Kafka topics --(this bridge)--> WebSocket --> clients

Robustness by design — this is an *accelerator*, never a source of truth:
  * Kafka (durable log) is the event source; ALS produces on every write, so the
    bridge sees changes from every client (mobile, desktop, assistant, SQL).
  * The bridge is stateless: if it restarts, clients auto-reconnect and a
    consumer group resumes; nothing is lost from the DB (the API stays canonical).
  * Clients keep a slow reconcile poll, so if the bridge is down everything still
    works — just less instantly. No fragile inter-app coupling.

The consumer runs in a background thread (confluent-kafka is poll-based) and
hands events to the asyncio fan-out via a threadsafe queue.
"""

from __future__ import annotations

import asyncio
import json
import logging
import threading
from typing import Any, Dict, Optional, Set

import jwt
import websockets
from websockets.server import WebSocketServerProtocol

from .config import config

log = logging.getLogger("fleet.realtime")


# --- Event model -------------------------------------------------------------

def _unwrap(value: Any) -> Dict[str, Any]:
    """ALS mappers may send the row dict directly or wrapped; be tolerant."""
    if isinstance(value, dict):
        for key in ("payload", "args", "row", "data"):
            inner = value.get(key)
            if isinstance(inner, dict):
                return inner
        return value
    return {}


def normalize(topic: str, raw: bytes) -> Optional[Dict[str, Any]]:
    """Turn a Kafka record into a routable event, or None if unusable."""
    try:
        value = json.loads(raw.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        log.warning("dropping non-JSON message on topic %s", topic)
        return None
    row = _unwrap(value)
    # Event type: explicit field wins, else the topic name (ALS uses one topic
    # per row type, e.g. "message", "position").
    etype = (row.get("type") or value.get("type") or topic).lower()
    event: Dict[str, Any] = {"type": etype, **row}
    return event


def routing_keys(topic: str, event: Dict[str, Any]) -> Set[str]:
    """WebSocket subscription keys this event is delivered to — derived from the
    config-driven route registry, so adding a topic/purpose needs no code change.
    """
    route = config.route_for(topic)
    keys: Set[str] = {event.get("type") or topic}  # the type/topic stream itself
    broadcast = route.get("broadcast")
    if broadcast:
        keys.add(broadcast)
    for scope in route.get("scopes", []):
        value = event.get(scope.get("field", ""))
        if value:
            keys.add(f"{scope['prefix']}:{value}")
    return keys


# --- Client registry ---------------------------------------------------------

class Client:
    def __init__(self, ws: WebSocketServerProtocol, user: Dict[str, Any]):
        self.ws = ws
        self.user = user
        self.topics: Set[str] = set()


class Hub:
    def __init__(self) -> None:
        self.clients: Set[Client] = set()

    def add(self, c: Client) -> None:
        self.clients.add(c)

    def remove(self, c: Client) -> None:
        self.clients.discard(c)

    async def fan_out(self, topic: str, event: Dict[str, Any]) -> None:
        keys = routing_keys(topic, event)
        payload = json.dumps({"type": "event", "event": event})
        dead = []
        for c in list(self.clients):
            if c.topics & keys:
                try:
                    await asyncio.wait_for(c.ws.send(payload), config.send_timeout)
                except Exception:  # slow/broken client — drop it
                    dead.append(c)
        for c in dead:
            self.remove(c)
            try:
                await c.ws.close()
            except Exception:
                pass


hub = Hub()


# --- Auth --------------------------------------------------------------------

def verify_token(token: Optional[str]) -> Optional[Dict[str, Any]]:
    if not config.auth_required:
        return {"sub": "anonymous"}
    if not token:
        return None
    try:
        return jwt.decode(token, config.jwt_secret, algorithms=[config.jwt_algorithm])
    except jwt.PyJWTError as e:
        log.info("rejected token: %s", e)
        return None


def _query_token(path: str) -> Optional[str]:
    # ws://host:port/?token=...
    if "?" not in path:
        return None
    from urllib.parse import parse_qs, urlsplit

    qs = parse_qs(urlsplit(path).query)
    vals = qs.get("token") or qs.get("access_token")
    return vals[0] if vals else None


# --- WebSocket handler -------------------------------------------------------

async def handle(ws: WebSocketServerProtocol) -> None:
    token = _query_token(ws.path)
    user = verify_token(token)
    if user is None:
        await ws.close(code=4401, reason="unauthorized")
        return

    client = Client(ws, user)
    hub.add(client)
    try:
        await ws.send(json.dumps({"type": "hello", "you": user.get("sub")}))
        async for raw in ws:
            try:
                msg = json.loads(raw)
            except ValueError:
                continue
            action = msg.get("action")
            topics = msg.get("topics") or []
            if action == "subscribe":
                client.topics.update(t for t in topics if isinstance(t, str))
                await ws.send(json.dumps({"type": "subscribed", "topics": sorted(client.topics)}))
            elif action == "unsubscribe":
                client.topics.difference_update(topics)
            elif action == "ping":
                await ws.send(json.dumps({"type": "pong"}))
    except websockets.ConnectionClosed:
        pass
    finally:
        hub.remove(client)


# --- Kafka consumer (background thread) --------------------------------------

def _consume_loop(loop: asyncio.AbstractEventLoop, queue: "asyncio.Queue[tuple[str, Dict[str, Any]]]",
                  stop: threading.Event) -> None:
    """Poll Kafka and hand events to the asyncio loop. Resilient to broker
    outages (confluent-kafka reconnects); fatal errors retry with backoff."""
    from confluent_kafka import Consumer, KafkaError

    backoff = 1.0
    while not stop.is_set():
        consumer = None
        try:
            consumer = Consumer(config.kafka_conf())
            consumer.subscribe(config.topics)
            log.info("kafka consumer up: %s topics=%s", config.bootstrap_servers, config.topics)
            backoff = 1.0
            while not stop.is_set():
                rec = consumer.poll(1.0)
                if rec is None:
                    continue
                if rec.error():
                    if rec.error().code() != KafkaError._PARTITION_EOF:
                        log.warning("kafka error: %s", rec.error())
                    continue
                event = normalize(rec.topic(), rec.value())
                if event is not None:
                    loop.call_soon_threadsafe(queue.put_nowait, (rec.topic(), event))
        except Exception as e:  # broker unreachable, etc.
            log.error("kafka loop error (retry in %.0fs): %s", backoff, e)
            stop.wait(backoff)
            backoff = min(backoff * 2, 30.0)
        finally:
            if consumer is not None:
                try:
                    consumer.close()
                except Exception:
                    pass


async def _dispatch(queue: "asyncio.Queue[tuple[str, Dict[str, Any]]]") -> None:
    while True:
        topic, event = await queue.get()
        await hub.fan_out(topic, event)


# --- Entry -------------------------------------------------------------------

async def run() -> None:
    if config.auth_required and not config.jwt_secret:
        raise SystemExit(
            "FLEET_JWT_SECRET is required (set it to the ALS JWT secret, or set "
            "BRIDGE_AUTH_REQUIRED=false for local dev)."
        )

    loop = asyncio.get_running_loop()
    queue: "asyncio.Queue[tuple[str, Dict[str, Any]]]" = asyncio.Queue()
    stop = threading.Event()

    consumer_thread = threading.Thread(
        target=_consume_loop, args=(loop, queue, stop), name="kafka-consumer", daemon=True
    )
    consumer_thread.start()

    dispatcher = asyncio.create_task(_dispatch(queue))

    log.info("websocket bridge listening on ws://%s:%d", config.host, config.port)
    async with websockets.serve(
        handle, config.host, config.port,
        ping_interval=config.ping_interval, ping_timeout=config.ping_timeout,
    ):
        try:
            await asyncio.Future()  # run forever
        finally:
            stop.set()
            dispatcher.cancel()

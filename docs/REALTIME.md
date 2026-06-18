# Realtime (WebSocket bridge over ALS → Kafka)

Cross-client realtime — a message sent on mobile appears on the dispatcher
desktop instantly; telemetry flows live to the map — without fragile point-to-
point coupling between apps.

```
            write (JSON:API)                     events
 client ───────────────────────▶ ApiLogicServer ───────▶ Kafka ──▶ realtime bridge ──▶ WebSocket ──▶ clients
   ▲           (source of truth)   (Kafka producer)      (durable)   (Kafka consumer)     (fan-out)      │
   └──────────────────────────── read (JSON:API) ◀───────────────────────────────────────── reconcile ─┘
                                                                                            (fallback poll)
```

## Why this shape (robust, not fragile)

- **ALS → Kafka is the canonical event source.** ALS already produces domain
  events to Kafka (`Rule.after_flush_row_event` + a mapper `row_to_dict`; see the
  GenAI-Logic [Sample-Integration](https://apilogicserver.github.io/Docs/Sample-Integration/)).
  Every write — from any client — produces an event, so there is **one** event
  path, not N app-to-app links.
- **Kafka is durable.** A bridge restart re-joins the consumer group; nothing is
  lost. The DB/JSON:API stays the source of truth.
- **The bridge is stateless** (`realtime/`): a Kafka consumer that fans out to
  WebSocket subscribers. It can be scaled or replaced freely.
- **Clients degrade gracefully.** Writes/reads always go through the JSON:API, and
  each client keeps a slow **reconcile poll**. If the bridge or Kafka is down,
  the app still works — just less instantly. The realtime layer is an
  *accelerator*, never a dependency.

## Topic strategy — single topic per row type + correlation id (NOT topic-per-channel)

**Decision:** one Kafka topic **per row type** (`message`, `position`), with the
**`channel_id` carried in the JSON payload as the correlation id**. Per-channel
fan-out happens in the **bridge → WebSocket** layer (`channel:<id>` subscriptions),
not at the Kafka layer.

Why not a Kafka topic per comms channel:

- Channels are **dynamic, UUID-keyed domain objects** created at runtime. A topic
  per channel means runtime topic creation and an unbounded, ever-growing topic
  count — Kafka metadata bloat, controller pressure, and rebalancing churn. Kafka
  is built for a **stable, finite** set of topics, not thousands of dynamic ones.
- ALS's declarative producer emits **one topic per row class** — topic-per-channel
  would fight that model.
- The bridge already needs the full message stream (it serves *all* channels), so
  reading one `message` topic and routing by `channel_id` is strictly simpler.

**Ordering:** set the Kafka **message key = `channel_id`** so all messages for a
channel land on the same partition and stay ordered, while the topic count stays
fixed. (Correlation id in the payload; partition affinity via the key.)

Use topic-per-*type* (what we do) — never topic-per-*instance*.

## Event envelope (the contract)

The bridge maps **topic → type** and is tolerant of the ALS mapper's shape
(accepts the row dict directly or wrapped in `payload`/`args`/`row`). Minimum
fields the clients use:

```jsonc
// topic "message"  (key = channel_id)
{ "type": "message", "id": "...", "channel_id": "...", "author_id": "...",
  "posted_at": "2026-06-18T15:00:00Z", "body": "first 240 chars" }

// topic "position" (key = equipment_id)
{ "type": "position", "id": "...", "equipment_id": "...", "driver_id": "...",
  "lat": 39.5, "lng": -98.3, "recorded_at": "..." }
```

## WebSocket protocol (bridge ↔ client)

1. Connect `ws://<host>:8765/?token=<ALS-JWT>` (HS256, verified with the shared
   `FLEET_JWT_SECRET`). Invalid/missing → close `4401`.
2. `{"action":"subscribe","topics":["channel:<id>","fleet"]}`.
   Topics: `channel:<id>`, `messages`, `fleet`, `equipment:<id>`.
3. Receive `{"type":"event","event":{...}}`.

## Pieces

| Piece | Where | Notes |
| --- | --- | --- |
| Producer | ALS project (outside repo) | `KAFKA_PRODUCER` + `send_row_to_kafka`, key = correlation id |
| Bridge | [`realtime/`](../realtime/) | confluent-kafka consumer → WebSocket; JWT; auto-reconnect |
| Mobile | `portals/mobile/src/realtime/` | `RealtimeClient` (backoff reconnect) + `RealtimeProvider`; wired into Channel/Channels |
| Desktop/HUD | `portals/dispatcher-desktop/` | intra-server push today (`CommBus` + Wt WebSockets, `wt_config.xml`); joining the bridge via a WS client is the next step |

## Status / follow-ups

- Mobile: **done** — live channel messages + unread, with reconcile fallback.
- Desktop: consumes its own sends instantly via `CommBus`; **joining the external
  bridge** (a WS client → `CommBus`) is the remaining step for mobile→desktop.
- ALS: configure the Kafka producer + per-type mappers (keys = correlation ids).

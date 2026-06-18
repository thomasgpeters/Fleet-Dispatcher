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

## Two data planes

The app deliberately splits its data into two planes:

| Plane | Carries | How clients get it |
| --- | --- | --- |
| **Realtime** | **messages, fleet locations / map updates**, and live status changes (load/trip transitions, alerts) | **Live from the Kafka stream via the bridge** — applied directly from the event payload, no DB read-back |
| **Request/response (ALS)** | everything else — CRUD, reference/lookups, detail & forms, settlements, and the **initial snapshot** of a realtime view | **JSON:API (ApiLogicServer)** — request/response |

So: **writes** and **one-time snapshot loads** go through ALS; **ongoing live
updates** for the realtime categories come from the stream — clients do **not**
poll the DB through ALS to stay live. A view typically loads its snapshot once via
ALS, then applies live deltas from the stream.

## Kafka stream = public realtime plane; bridge = its public edge

The Kafka **stream** is now first-class **public** infrastructure that every live
client (mobile, desktop, HUD) integrates with — not just internal IAC. But clients
integrate **through the bridge**, which is the stream's public, client-facing edge:

- Clients get only the **bridge URL + a JWT** — never broker addresses, the
  consumer config, or the signing secret (those stay internal; see
  "Configuration & secrets"). The bridge is the trust boundary and the protocol
  adapter (browsers/Capacitor can't speak Kafka's native protocol; they speak
  WebSocket).
- Behind it, the bridge is a Kafka **consumer** (pub/sub — see "Consumer model"),
  so the public realtime plane is backed by the durable Kafka log.

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

## Configuration & secrets (internal only)

Kafka is an **internal** concern. Brokers, credentials, the consumer group, and
the JWT secret live **only in server-side env files**, which are gitignored
(`.env`, `*.env`); only `*.env.example` **placeholder** templates are committed.

**Clients never see Kafka.** The mobile and desktop apps only ever know the
**bridge WebSocket URL** and a **JWT** — never a broker address, topic list, or
the signing secret. The bridge is the trust/encapsulation boundary.

| Secret / setting | Lives in (internal, gitignored) | Who reads it |
| --- | --- | --- |
| Kafka brokers / creds | ALS project env · `realtime/.env` (`KAFKA_*`) | ALS producer · bridge consumer |
| JWT signing secret | ALS project env · `realtime/.env` (`FLEET_JWT_SECRET`) | ALS (issues) · bridge (verifies) |
| Bridge URL | client env (`VITE_REALTIME_URL` / `FLEET_REALTIME_URL`) | clients only |
| Per-user / service JWT | issued at login / service env (`FLEET_REALTIME_TOKEN`) | clients → bridge |

So a leaked client build exposes only a WebSocket URL and a (scoped, expiring)
token — never the Kafka topology or the secret. Rotate secrets by editing the
server-side env files; no client change needed.

## Consumer model — publish/subscribe (one or more consumers)

ALS produces each event **once** to a topic. Kafka then delivers it to **every
consumer group** independently — that's the pub/sub part. So any number of
independent consumers can pick up the same messages:

```
                              ┌─ group "realtime-bridge-A" → WS clients   (this service)
ALS ─▶ topic ─▶ Kafka  ──────▶├─ group "realtime-bridge-B" → WS clients   (HA replica)
        (one copy produced)   ├─ group "notifications"      → push/email   (future)
                              └─ group "analytics"          → warehouse    (future)
```

Two rules that keep this correct:

- **Different purpose ⇒ different `group.id`.** Each service is its own
  subscriber and receives the **full** stream. Add consumers without touching the
  producer or other consumers.
- **The bridge is a FAN-OUT relay, so every instance needs every event.** Each
  bridge instance therefore uses a **unique** group (`KAFKA_GROUP_UNIQUE=true`,
  the default — base id + host/pid/uuid). Run as many instances as you like for
  HA/scale; each fans out to its own connected WebSocket clients.
- Set `KAFKA_GROUP_UNIQUE=false` (shared `KAFKA_GROUP_ID`) only when you *want*
  Kafka's competing-consumer **queue** semantics — one event handled by exactly
  one instance (work distribution, e.g. a notifications worker pool) — never for
  fan-out. (The bridge also skips offset commits in unique mode — it only wants
  live events.)

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

Use topic-per-*type/purpose* (what we do) — never topic-per-*instance*.

### Multiple topics, and adding one as the app evolves

We expect more purposes over time (loads, trips, alerts, settlements, …). The
bridge routing is **config-driven**, so a new topic is config, not code:

- The bridge keeps a **route registry** (`realtime/app/config.py` →
  `DEFAULT_ROUTES`, overridable with the `KAFKA_TOPIC_ROUTES` env JSON). Each
  topic maps to a `broadcast` key + optional `scopes` (`{prefix, field}` →
  `prefix:<payload value>`). An **unknown topic still works** — it's broadcast on
  its own name (generic fallback), so nothing breaks if a producer ships ahead of
  a route entry.
- To add a purpose:
  1. **ALS** — add a `_send_*` handler + entry in `_PRODUCERS`
     (`als-extensions/logic_discovery/fleet_events.py`); one topic, correlation
     id as `kafka_key`.
  2. **Bridge** — add a route (in `DEFAULT_ROUTES` or via `KAFKA_TOPIC_ROUTES`)
     if you want scoped keys; otherwise the fallback handles it.
  3. **Clients** — `subscribe` to the new keys (e.g. `loads`, `driver:<id>`).

No change to the consumer loop, auth, or fan-out — those are generic.

**Seeded routes:** `message`, `position`, `load`, `trip`, `alert`. ALS producers
exist for message/position and now **load/trip** (fire on insert *and* update, so
status/assignment changes stream live — keys: `loads`/`driver:<id>`/`week:<id>`,
`trips`/`driver:<id>`). `alert`'s route is ready; its producer lands with an
alert source.

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
   Keys are whatever the route registry produces — currently `channel:<id>`,
   `messages`, `fleet`, `equipment:<id>` — and grow as purposes are added.
3. Receive `{"type":"event","event":{...}}`.

## Pieces

| Piece | Where | Notes |
| --- | --- | --- |
| Producer | ALS project (outside repo) | our snippet in [`als-extensions/`](../als-extensions/) — `Rule.after_flush_row_event` + `send_kafka_message`, key = correlation id; installed via `make als-extensions` |
| Bridge | [`realtime/`](../realtime/) | confluent-kafka consumer → WebSocket; JWT; auto-reconnect |
| Mobile | `portals/mobile/src/realtime/` | `RealtimeClient` (backoff reconnect) + `RealtimeProvider`; wired into Channel/Channels |
| Desktop/HUD | `portals/dispatcher-desktop/` | `RealtimeClient` (server-wide Boost.Beast WS client, opt-in `-DFD_REALTIME_CLIENT=ON`) consumes the bridge → `CommBus` → all sessions via Wt push; intra-server `CommBus` + reconcile poll are the fallback |

## Status / follow-ups

- Mobile: **done** — live channel messages + unread applied **directly from the
  stream** (no ALS read-back); initial snapshot via ALS.
- Desktop: **done** — `RealtimeClient` (build with `-DFD_REALTIME_CLIENT=ON`)
  connects to the bridge with a service token and publishes events into in-process
  buses: **messages → `CommBus`** (CommPanel, de-duped by id) and **positions →
  `PositionBus`** (MapView + HUD apply live fleet locations / map updates;
  60 s ALS reconcile as fallback). Set `FLEET_REALTIME_URL` + `FLEET_REALTIME_TOKEN`.
- ALS: the producer logic lives in [`als-extensions/`](../als-extensions/) and is
  re-installed after each ALS generate with `make als-extensions` (ALS rebuilds
  preserve `logic/`, so it's a one-time step per fresh `create`). Enable the
  producer config (`KAFKA_CONNECT`) in the ALS project.

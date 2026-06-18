# Realtime WebSocket bridge

Fans out **ApiLogicServer (ALS) events** to all clients in real time. ALS already
produces domain events to **Kafka**; this service is a stateless **Kafka consumer
→ WebSocket relay**. See [`docs/REALTIME.md`](../docs/REALTIME.md) for the full
architecture.

```
ALS events ──(Kafka producer)──▶ Kafka topics ──(this bridge)──▶ WebSocket ──▶ clients
                                  (durable log)     (fan-out)
```

**Why this is robust, not fragile**

- **Kafka is the durable source** of change events; ALS produces on every write,
  so the bridge sees changes from *every* client (mobile, desktop, assistant,
  even direct SQL via ALS).
- The bridge is **stateless** — restart it freely; clients auto-reconnect and the
  consumer group resumes. The **DB/JSON:API stays canonical**; this only
  accelerates delivery.
- Clients keep a **slow reconcile poll**, so if the bridge (or Kafka) is down,
  everything still works — just less instantly. No tight inter-app coupling.

## Run

```bash
cd realtime
python -m venv venv && . venv/bin/activate
pip install -r requirements.txt
cp .env.example .env        # set KAFKA_* and FLEET_JWT_SECRET
python -m app.main          # ws://0.0.0.0:8765
```

Docker: `docker build -t fleet-realtime . && docker run --env-file .env -p 8765:8765 fleet-realtime`.
systemd: see `deploy/`.

## Client protocol

1. Connect `ws://<host>:8765/?token=<ALS-JWT>` (HS256, verified with
   `FLEET_JWT_SECRET`). Bad/missing token → close `4401`.
2. Subscribe: `{"action":"subscribe","topics":["channel:<id>","fleet"]}`.
   - `channel:<id>` — messages in a channel · `messages` — all messages
   - `fleet` — all positions · `equipment:<id>` — one rig
3. Receive: `{"type":"event","event":{"type":"message","channel_id":...,...}}`.
4. `{"action":"ping"}` → `{"type":"pong"}` (WS ping/pong also runs automatically).

## ALS producer side (what this consumes)

The producer logic is maintained in [`../als-extensions/`](../als-extensions/)
(`logic_discovery/fleet_events.py`) and installed into the generated ALS project
with `make als-extensions`. It uses the GenAI-Logic pattern —
`Rule.after_flush_row_event` + `kafka_producer.send_kafka_message(...)` with
`kafka_key` = the correlation id (channel_id / equipment_id) for per-channel
ordering. Enable the producer in the ALS config (`KAFKA_CONNECT`).

The bridge maps **topic → event type** (`message`, `position`) and is tolerant of
the envelope (accepts the row dict directly or wrapped in `payload`/`args`/`row`).
Adjust `KAFKA_TOPICS` to match your ALS topic names. ALS Kafka docs:
<https://apilogicserver.github.io/Docs/Sample-Integration/>.

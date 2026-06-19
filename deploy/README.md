# Deployment — systemd units

systemd units for every Fleet Dispatcher service. Each reads its secrets/config
from `/etc/fleet-dispatcher/<name>.env` (mode `600`), runs as the shared
**`fleet`** system user, and restarts on failure. Architecture overview:
[`../docs/architecture.svg`](../docs/architecture.svg) · [`REALTIME.md`](../docs/REALTIME.md).

## Components & units

| Service | Unit | Port | Depends on | Source dir |
| --- | --- | --- | --- | --- |
| **ApiLogicServer** (JSON:API + JWT) | `deploy/fleet-dispatcher-api.service` | 5659 | PostgreSQL | generated (`/opt/fleet-dispatcher/api`) |
| **Realtime bridge** (Kafka→WS) | `realtime/deploy/fleet-dispatcher-realtime.service` | 8765 | Kafka | `realtime/` |
| **Geospatial** (PostGIS + route recompute) | `geospatial/deploy/fleet-dispatcher-geospatial.service` | 5701 | PostgreSQL, Kafka* | `geospatial/` |
| **Assistant** ("Hey Dispatch") | `assistant/deploy/fleet-dispatcher-assistant.service` | 5710 | ApiLogicServer | `assistant/` |
| **Dispatcher desktop** (Wt console + HUD) | `portals/dispatcher-desktop/deploy/fleet-dispatcher-desktop.service` | 8089 | ApiLogicServer | `portals/dispatcher-desktop/` |
| **Mobile web** (built SPA) | `portals/mobile/deploy/fleet-dispatcher-mobile.service` | 5173 | (browser → API/bridge) | `portals/mobile/` |

\* Kafka is optional — geospatial auto-recompute only runs if `KAFKA_*` is set.

**Externally managed** (their own packages/units, not ours):
- **PostgreSQL 16** (+ PostGIS) — `postgresql.service` (the `fleet` DB; see
  [`../docs/DEPLOYMENT.md`](../docs/DEPLOYMENT.md)).
- **Apache/Confluent Kafka** — its distribution's unit (e.g. `kafka.service`).
  The bridge + geospatial consume it; ALS produces to it.

## One-time setup

```bash
sudo useradd --system --no-create-home --shell /usr/sbin/nologin fleet   # shared user
sudo mkdir -p /etc/fleet-dispatcher /opt/fleet-dispatcher
```

## Install a service (same pattern for each)

```bash
# 1. place code under /opt/fleet-dispatcher/<svc> (+ a venv for the Python ones)
# 2. config:
sudo cp <dir>/deploy/<svc>.env.example /etc/fleet-dispatcher/<svc>.env
sudo chmod 600 /etc/fleet-dispatcher/<svc>.env && sudo $EDITOR /etc/fleet-dispatcher/<svc>.env
# 3. unit:
sudo cp <dir>/deploy/fleet-dispatcher-<svc>.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now fleet-dispatcher-<svc>
journalctl -u fleet-dispatcher-<svc> -f
```
Per-service install notes are in the header comment of each `.service` file.

## Start order (handled by `After=` + dependencies)

```
postgresql  →  [kafka]  →  fleet-dispatcher-api  →  desktop / assistant / mobile
                       ↘   fleet-dispatcher-realtime
                       ↘   fleet-dispatcher-geospatial
```

## Shared secret

`SECRET_KEY` (ALS, `api.env`) and `FLEET_JWT_SECRET` (bridge, `realtime.env`)
**must be identical** — the bridge verifies the JWTs ALS issues. Clients only
receive the bridge URL + a token, never Kafka/broker config.

## Mobile: prefer nginx in production

The mobile unit uses `vite preview` for simplicity. For production, serve the
static build with nginx (SPA fallback + gzip + caching):

```nginx
server {
    listen 80;
    server_name fleet.example.com;
    root /opt/fleet-dispatcher/mobile/dist;
    location / { try_files $uri /index.html; }   # SPA routing
}
```
Remember `VITE_*` are baked at **build** time — set `mobile.env` before
`npm run build`.

## Verify the stack

```bash
systemctl --type=service | grep fleet-dispatcher
curl -s http://localhost:5659/api >/dev/null && echo "API ok"
curl -s http://localhost:5701/health
# console: http://<host>:8089/   ·   HUD: http://<host>:8089/hud   ·   mobile: http://<host>:5173/
```

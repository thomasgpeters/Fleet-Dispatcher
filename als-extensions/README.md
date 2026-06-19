# ALS extensions

Customizations we layer onto the **generated** ApiLogicServer project
(`fleet-dispatcher-api`, built outside this repo — see
[`../docs/MIDDLEWARE_SETUP.md`](../docs/MIDDLEWARE_SETUP.md)). Kept here, in the
monorepo, so they're versioned and re-installable after ALS is (re)generated.

## What's here

- `logic_discovery/fleet_events.py` — emits **Kafka events** on Message /
  PositionReport / Load / Trip / Waypoint changes, so the realtime bridge
  (`../realtime/`) can fan changes out to clients. See
  [`../docs/REALTIME.md`](../docs/REALTIME.md).
- `security/authentication_provider/sql/auth_provider.py` — ALS SQL auth provider
  that authenticates against our **`app_user`** table and verifies werkzeug
  password hashes (the default uses a separate sqlite auth DB + plaintext).
- `security/declare_security.py` — role grants so authenticated users can read
  data (dev-permissive baseline; tighten for prod). See
  [`../docs/AUTHENTICATION.md`](../docs/AUTHENTICATION.md).
- `install.sh` — copies all of the above into the ALS project.

## Install — from-scratch sequence

```bash
ApiLogicServer create --project_name=fleet-dispatcher-api --db_url="$DATABASE_URL"
cd fleet-dispatcher-api && ApiLogicServer add-auth --provider-type=sql && cd -
make als-extensions ALS_PROJECT=./fleet-dispatcher-api    # installs logic + security
```

`make als-extensions` (or `als-extensions/install.sh /path/to/fleet-dispatcher-api`)
copies the Kafka producers into `logic/logic_discovery/` and — **if `add-auth`
has been run** (so `security/` exists) — overwrites `auth_provider.py` +
`declare_security.py`. If `security/` isn't there yet, it installs the producers
and reminds you to run `add-auth`, then re-run.

Run ALS with security on:
```bash
export SECURITY_ENABLED=true
export SECRET_KEY=<your-jwt-secret>      # share this with the realtime/ bridge
python api_logic_server_run.py 0.0.0.0 5659
```

Then enable the producer in the ALS project and (re)start ALS — read the broker
from the **environment**, never a hardcoded/committed value (keep the ALS
project's `.env` out of git):

```python
# config/config.py  (older ALS: KAFKA_PRODUCER)
import os
KAFKA_CONNECT = os.getenv("KAFKA_CONNECT")  # e.g. '{"bootstrap.servers": "broker:9092"}'
```

```bash
# ALS project .env (gitignored — internal only)
KAFKA_CONNECT={"bootstrap.servers": "broker:9092"}
```

Kafka config is internal: brokers/creds and the JWT secret live only in
server-side env files; clients never see them (see `docs/REALTIME.md`).

## Why this is low-maintenance

ALS **auto-discovers** every module in `logic/logic_discovery/` and calls its
`declare_logic()` — no edits to generated files. An ALS **`rebuild`** preserves
`logic/`, so once installed this survives rebuilds. Only a fresh
**`create`** wipes it; re-run `make als-extensions` then.

**Automate it**: chain the install onto your generate step, e.g.

```bash
ApiLogicServer create --project_name=fleet-dispatcher-api ... \
  && make als-extensions ALS_PROJECT=./fleet-dispatcher-api
```

or add that `make als-extensions` line to whatever script/CI rebuilds ALS.

> Version note: ALS model class names (`Message`, `PositionReport`), the
> `kafka_producer.send_kafka_message` signature, and the producer config key can
> vary by ALS version. `fleet_events.py` flags the spots to check.

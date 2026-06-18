# ALS extensions

Customizations we layer onto the **generated** ApiLogicServer project
(`fleet-dispatcher-api`, built outside this repo — see
[`../docs/MIDDLEWARE_SETUP.md`](../docs/MIDDLEWARE_SETUP.md)). Kept here, in the
monorepo, so they're versioned and re-installable after ALS is (re)generated.

## What's here

- `logic_discovery/fleet_events.py` — emits a **Kafka event** on every
  `Message` / `PositionReport` insert, so the realtime bridge (`../realtime/`)
  can fan changes out to clients. See [`../docs/REALTIME.md`](../docs/REALTIME.md).
- `install.sh` — copies the above into `<ALS_PROJECT>/logic/logic_discovery/`.

## Install (after generating ALS)

```bash
# one command (defaults ALS_PROJECT=../fleet-dispatcher-api):
make als-extensions
# or explicitly:
als-extensions/install.sh /path/to/fleet-dispatcher-api
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

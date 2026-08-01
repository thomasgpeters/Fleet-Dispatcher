# integration/ — Fleet ⇄ Smitty Services sync (Fleet-side consumer)

Fleet's half of the vehicle/service integration with **Smitty Services**. Full
contract: [`../docs/INTEGRATION_SMITTY.md`](../docs/INTEGRATION_SMITTY.md).

## What's here

- `smitty_poller.py` — the **thin fetch driver** (scaffold). It pulls Smitty's
  service data and writes it through **Fleet's ALS JSON:API** so the LogicBank
  rules fire on ingest (cost-allocation snapshot, maintenance status, OOS
  dispatch-lock). It carries no business logic — that lives in
  `als-extensions/logic_discovery/fleet_governance.py`.

## Design (per the LogicBank doctrine)

```
Smitty ALS  --GET (X-Service-Token, scoped to Fleet's customer_id)-->  poller
   poller   --POST/PATCH Fleet ALS JSON:API-->  service_record / maintenance_schedule
                                                / vehicle_out_of_service  (mirror tables)
   Fleet LogicBank (fleet_governance.py)  --fires on those inserts-->
        · Rule.copy   maint_responsibility snapshot (allocate at-cost)
        · Rule.formula maintenance status (upcoming/due/overdue)
        · dispatch-lock reads the OOS window -> can't dispatch an in-shop rig
```

Boundary guarantees the poller enforces:
- **Only Fleet-owned VINs** ingest (unresolved VIN → dropped; third-party never lands).
- **At-cost only** — the retail/markup fields are never read or stored.
- Idempotent by the Smitty correlation id (PATCH if present, else POST).

## Status

Scaffold — runs once Smitty ships its Phase-1 endpoints (`/api/Job`,
`/api/MaintenanceSchedule`, `/api/VehicleOutOfService`) + the `X-Service-Token`
middleware. Sequencing: **RMA first** (see the contract). Field names marked
`TODO` need confirming against Smitty's actual JSON shapes — especially the
**at-cost** figure on `Job` (must be distinct from retail/`job_total`).

## Run (once live)

```bash
pip install -r integration/requirements.txt
FLEET_TOKEN=... SMITTY_API_BASE=... SMITTY_SERVICE_TOKEN=... SMITTY_CUSTOMER_ID=... \
  python integration/smitty_poller.py          # one pass; schedule via cron (thin driver, like the sys_clock tick)
```

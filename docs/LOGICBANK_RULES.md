# LogicBank rules — the fleet's declarative business logic

**Doctrine (read this first).** Business logic for Fleet Dispatcher lives in
**LogicBank rules** (ApiLogicServer middleware), *not* hard-coded in the C++
desktop or the Python assistant/realtime clients. A rule belongs in LogicBank
when it is an **invariant that must hold no matter which client writes** — because
LogicBank runs at the one place every write funnels through (the ALS commit), so
mobile, desktop, the assistant, and any raw API call are governed **identically
and once**. Clients stay **presentation/UX**. This is consistent with our golden
rule ("don't write middleware Python" bans redundant *services*; LogicBank rules
in `als-extensions/logic_discovery/` are the blessed way to add server logic).

> **Litmus test:** *"If I wrote this rule only in the desktop board and a driver
> did the same thing from the mobile app, would something bad happen?"* If yes →
> LogicBank.

## Why LogicBank can do *temporal* rules too (the tick pattern)

LogicBank **forward-chains**: change any attribute and every dependent derivation
recomputes transitively. Its one blind spot looks like *time* — a lease expiring
at midnight fires no row event. The enterprise pattern (how Val Huber / Tyler Band
run LogicBank as agent code) dissolves it: **model "now" as data and let a thin
agent poke it.**

- **`sys_clock`** — a singleton table (`today DATE`, `tick_at`), one row.
- Temporal entities carry an FK to it (`vehicle_lease.sys_clock_id`, default 1).
- A **tick agent** does one thing: `UPDATE sys_clock SET today = current_date`.
- That single parent change **cascades** — LogicBank re-derives every dependent
  formula (lease expiry, maintenance overdue, responsibility reversion). The
  agent carries **no business logic**; it's a pulse.

The same shape covers the rest of the "agents": the Smitty **sync** is a fetch
agent that just *inserts staging rows* (LogicBank then validates/derives/
allocates); an **odometer push** is a row-event that recomputes mileage-based due
dates. Agents are thin drivers; the rules are one declarative source of truth.

So the boundary is: **logic → LogicBank; driving pulses → thin agents; views →
clients.**

## Rule catalog (`als-extensions/logic_discovery/fleet_governance.py`)

Active now (tables exist):

| Rule | Type | What it guarantees |
| --- | --- | --- |
| **Dispatch lock** | `constraint` on `Load` + `Trip` | can't assign a load/trip to a vehicle that's `in_maintenance` / `out_of_service` — the closed-loop payoff of the Smitty integration (shop flips a rig, dispatch blocked everywhere) |
| **Bundle integrity** | `constraint` on `RigBundle` | power vehicle is a tractor, trailer is a trailer |
| **Spec ↔ asset type** | `constraint` on `TractorSpec` / `TrailerSpec` | a spec only attaches to the matching asset type |
| **Odometer monotonic** | `constraint` on `Vehicle` | odometer never decreases (bad telematics/typos) |
| **Lease activity** | `formula` on `VehicleLease.is_active` | `start ≤ sys_clock.today ≤ end` — **tick-driven**; expiry needs no scheduled Python |

Planned (need Phase-3 Smitty-mirror tables — stubbed in the module):

| Rule | Type | What it will do |
| --- | --- | --- |
| **Cost allocation** | `copy` | snapshot the vehicle's `maint_responsibility` onto an ingested service record so the at-cost amount is allocated to whoever was responsible *at service time* |
| **Maintenance due→overdue** | `formula` | from `sys_clock.today` (date) + `vehicle.odometer` (mileage) |
| **Responsibility resolution** | `formula` | re-derive `vehicle.maint_responsibility` from ownership + the *active* lease (lease wins), tick-driven |
| **Status from Smitty** | `row_event` | set `vehicle_status` from a Smitty in-shop / out-of-service ingest |
| **Owner-operator settlement** | `sum`/`formula` | weekly cap / settlement math (see `TODO.md`) |

## How the rules are installed (with the ALS rebuild)

We already reinstall our customizations after every ALS regenerate. `fleet_
governance.py` rides that same path — **no new step**:

1. It lives in `als-extensions/logic_discovery/`.
2. `als-extensions/install.sh` copies **`logic_discovery/*.py`** into
   `<ALS_PROJECT>/logic/logic_discovery/`, where ALS auto-discovers it and calls
   its `declare_logic()` on startup.
3. So the standing deploy step already installs it:
   ```bash
   make als-extensions ALS_PROJECT=/home/thomas/fleet-dispatcher-api
   sudo systemctl restart fleet-dispatcher-api
   ```
   Confirm on boot: the log prints
   `Fleet Dispatcher fleet governance registered (...)` alongside the comms one,
   and `..discovered logic: [... 'fleet_governance.py' ...]`.

The `sys_clock` table + seed row are part of `database/schema.sql` /
`seed_data.sql`, so they come in on the normal schema apply + ALS regenerate
(a new table means ALS must regenerate to expose `SysClock`).

## The tick agent (the only new moving part)

A few lines whose sole job is to advance the clock — deploy it whichever way fits:

```bash
# option A: cron on the box (daily just after midnight, idempotent)
0 0 * * *  psql "$DATABASE_URL" -c \
  "UPDATE fleet.sys_clock SET today = CURRENT_DATE, tick_at = now() WHERE id = 1 AND today <> CURRENT_DATE;"
```
```bash
# option B: an ALS custom endpoint POST /api/tick that does the same UPDATE,
#           called by cron/curl or the realtime service.
```
Idempotent by design: if `today` is unchanged the UPDATE is a no-op, so no cascade
fires. Advance **daily** (not per-minute) — the cascade touches every dependent
child, which is nothing for a fleet but wants the right granularity at scale.

## Trade-offs to design around

- **Cascade cost.** One `sys_clock` parent with many children means a tick
  re-derives all of them. Fine for hundreds of vehicles/leases; at tens of
  thousands, tick daily, keep it idempotent, and gate formulas so only
  near-boundary rows recompute (or shard the clock).
- **Version sensitivity.** The `Rule.formula` parent-cascade wiring
  (`sys_clock → lease → vehicle`) can vary by LogicBank version — validate on the
  Linux box (flagged in `fleet_governance.py`), the way we flag Wt spots.
- **Read-time truth.** Views may still compute "active as of now" at read time
  rather than trust only a stored flag between ticks.

## Forward-going principle

When we add a fleet behavior, ask the litmus test first. If it's an invariant,
it's a LogicBank rule in `fleet_governance.py` (or a sibling module) — reactive if
it hangs off a domain write, tick-driven if it hangs off time. Keep the clients
thin. See also `CLAUDE.md` (golden rules) and `als-extensions/README.md`.

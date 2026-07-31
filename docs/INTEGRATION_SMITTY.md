# Fleet Dispatcher ⇄ Smitty Services — vehicle integration spec

**Status:** proposal v1 (2026-07-13) · **Owners:** Fleet Dispatcher + Smitty
Services teams · **Purpose:** share **vehicle** identity + specs, **mileage**,
**operational status**, and **service/repair** data between the two apps so the
dispatcher always knows a rig's service state and the shop always knows the rig's
usage.

This document is written to be handed to the **Smitty Services** side. Sections
marked **[Smitty to confirm]** need their input; everything under "Fleet side" is
concrete against Fleet's schema.

---

## 1. Context (what we're connecting)

| | Fleet Dispatcher | Smitty Services |
| --- | --- | --- |
| Role | Dispatch & operations (who/where/what load) | Vehicle service & repair |
| Vehicle concept | `equipment` (a rig: power unit + trailer config) | a serviceable vehicle **[Smitty to confirm entity name]** |
| Stack | PostgreSQL → ApiLogicServer JSON:API + clients | ApiLogicServer JSON:API **[confirm]** |
| Deployment | **Shared** PostgreSQL 16 instance, schema-separated (`fleet`, `smitty`, `student`); Fleet also runs a **Kafka** event plane (see `REALTIME.md`) | `smitty` schema on the same instance |

Because both apps are **ALS/JSON:API on one Postgres instance** and Fleet already
has a Kafka bridge, we have three viable coupling styles (§5). The recommendation
is **loose coupling via a canonical VIN key + events**, not shared FKs.

---

## 2. Design principles

1. **VIN is the canonical cross-app key.** A 17-char VIN uniquely identifies a
   serviceable vehicle across both systems. Each app also keeps its own local id
   and stores the *other* app's id for correlation (§3).
2. **System of record (SoR) per field, not per row.** Fleet owns *operational*
   facts (identity, specs, live odometer, dispatch status); Smitty owns
   *service* facts (work orders, maintenance schedule, out-of-service windows).
   Neither app overwrites the other's owned fields — it **mirrors** them
   read-only. The ownership matrix (§4) is the contract.
3. **Mirror, don't share-write.** Each app keeps a local read-only projection of
   the fields the other owns, updated by events (or reconcile). No cross-schema
   writes, no distributed transactions.
4. **Idempotent, event-versioned.** Every event carries `event_id` +
   `schema_version` + `occurred_at`; consumers upsert idempotently so replays and
   out-of-order delivery are safe.
5. **Backfill + stream.** A one-time JSON:API reconcile seeds the mirror; events
   keep it fresh. Either side can re-request a full reconcile any time.

---

## 3. Canonical Vehicle — identity & correlation

The shared anchor. Both apps agree on these identity fields; each stores the
other's id so records line up even if a VIN is missing or later corrected.

| Field | Type | Notes |
| --- | --- | --- |
| `vin` | text(17) | **Canonical key.** Unique, validated. Primary correlation. |
| `fleet_equipment_id` | uuid | Fleet's `equipment.id`. |
| `smitty_vehicle_id` | *[Smitty type]* | Smitty's local vehicle id. **[Smitty to confirm]** |
| `unit_number` / fleet # | text | Human-facing fleet number (Fleet `equipment.unit_number`). |
| `asset_type` | enum | `tractor` \| `truck` \| `trailer` (a rig may be >1 asset — see note). |

> **Modeling note (important).** Fleet's `equipment` row today represents a **rig
> configuration** (power unit + trailer). Smitty services **individual assets**,
> each with its own VIN (the tractor has one; a trailer has its own). For v1 we
> recommend the canonical Vehicle = **one VIN'd asset**, and Fleet maps its
> `equipment` to the **power unit's** VIN as the primary serviced vehicle, with
> trailers optionally tracked as their own canonical vehicles.
> **[Decision needed — see §9.]**

---

## 4. Field ownership matrix (the contract)

**SoR** = the app allowed to write the field. The other app mirrors it read-only.

### Vehicle descriptive & operational — **Fleet is SoR**

| Field | SoR | Smitty uses it for |
| --- | --- | --- |
| `vin`, `unit_number`, `asset_type` | Fleet | identity / matching |
| `year`, `make`, `model` | Fleet | correct parts / service manuals |
| `plate`, `dot_number`, `fuel_type` | Fleet | shop records, compliance |
| `odometer_miles` + `odometer_as_of` | **Fleet** (telematics / driver reports) | mileage-based service due-dates |
| `operational_status` (`in_service` \| `out_of_service`) | Fleet | knowing if a rig is available |
| specs (`deck_length_ft`, `weight_capacity_lbs`, `has_ramps`, `has_duals`, power unit) | Fleet | service context |

### Service / repair — **Smitty is SoR**

| Field / entity | SoR | Fleet uses it for |
| --- | --- | --- |
| `work_order` (open/closed, complaint, cost, vendor, odometer-at-service) | **Smitty** | showing service history + current shop status on the vehicle detail page |
| `maintenance_schedule` (service type, interval, next due by date/odometer, overdue) | **Smitty** | warning dispatchers a rig is due/overdue before assigning it |
| `service_status` (`in_maintenance`, expected-back date) | **Smitty** | **not dispatching a rig that's in the shop** |
| `out_of_service` window (from, to, reason) | **Smitty** | availability on the board |
| repair-cost history / total cost of ownership | Smitty | Fleet's valuation tab may *display* it |

### Valuation — **[Decision needed, §9]**

Proposed: Fleet is SoR for asset **book value / for-sale price** (Feature 6);
Smitty **informs** it by publishing repair-cost history. Confirm with Smitty.

---

## 5. Sync mechanism — options & recommendation

All three work over the shared instance; they differ in coupling.

| Option | How | Pros | Cons |
| --- | --- | --- | --- |
| **A. Events (recommended)** | Each app publishes changes to canonical Kafka topics; the other consumes → upserts its local mirror. Bulk reconcile via JSON:API on startup. | Loose coupling; near-real-time; survives one app moving to a separate DB; Fleet already has the Kafka plane + producer pattern | Needs Smitty to produce/consume Kafka (or a small adapter) |
| **B. Shared cross-schema views** | On the one instance, a `shared` schema (or grants) exposes read-only canonical views; each app's ALS reflects what it needs | Simplest reads; always consistent (no lag) | Tight DB coupling; breaks if apps ever split DBs; ALS reflection cross-schema is fiddly |
| **C. API-to-API pull** | Each app polls the other's JSON:API (`GET /Equipment`, `GET /WorkOrder`) on a schedule | No new infra; uses existing JSON:API + JWT | Polling lag + load; auth plumbing both ways; no push |

**Recommendation: A (events) for the steady state + C (JSON:API) for the initial
backfill and on-demand reconcile.** It matches Fleet's existing architecture
(one Kafka topic per row type + a correlation id — see `REALTIME.md`), keeps the
two schemas independent, and degrades gracefully (if events lag, a reconcile
pull re-syncs). Use **B only** if Smitty can't run a Kafka consumer near-term —
then a shared read-only `vehicle` view is the pragmatic bridge.

---

## 6. Event contract (Option A)

**Envelope** (every message):

```json
{
  "event_id": "uuid",              // idempotency key (dedupe on this)
  "event_type": "VehicleUpserted",
  "schema_version": "1.0",
  "occurred_at": "2026-07-13T12:00:00Z",
  "source_app": "fleet" | "smitty",
  "vin": "1FUJGLDR9CLBP8834",      // = Kafka message key (ordering per vehicle)
  "payload": { ... }               // event-specific, below
}
```

Kafka **key = VIN** so all events for one vehicle stay ordered on one partition
(same strategy Fleet uses for `channel_id` on the `message` topic).

### Topic `vehicle.v1` — Fleet → Smitty (Fleet-owned facts)

- **`VehicleUpserted`** — `{ vin, fleet_equipment_id, unit_number, asset_type, year, make, model, plate, dot_number, fuel_type, specs:{deck_length_ft, weight_capacity_lbs, has_ramps, has_duals, power_unit} }`
- **`OdometerReported`** — `{ vin, odometer_miles, as_of }` (throttled, e.g. daily or on ≥N-mile delta — don't stream every GPS ping)
- **`OperationalStatusChanged`** — `{ vin, status: "in_service"|"out_of_service", reason, effective_from }`
- **`VehicleRetired`** — `{ vin, retired_on, reason }`

### Topic `vehicle-service.v1` — Smitty → Fleet (Smitty-owned facts)

- **`WorkOrderOpened`** — `{ work_order_id, vin, complaint, opened_at, odometer_at_service, vendor, expected_ready_at }`
- **`WorkOrderClosed`** — `{ work_order_id, vin, closed_at, total_cost, summary, lines:[{service_type, parts_cost, labor_cost}] }`
- **`ServiceStatusChanged`** — `{ vin, service_status: "in_maintenance"|"available", expected_ready_at }` → Fleet flags the rig un-dispatchable
- **`MaintenanceDue`** — `{ vin, service_type, due_on, due_odometer, severity: "upcoming"|"due"|"overdue" }`
- **`OutOfService`** — `{ vin, from, to, reason }`

**Idempotency:** consumers upsert by `event_id` (and ignore an `occurred_at`
older than the last applied for that `(vin, field-group)`).

---

## 7. Backfill & on-demand reconcile (Option C)

- **Initial load:** each app pulls the other's canonical collection once and
  seeds its mirror:
  - Smitty ← Fleet: `GET /api/Equipment` (+ the new asset fields).
  - Fleet ← Smitty: `GET /api/WorkOrder`, `GET /api/MaintenanceSchedule` **[confirm resource names]**.
- **Reconcile trigger:** either side can request a full re-publish (e.g. after
  downtime) — a `ReconcileRequested { source_app, since }` control message, or
  simply re-run the JSON:API pull.
- **Auth:** JSON:API calls use the ALS **JWT**; the two services share the
  `SECRET_KEY` already used between Fleet's ALS and its realtime bridge, or a
  dedicated service-account token. **[Confirm token strategy with Smitty.]**

---

## 8. Canonical schema sketch (neutral — each app adopts/mirrors)

Presented app-neutrally; Fleet implements in the `fleet` schema (UUID domain
keys, integer lookups, real FKs — Fleet's conventions). Smitty mirrors the
Smitty-owned tables as its SoR and keeps a read-only `vehicle` projection.

```sql
-- Canonical vehicle (identity + Fleet-owned facts). Fleet SoR.
vehicle (
  vin                text primary key,          -- 17-char, validated
  fleet_equipment_id uuid,                       -- Fleet local id
  smitty_vehicle_id  text,                        -- Smitty local id (correlation)
  unit_number        text,
  asset_type         text,                        -- tractor|truck|trailer
  year int, make text, model text,
  plate text, dot_number text, fuel_type text,
  odometer_miles     int,
  odometer_as_of     timestamptz,
  operational_status text,                        -- in_service|out_of_service
  updated_at         timestamptz
);

-- Service history (Smitty SoR; Fleet mirrors read-only).
work_order (
  id text primary key, vin text references vehicle(vin),
  status text, complaint text,
  opened_at timestamptz, closed_at timestamptz,
  odometer_at_service int, vendor text,
  total_cost numeric(12,2)
);
work_order_line (
  id text primary key, work_order_id text references work_order(id),
  service_type text, parts_cost numeric(12,2), labor_cost numeric(12,2), notes text
);

-- Upcoming service (Smitty SoR; Fleet mirrors to warn dispatchers).
maintenance_schedule (
  id text primary key, vin text references vehicle(vin),
  service_type text,
  interval_miles int, interval_days int,          -- one or both (see §9 Q2)
  next_due_on date, next_due_odometer int,
  status text                                      -- upcoming|due|overdue
);

-- Availability windows (Smitty SoR).
out_of_service (
  id text primary key, vin text references vehicle(vin),
  from_ts timestamptz, to_ts timestamptz, reason text
);
```

### Fleet-side mapping (concrete)

- `vehicle` ← Fleet `equipment` (+ the Feature-6 asset fields: `vin`, `year`,
  `make`, `model`, `odometer_miles`, `status`, …). Fleet `equipment.in_service`
  → `operational_status`.
- Fleet **consumes** `work_order` / `maintenance_schedule` / `out_of_service` to
  render the **Vehicle detail page** (Maintenance tab) and to flag rigs on the
  **board** that are in the shop or overdue.

---

## 9. Open decisions **[for the Smitty thread]**

1. **Canonical granularity** — is the shared "vehicle" the **power unit only**
   (tractor/truck), or do we also sync **trailers** as their own VIN'd vehicles?
   (Fleet's `equipment` bundles a rig; Smitty likely services individual units.)
2. **Maintenance interval basis** — schedule by **mileage**, **time**, or
   **both, whichever comes first**? (Drives the `maintenance_schedule` shape.)
3. **Sync mechanism** — can Smitty **produce/consume Kafka** (Option A), or
   should we start with a **shared read-only view / JSON:API pull** (B/C)?
4. **Valuation SoR** — does Smitty want to own repair-cost-driven value, or just
   **publish cost history** and let Fleet compute book/for-sale value?
5. **Identity when VIN is missing/dirty** — fallback correlation key (fleet
   unit # ↔ Smitty asset #) and who's authoritative on a VIN conflict?
6. **Resource names & auth** — Smitty's JSON:API resource names for work orders /
   schedules, and the **token** the two services use to call each other.

---

## 10. Phased rollout

- **P1 — Identity & backfill:** agree the canonical `vehicle` fields + VIN key;
  Fleet adds the asset fields (Feature 6); one-time JSON:API reconcile both ways.
- **P2 — Fleet → Smitty stream:** `vehicle.v1` events (upsert, odometer, status)
  so Smitty always has current mileage + which rigs are active.
- **P3 — Smitty → Fleet stream:** `vehicle-service.v1` events; Fleet shows service
  status/history on the vehicle detail page and flags in-shop/overdue rigs on the
  board.
- **P4 — Closed loop:** dispatch respects `service_status` (won't assign a rig
  that's `in_maintenance`); optional "schedule service" action from Fleet that
  opens a Smitty work order.

---

*Fleet-side references: `docs/domain-model.md` (equipment/vehicle), `docs/TODO.md`
Feature 6 (vehicle detail & lifecycle — the asset fields this depends on),
`docs/REALTIME.md` (Kafka topic strategy this mirrors), `docs/DEPLOYMENT.md`
(shared-instance schema separation).*

---

# Smitty response v1

**Date:** 2026-07-13 · **Owner:** Smitty Services team ·
**Status:** proposal, awaiting Fleet review

Answers to §9's six open questions and the three cross-cutting constraints
Smitty is holding to. Concrete Phase 1 work items are captured in
`tasks/TODO.md §5 Fleet Dispatcher integration` — read that for the
checklist; this section is the *position* the checklist is built against.

## R.1 Smitty state today (facts, not aspirations)

| Concept | Smitty today | Gap Fleet needs to know about |
| --- | --- | --- |
| Vehicle | `vehicles` table. `vehicle_id SMALLINT` PK, `vin VARCHAR(30) NOT NULL UNIQUE`, `year`, `make`, `model`, `license_plate`, `notes`, `customer_id` FK. ALS resource name: **`Vehicle`**. | No odometer, no operational_status, no dot/fuel/asset_type, no specs, no fleet_equipment_id correlation. Additive migration adds them. |
| Work order | `jobs` table + `job_parts` / `job_labor_items` / `job_notes` / `job_purchases`. ALS resource: **`Job`** (not `WorkOrder`). Lifecycle: `New → In Progress → Complete → Invoiced`. | Fleet consumers should map `WorkOrder ↔ /api/Job`. Cost fields already present: `estimated_cost`, `actual_cost`, `parts_total`, `labor_total`, `job_total`. |
| Maintenance schedule | *does not exist* | Phase 1 adds `maintenance_schedules` table + `/api/MaintenanceSchedule` resource. |
| Out-of-service window | *does not exist* | Phase 1 adds `vehicle_out_of_service` table + `/api/VehicleOutOfService` resource. |
| Auth | bcrypt + username/password + Wt session. **No JWT support.** ALS is currently open on the LAN. | Phase 1 adds shared-service-token check for cross-app calls. |
| Kafka | *not in the stack* — Wt + ApiLogicServer + Postgres only. | Deferred to Phase 5. Phase 1-3 use JSON:API pull only. |

## R.2 Cross-cutting constraints Smitty is holding to

These bound every schema and code change in the integration:

**C.1 — Schema changes are additive-only.** No `DROP COLUMN`, no renames,
no type changes. Every migration is `ADD COLUMN IF NOT EXISTS` on existing
tables and `CREATE TABLE IF NOT EXISTS` for new ones. Deprecated fields stay
in place, become nullable, and just stop being written. This preserves the
ability to roll back a Smitty deploy without losing correlation data Fleet
has already published into the mirror.

**C.2 — Byte-identical DDL on both sides for shared tables.** The extended
`vehicles` shape, the new `maintenance_schedules` table, and
`vehicle_out_of_service` are **the same DDL** on both Smitty and Fleet
Dispatcher, each maintained in its own schema (`smitty.*` / `fleet.*`).
Not "compatible" — literally the same column list, types, defaults. The
sync layer replicates rows, not translates them. Divergence in these
tables is a bug.

**C.3 — Fleet Dispatcher holds the on/off switch.** Smitty does not have a
matching toggle; it is a passive party. When Fleet's admin turns
integration off, Smitty's mirror fields simply go stale (or NULL where
they were never published). Smitty's UI must degrade gracefully — no
dependency on the mirror being fresh, no error surface when Fleet is silent.

## R.3 Answers to §9's open questions

**Q1 — Canonical granularity: one VIN per canonical vehicle.**
Every serviced asset — power unit *and* trailer — is its own Smitty
`vehicles` row today, keyed by VIN. Fleet's `equipment` rig-bundling maps
to *N* Smitty vehicles; a single Fleet equipment row can point at (say) a
tractor + trailer as two separate Smitty vehicle rows via VIN, and the
sync layer keeps both agreed. Rig-level canonical vehicle would collapse
Smitty's per-asset service history and is a non-starter.

**Q2 — Maintenance interval basis: both, whichever comes first.**
`maintenance_schedules` carries `interval_miles NULL` and
`interval_days NULL`. Either or both may be set on a row. `next_due` is
the earlier of (mileage-based next-due, date-based next-due); the app
computes both when the row is written, and re-computes on each odometer
push from Fleet. Time-only items (annual DOT inspection) leave
`interval_miles` NULL; mileage-only items (oil change) leave
`interval_days` NULL; mixed items (brake fluid: 24 months **or** 25 000
miles) set both.

**Q3 — Sync mechanism: JSON:API pull for Phase 1-3, Kafka in Phase 5.**
Smitty is a C++ Wt frontend against ApiLogicServer; a native Kafka
producer/consumer in that stack is disproportionate for the initial
integration. Both apps' ALS already speaks JSON:API. Phase 1-3 uses
scheduled pull from either direction, coalescing at ALS. Phase 5 adds a
Python-side event producer on ALS's row-change hooks — a small module,
does not touch the Wt frontend. Fleet's existing Kafka plane sees a
`vehicle-service.v1` topic emitted from Smitty's ALS.

**Q4 — Valuation SoR: Smitty publishes cost history, Fleet computes value.**
Job / JobPart / JobLaborItem already track repair costs at per-service
granularity and are exposed as `/api/Job`. Fleet reads that, rolls up
into asset valuation on its side. Rolling total-cost-of-ownership into
Smitty's model would drag Smitty into asset-lifecycle territory it does
not currently model — different consumers, different lifecycle.

**Q5 — Identity fallback when VIN is missing/dirty.**
Correlation order: `vin` → `(customer_id, license_plate)` → `unit_number`
via the correlation-mapping table both sides maintain
(`fleet_equipment_id` on Smitty's vehicles is the reverse pointer). On a
VIN conflict between the two sides, **Fleet wins** — telematics + DOT
compliance authoritative, Smitty applies the correction via the next
scheduled pull. If integration is off (per C.3), Smitty stays on its
last-known VIN until Fleet resumes publishing.

**Q6 — Resource names + auth.**
- **Smitty resource names Fleet should call:** `/api/Vehicle`,
  `/api/Job` (work order), `/api/JobPart`, `/api/JobLaborItem`,
  `/api/JobNote`, `/api/MaintenanceSchedule` (Phase 1 new),
  `/api/VehicleOutOfService` (Phase 1 new). No `WorkOrder` or
  `WorkOrderLine` — Fleet's docs should map to Smitty's names.
- **Auth for Phase 1-3:** shared service token in `X-Service-Token`
  header, checked by a small ALS middleware. Env-var `SMITTY_SERVICE_TOKEN`
  on both sides. Combined with LAN-only exposure since we share a
  Postgres instance. Not JWT — that's a Phase 5 discussion when Kafka
  lands and cross-service auth gets a bigger conversation.

## R.4 Shared schema (identical on both sides, additive to existing)

Applies verbatim to Smitty's `smitty.*` schema and Fleet's `fleet.*`
schema. Both sides commit the same DDL patch, adjusted only for the
local `vehicles` / `equipment` table name — the *columns* being added
are identical.

```sql
-- 1. ADDITIVE columns on the local vehicle table (both sides).
--    Fleet applies these to fleet.equipment (or fleet.vehicles if that
--    is the renamed target); Smitty applies to smitty.vehicles.
--    Column list is byte-identical.
ALTER TABLE vehicles
    ADD COLUMN IF NOT EXISTS fleet_equipment_id  VARCHAR(60),
    ADD COLUMN IF NOT EXISTS odometer_miles      INTEGER,
    ADD COLUMN IF NOT EXISTS odometer_as_of      TIMESTAMP,
    ADD COLUMN IF NOT EXISTS operational_status  VARCHAR(20),   -- in_service | out_of_service
    ADD COLUMN IF NOT EXISTS asset_type          VARCHAR(20),   -- tractor | truck | trailer
    ADD COLUMN IF NOT EXISTS dot_number          VARCHAR(20),
    ADD COLUMN IF NOT EXISTS fuel_type           VARCHAR(20),
    ADD COLUMN IF NOT EXISTS specs               JSONB;

-- 2. maintenance_schedules — new table, identical both sides.
--    vehicle_id FK targets the LOCAL vehicles table. vin is the
--    denormalized correlation key that survives cross-app matching.
CREATE TABLE IF NOT EXISTS maintenance_schedules (
    schedule_id       SMALLINT     PRIMARY KEY GENERATED BY DEFAULT AS IDENTITY,
    vehicle_id        SMALLINT     NOT NULL,
    vin               VARCHAR(30),
    service_type      VARCHAR(60)  NOT NULL,
    interval_miles    INTEGER,
    interval_days     INTEGER,
    next_due_on       DATE,
    next_due_odometer INTEGER,
    status            VARCHAR(20)  NOT NULL DEFAULT 'upcoming',
    created_at        TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at        TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    created_by        VARCHAR(60)
);

CREATE INDEX IF NOT EXISTS idx_maintenance_schedules_vin
    ON maintenance_schedules (vin);
CREATE INDEX IF NOT EXISTS idx_maintenance_schedules_vehicle
    ON maintenance_schedules (vehicle_id, status);

-- 3. vehicle_out_of_service — new table, identical both sides.
CREATE TABLE IF NOT EXISTS vehicle_out_of_service (
    oos_id      SMALLINT     PRIMARY KEY GENERATED BY DEFAULT AS IDENTITY,
    vehicle_id  SMALLINT     NOT NULL,
    vin         VARCHAR(30),
    from_ts     TIMESTAMP    NOT NULL,
    to_ts       TIMESTAMP,
    reason      TEXT,
    created_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    created_by  VARCHAR(60)
);

CREATE INDEX IF NOT EXISTS idx_vehicle_oos_vin
    ON vehicle_out_of_service (vin);
CREATE INDEX IF NOT EXISTS idx_vehicle_oos_active
    ON vehicle_out_of_service (vehicle_id, to_ts);
```

Note the deliberate omission of cross-app FKs. `vehicle_id` inside these
new tables points at the LOCAL vehicle table. `vin` is the cross-app
correlation carried as a denormalized column so an unmatched-VIN row on
one side still round-trips cleanly. This is what makes C.3 (Fleet holds
the toggle) safe — Smitty can operate on the tables with Fleet silent
and no dangling FKs.

## R.5 Phased plan (Smitty side)

Detailed checklist lives in `tasks/TODO.md §5`. Summary:

- **Phase 1 — Schema + token.** Additive DDL patch (rma-parallel style),
  service-token middleware on ALS, one-shot reconcile script. No UI
  changes. Fleet applies identical DDL its side.
- **Phase 2 — Fleet → Smitty pull.** Scheduled reconcile from Fleet's
  `/api/Equipment` into Smitty's `vehicles`. Log-only failure surface.
- **Phase 3 — Smitty → Fleet publish (readable).** Fleet pulls Smitty's
  `/api/Job`, `/api/MaintenanceSchedule`, `/api/VehicleOutOfService`
  through the same shared-token endpoint. No Smitty code change beyond
  Phase 1's middleware.
- **Phase 4 — Maintenance UI on Smitty.** VehicleDetail Maintenance tab
  showing upcoming/due/overdue + current OOS windows; Schedule Service
  and Start/End OOS actions. This is the first user-visible slice.
- **Phase 5 — Optional Kafka bridge.** Python-side producer on ALS
  row-change hooks emits `vehicle-service.v1`. Consumer for
  `vehicle.v1` (odometer / status) replaces or augments the Phase 2
  pull. C++ frontend never touches Kafka.

## R.6 Sequencing versus in-flight Smitty work

Landing Phase 1 pushes **§4b RMA Phase 1 Stage 2** (Inventory plumbing +
UI wiring) further out — RMA schema is on main but the C++ wiring
hasn't started. If Fleet's timeline is urgent, the integration outranks
RMA and RMA UI slides. If Fleet is comfortable waiting a cycle, we
finish RMA Stages 2-3 first and start integration after. Explicit call
from the Fleet + Smitty owners on which slot.

---

# Fleet response v1

**Date:** 2026-07-31 · **Owner:** Fleet Dispatcher team ·
**Status:** proposal, answers Smitty response v1

Smitty's v1 is accepted almost wholesale. The one place we diverge is C.2
("byte-identical DDL"); the rest we adopt. Below are Fleet's positions and the
one model change Fleet is making on its side to align with Smitty's per-asset
vehicle model.

## F.1 Granularity — Fleet moves to per-asset vehicles (agrees with Q1)

Decision: **each physical asset — a power unit (tractor) OR a trailer — is its
own Fleet entity with its own VIN.** This matches Smitty's one-VIN-per-vehicle
model exactly, so the canonical `vehicle` is 1:1 across both systems by VIN.

This replaces Fleet's current `equipment` row (which bundled a power unit +
trailer as one unit). The rig-bundling idea is still valuable — Fleet keeps it,
but as a **separate, time-effective association**, not the vehicle identity:

- **`vehicle`** — the per-asset entity (tractor or trailer), VIN-keyed. This is
  the canonical thing that syncs with Smitty.
- **`rig_bundle`** — a **temporal combination**: which power unit + trailer +
  **driver(s)** are working together, with `effective_from` / `effective_to`.
  Supports **teams** (2+ drivers on one bundle). Full history: "who and what
  was combined at any point in time" is a query against the effective window.

Only `vehicle` crosses the integration boundary (VIN). `rig_bundle` is a
**Fleet-internal dispatch concept** — Smitty services individual assets and
doesn't need the bundle. This cleanly separates the shared canonical (vehicle)
from Fleet's operational grouping (bundle).

## F.2 Specs are typed per asset type (not one JSONB shape)

Tractor specs and trailer specs are genuinely different domains (engine /
horsepower / sleeper vs deck length / weight capacity / ramps / duals). Fleet
keeps them as **typed, per-type value objects** — `tractor_spec` and
`trailer_spec`, each 1:1 with `vehicle` by `asset_type`. This is a Fleet golden
rule (typed columns + real FKs, not JSON blobs).

For the Smitty sync, Fleet **serializes the relevant typed specs into the shared
`specs` JSON on publish** — so Smitty still receives `specs` per C.2's column,
but Fleet doesn't store a JSONB blob natively. Serialize on the wire, typed at
rest.

## F.3 On C.2 "byte-identical DDL" — counter-proposal

We can't take C.2 literally: Fleet's conventions are **UUID primary keys + real
FK relationships + typed spec columns**, while Smitty's shared tables use
`SMALLINT` PKs, `specs JSONB`, and no FKs. A `SMALLINT vehicle_id` can't
reference Fleet's `vehicle(id) UUID`, so identical DDL breaks Fleet's model.

Proposed contract (already how VIN correlation is designed to work): **shared
field set + semantics + VIN as the correlation key; each side keeps its native
PK type and its own local FKs.** The sync **replicates by VIN**, not by identical
row images. Concretely:
- Fleet `vehicle`: `id UUID`, `smitty_vehicle_id SMALLINT` (correlation), `vin`.
- Smitty `vehicles`: `vehicle_id SMALLINT`, `fleet_equipment_id`→(now
  `fleet_vehicle_id`), `vin`.
- The **columns and their meaning** are identical; the **PK type is native** to
  each app.

For the Smitty-owned tables Fleet consumes (`MaintenanceSchedule`,
`VehicleOutOfService`, `Job`), Fleet reads them **via `/api/*` (pull)** and either
mirrors into Fleet-native tables keyed by UUID+VIN, or just caches — no need for
Fleet to hold `SMALLINT`-keyed mirror tables. Divergence is still a bug; we just
correlate on VIN + field semantics instead of demanding identical PKs.

## F.4 Accepted from Smitty v1 (no change needed)

- **C.1 additive-only** migrations — yes.
- **C.3 Fleet holds the toggle** — yes; Fleet owns the integration on/off and
  Smitty degrades gracefully when Fleet is silent.
- **Q2 both intervals, whichever first** — yes.
- **Q3 JSON:API pull for Phase 1–3, Kafka at Phase 5** — yes. Fleet builds a
  scheduled poller now; our existing Kafka producer slots in at Phase 5 (Fleet
  can also emit `vehicle.v1` for Smitty's optional consumer then).
- **Q4 Smitty publishes cost via `/api/Job`; Fleet computes valuation** — yes.
- **Q5 correlation `vin → (customer_id, license_plate) → unit_number`; Fleet wins
  VIN conflicts** — yes.
- **Q6 resource-name mapping** — Fleet's consumer maps `WorkOrder → /api/Job`,
  and calls `/api/Vehicle`, `/api/MaintenanceSchedule`, `/api/VehicleOutOfService`.
  **Auth: shared `X-Service-Token`** (`SMITTY_SERVICE_TOKEN` env, LAN-only, kept
  out of git, rotated). Fleet adds send + verify middleware on its ALS. JWT is a
  Phase 5 conversation.

## F.5 Fleet-side migration scope (heads-up, not blocking Smitty)

Per-asset vehicles + temporal bundle is a **significant internal refactor** of
Fleet's fleet aggregate — `equipment` (rig) → `vehicle` (asset) + `rig_bundle`
(temporal combination). It touches `load`, `trip`, `position_report`,
`driver_equipment`, the seed data, the desktop board/fleet views, and mobile.
It does **not** block the integration contract above (the shared surface is
`vehicle` by VIN + reading Smitty's `/api/Job` etc.), but it is the larger piece
of Fleet-side work and will be phased (see Fleet `docs/TODO.md`).

## F.6 Open item back to Smitty

- Smitty's `vehicles.fleet_equipment_id` should become **`fleet_vehicle_id`**
  (Fleet now keys the canonical asset as `vehicle`, not `equipment`). Same
  correlation, renamed target. Additive per C.1.

## Sequencing (R.6)

**[Fleet owner to confirm]** — integration ahead of Smitty's RMA work, or after.
Not yet decided.

# Integration boundary — Smitty as a profit center (Fleet, 2026-07-31)

New fact from the Fleet side that refines the whole contract: **Smitty's
garage/staff is a separate profit center** that services **third-party rigs**,
not only Fleet's fleet. This makes the two vehicle sets asymmetric and tightens
a few rules — no schema change to Feature 7, but it changes how the sync scopes.

## B.1 Fleet's vehicles are a *subset* of Smitty's

```
   Smitty `vehicles`  ⊇  { Fleet-owned rigs }  ∪  { third-party customer rigs }
   Fleet  `vehicle`   =    Fleet-owned rigs only
```

- **Correlation is partial.** `fleet_vehicle_id` (and the matching VIN) is set
  **only** on Smitty vehicles that belong to Fleet. Third-party vehicles have
  `fleet_vehicle_id = NULL` and never appear on the Fleet side.
- Fleet **ignores unknown VINs.** When Fleet reconciles/pulls from Smitty, any
  `Vehicle` / `Job` / `MaintenanceSchedule` / `VehicleOutOfService` whose VIN
  Fleet doesn't own is dropped — it's another customer's data.

## B.2 Fleet is one *customer* of the garage

Smitty already has `customer_id` on `vehicles` (per Smitty response R.1). Fleet
is modeled as **the house / internal customer** — one `customer` row among the
third parties. Practically:
- **Scoping the sync:** the cleanest filter for Fleet↔Smitty calls is Smitty's
  **Fleet customer_id** (all Fleet rigs share it), with VIN as the per-asset key.
  A service-token request from Fleet should be scoped to that customer so Smitty
  never exposes third-party vehicles/jobs across the boundary.
- **Privacy:** third-party customer data (their VINs, jobs, costs) must **not**
  cross to Fleet. The subset filter (B.1) + customer scoping (here) enforce that.

## B.3 Cost is confidential — Fleet-job dollars do NOT cross the boundary (refines/overrides Q4)

**Correction to Q4 / F.4.** Fleet-vehicle **job costs never cross the integration
boundary** — not the invoiced amount, not the at-cost parts/labor. Servicing a
Fleet rig at Smitty is an internal, inter-company transaction whose pricing is
frequently **absorbed ("eaten") or heavily discounted**; exposing those dollars
across the boundary would leak the internal pricing arrangement (and, given
Smitty's public/third-party context, must be treated as non-shareable).

So the boundary carries **service facts, not service money**:

| Crosses to Fleet (for Fleet-owned VINs) | Stays in Smitty's ledger (never crosses) |
| --- | --- |
| work performed, service type, dates, odometer-at-service, vendor | `total_cost`, `parts_total`, `labor_total`, `estimated/actual_cost` |
| `maintenance_schedule` (due/overdue), `out_of_service`, in-shop status | any per-part / per-labor pricing on Fleet Jobs |

Implications:
- The Fleet↔Smitty pull of `/api/Job` (and JobPart/JobLaborItem) for Fleet VINs
  must be **cost-field-stripped** — Smitty returns the service record without the
  money columns, or Fleet's consumer drops them on ingest. (Third-party Jobs
  never cross at all, per B.1/B.2.)
- **Valuation/TCO (Feature 6) is computed from Fleet's OWN cost basis**, not from
  Smitty's discounted/eaten invoice. Fleet may still record its internal service
  cost separately in its own ledger; that number is a Fleet-side figure, not
  sourced across the boundary.
- Net: the two directions of confidentiality are symmetric — third-party data
  never reaches Fleet (B.1/B.2), and Fleet-job **pricing** never leaves Smitty.
  Only the operational service history crosses.

## B.4 Asks back to Smitty

1. Confirm **Fleet has a dedicated `customer_id`** (the house customer) so both
   sides can scope Fleet↔Smitty traffic to it.
2. The service-token endpoints Fleet calls (`/api/Job`, `/api/MaintenanceSchedule`,
   `/api/VehicleOutOfService`, `/api/Vehicle`) should be **filterable by
   customer_id and/or a set of VINs**, so Fleet only ever receives Fleet-owned
   rows — never third-party data.
3. `fleet_vehicle_id` stays **nullable** on `vehicles` (only Fleet rigs set it) —
   already implied by additive C.1, restated here for the third-party case.
4. The Fleet-facing `/api/Job` (+ JobPart/JobLaborItem) responses must **omit the
   cost/money fields** for Fleet VINs (`total_cost`, `parts_total`, `labor_total`,
   `estimated/actual_cost`, per-line pricing) — service facts only, no dollars
   (see B.3). Easiest as a cost-stripped projection/view for the Fleet token.

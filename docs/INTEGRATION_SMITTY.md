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

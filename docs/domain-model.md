# Domain Model

This is the heart of the project: the **shared domain language**. The schema
(`database/schema.sql`), the generated ApiLogicServer API, both portals, and our
conversations all use the nouns and verbs defined here. Change the language here
first, then propagate. Consistent terms keep design unambiguous and reviews fast.

## Ubiquitous language

| Term                | Meaning                                                                                          |
| ------------------- | ------------------------------------------------------------------------------------------------ |
| **Fleet**           | The organization's collection of drivers and equipment.                                          |
| **Driver**          | A person who hauls loads. A **Company Driver** (30%) or an **Owner-Operator** (70%).              |
| **Equipment / Rig** | A truck/trailer configuration (e.g. tractor + step-deck w/ ramps, RGN low-boy, 52' flatbed).     |
| **Tractor**         | A semi truck power unit.                                                                          |
| **Trailer**         | The towed unit: Step-Deck (52'), RGN (Removable Gooseneck / low-boy), Flatbed (52'), Car Carrier.|
| **Straight Truck**  | A RAM 3500/4500 power unit (often with duals) pulling, e.g., a car carrier.                       |
| **Load**            | A shipment to haul. The central unit of work for dispatch and settlement.                        |
| **Commodity**       | What is being hauled: vehicles, power generators, lifts, farm equipment, …                       |
| **Shipper**         | The party releasing the freight at the pickup.                                                   |
| **Receiver**        | The party accepting the freight at the drop-off.                                                 |
| **Location**        | A physical address used as a pickup or drop-off point.                                            |
| **Dead-head**       | Unloaded miles driven to reach the pickup.                                                        |
| **Loaded miles**    | Miles driven with freight aboard, from pickup to drop-off.                                        |
| **Broker**          | An intermediary who takes a cut; the **Rate** we record is *after* the broker's take.            |
| **Rate**            | Money paid for a load, after the broker's take. The basis for settlement.                        |
| **Run type**        | **Long-haul** or **Regional**.                                                                   |
| **Dispatch Week**   | A scheduling window running **Monday 00:00 → the following Monday 00:00**.                        |
| **Settlement**      | The computation of a driver's pay for a load (or a week): `rate × contract percentage`.          |
| **ELD**             | Electronic Logging Device — drivers log hours / status from the mobile app.                      |
| **Post-Trip Inspection** | Driver's end-of-trip equipment inspection.                                                  |

### Actors / roles

| Role           | Portal              | Responsibility                                                            |
| -------------- | ------------------- | ------------------------------------------------------------------------- |
| **Dispatcher** | Desktop (C++/Wt)    | Receives and dispatches new loads; manages the weekly board.              |
| **Driver**     | Mobile (Ionic)      | Takes loads, drives, performs post-trip inspection, logs fuel/DEF/oil via ELD, manages own schedule. |
| **Updater**    | Mobile (Ionic)      | Supports communication between drivers and dispatchers (one+ per dispatcher). |

## Bounded contexts

```
        ┌───────────────────────┐        ┌───────────────────────┐
        │     Fleet Context     │        │   Dispatch Context    │
        │  Driver, Equipment    │◀──────▶│  Load, DispatchWeek,  │
        │                       │  driver │  Shipper, Receiver,   │
        └───────────┬───────────┘  /equip │  Location, Commodity  │
                    │                      └───────────┬───────────┘
                    │                                  │ rate, %
                    ▼                                  ▼
        ┌───────────────────────┐        ┌───────────────────────┐
        │   Identity & Access   │        │  Settlement Context   │
        │   User, Role          │        │  Settlement           │
        └───────────────────────┘        └───────────────────────┘
```

1. **Fleet** — drivers and the equipment they run. Owns driver contract type and
   equipment configurations.
2. **Dispatch** — the operational core: loads, the weekly board, and the parties
   and places involved. Owns load lifecycle and scheduling invariants.
3. **Settlement** — turns a completed/dispatched load's rate into driver pay
   using the contract percentage from Fleet.
4. **Identity & Access** — users and the three roles (Dispatcher, Driver,
   Updater).

The **Driver** is shared by Fleet, Dispatch, and Settlement; Fleet is its
*owning* context. Other contexts reference it by identity (`driver_id`).

## Aggregates

An aggregate is a consistency boundary. Only the **root** is referenced from
outside; everything inside is loaded and saved as a unit.

### Driver  *(aggregate root — Fleet)*
- Identity: `driver_id`
- State: name, contact, `DriverType` (COMPANY | OWNER_OPERATOR), home base.
- Holds: the equipment it is qualified/assigned to operate.
- **Invariants**
  - `DriverType` determines the contract percentage (Company 30%, Owner-Operator
    70%) — see `Percentage` value object; the percentage is *derived*, not freely
    set.

### Equipment  *(aggregate root — Fleet)*
- Identity: `equipment_id`
- State: an `EquipmentConfiguration` value object (power unit + trailer + options
  such as ramps / deck length), status.
- **Invariants**
  - Configuration must be one of the known, valid power-unit/trailer combinations.

### Load  *(aggregate root — Dispatch)*
- Identity: `load_id`
- References: `driver_id`, `equipment_id`, `dispatch_week_id`, `shipper_id`,
  `receiver_id`, `commodity_id`, pickup `Location`, drop-off `Location`.
- State: `RunType` (LONG_HAUL | REGIONAL), `Distance` dead-head, `Distance`
  loaded, `Money` rate (post-broker), `LoadStatus`.
- **Invariants**
  - `rate ≥ 0`; distances `≥ 0`.
  - Pickup and drop-off locations must differ.
  - The assigned equipment must be compatible with the commodity (e.g. a car
    carrier hauls vehicles; an RGN hauls heavy/over-height equipment).
  - A load belongs to exactly one dispatch week.

### DispatchWeek  *(aggregate root — Dispatch)*
- Identity: `dispatch_week_id` (the Monday that opens the week).
- A `DateRange` value object: `[Monday 00:00, next Monday 00:00)`.
- Conceptually owns the set of loads assigned to drivers that week.
- **Invariants**
  - A driver may hold **at most `MAX_LOADS_PER_WEEK` (4)** loads in a week
    (soft target 3). Enforced by `DispatchService` and by a LogicBank count rule.

### Settlement  *(aggregate root — Settlement)*
- Identity: `settlement_id`
- References: `driver_id`, `load_id` (or a dispatch week for weekly settlement).
- State: `Money` gross rate, `Percentage` contract share, `Money` driver pay.
- **Invariants**
  - `driver_pay = rate × contract_percentage`.
  - Cost responsibility (maintenance, fuel, tolls, repairs) is informational and
    follows `DriverType`: the **company** bears them for a Company Driver; the
    **owner** bears them for an Owner-Operator. Settlement records *who* is
    responsible but does not deduct them from the percentage in this baseline.

## Value objects

Immutable, equality-by-value, no identity:

| Value object             | Fields / meaning                                                         |
| ------------------------ | ------------------------------------------------------------------------ |
| `Money`                  | amount (Decimal) + currency (USD). No negative rates.                    |
| `Percentage`             | 0–100; `Percentage.for_driver_type(...)` → 30 or 70.                     |
| `Distance` (Miles)       | non-negative miles; dead-head and loaded.                                |
| `Address` / `Location`   | line1, line2, city, state, postal_code, country.                         |
| `DateRange`              | start/end; `DispatchWeek.range` is a Monday→Monday `DateRange`.          |
| `EquipmentConfiguration` | power unit + trailer type + options (ramps, deck length, duals).         |

## Domain services

Behavior that doesn't naturally belong to a single entity:

- **`DispatchService`** — assigns a `Load` to a `Driver`/`Equipment` for a
  `DispatchWeek`, enforcing the max-loads-per-week rule and equipment/commodity
  compatibility.
- **`SettlementCalculator`** — computes driver pay from a load's rate and the
  driver's contract percentage.

## Enumerations (realized as lookup tables)

Each of these is a **lookup table** with a sequential integer key, referenced by
FK from the domain objects (so ApiLogicServer generates them as related
resources). Codes shown below match the `code` column in
`database/seed_data.sql`.

- `driver_type`: `company` (30%), `owner_operator` (70%) — carries
  `default_percent` and `owner_bears_costs`.
- `run_type`: `long_haul`, `regional`.
- `power_unit`: `tractor`, `ram_3500`, `ram_4500`.
- `trailer_type`: `step_deck_52`, `rgn_lowboy`, `flatbed_52`, `car_carrier`, `none`.
- `load_status`: `draft`, `dispatched`, `in_transit`, `delivered`, `settled`,
  `cancelled`.
- `app_role`: `dispatcher`, `driver`, `updater`.
- `commodity_category`: `vehicles`, `heavy_equipment`, `farm_equipment`,
  `generators`, `lifts`.
- `channel_type`: `direct`, `group`, `broadcast`.
- `channel_member_role`: `owner`, `member`.
- `document_type`: `document`, `image`, `bill_of_lading`, `invoice`,
  `inspection_photo`, `license`, `other`.

## Policies / business rules (canonical list)

These are the invariants the model must uphold. Structural ones live in
`database/schema.sql` (CHECK constraints, FK relationships, the Monday-only
`week_start`); cross-row and computed ones (weekly load cap, `driver_pay`) are
declared as ApiLogicServer/LogicBank rules in the generated middleware:

1. **Contract percentage** is derived from `DriverType` (30 / 70) — never set
   directly on a settlement.
2. **Weekly load cap**: ≤ 4 loads per driver per dispatch week (target 3).
3. **Dispatch week** is Monday→Monday; a load belongs to exactly one.
4. **Non-negative money & distance.**
5. **Distinct pickup / drop-off.**
6. **Equipment/commodity compatibility.**
7. **Driver pay** = `rate × contract_percentage`.
8. **Cost responsibility** follows `DriverType` (company vs. owner).

## Messaging context

A **message board** for the three roles. Participants are `app_user`s, so
drivers, dispatchers, and updaters all converse through one model.

- **Channel** *(aggregate root)* — a conversation space: a `group`/board, a 1:1
  `direct` thread, or a `broadcast`. Holds members and messages.
- **ChannelMember** — a user's membership in a channel, with `member_role`
  (`owner`/`member`) and `last_read_at` (drives per-user unread counts).
- **Message** — text posted to a channel by an author; `reply_to` supports
  threaded replies; may be empty when it only carries documents.

A message references **Documents** (below) through the `message_document` link,
so attachments are shared content rather than message-private blobs.

## Content (CMS) context

Attachments are **first-class, reusable content**, not message-only blobs, so
they can be reached from anywhere in the app.

- **Document** *(aggregate root)* — a digital scan / image / artifact (BOL,
  invoice, license, inspection photo, …). The binary lives in the DB (`bytea`)
  with metadata (`title`, `document_type`, `filename`, `content_type`,
  `byte_size`, `checksum`, `uploaded_by`).
- **Links** — FK join tables connect a document to other resources, one per
  related type, keeping referential integrity and letting ApiLogicServer
  generate the relationships: `message_document`, `load_document` today; add
  `driver_document`, `equipment_document`, `inspection_document`, … as needed.
  The *same* document can be linked from several places at once.

## Planned: Truck Location & Dispatcher HUD

> Not yet built — captured here so the schema and portals can grow into it.
> Pairs with the GPS/navigation feature (HERE provider, PostGIS storage).

- **Truck Location (telemetry)** — realtime vehicle positions, ingested from
  **multiple, interchangeable sources**: Apple AirTags, Google devices, or a
  driver's phone pushing location. Model this **source-agnostic**: a
  `location_source` lookup (`airtag`, `google_device`, `phone_push`, …) plus a
  `position_report` time series (equipment/driver, lat/lng, heading, speed,
  accuracy, recorded_at, source). Geospatial data stored with **PostGIS**.
- **Dispatcher HUD** — a desktop (C++/Wt) heads-up display showing **truck
  locations** for the whole fleet on a map, plus at-a-glance fleet data (active
  loads, status, this week's board). Reads the latest `position_report` per rig
  and the dispatch/load resources via the shared JSON:API.

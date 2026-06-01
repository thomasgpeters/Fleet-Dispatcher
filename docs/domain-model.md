# Domain Model

This is the heart of the project. Every layer — the `domain/` Python package,
`database/schema.sql`, and the ApiLogicServer/LogicBank rules — uses the nouns
and verbs defined here. Change the language here first, then propagate.

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

## Enumerations

- `DriverType`: `COMPANY` (30%), `OWNER_OPERATOR` (70%).
- `RunType`: `LONG_HAUL`, `REGIONAL`.
- `PowerUnit`: `TRACTOR`, `RAM_3500`, `RAM_4500`.
- `TrailerType`: `STEP_DECK_52`, `RGN_LOWBOY`, `FLATBED_52`, `CAR_CARRIER`, `NONE`.
- `LoadStatus`: `DRAFT`, `DISPATCHED`, `IN_TRANSIT`, `DELIVERED`, `SETTLED`,
  `CANCELLED`.
- `Role`: `DISPATCHER`, `DRIVER`, `UPDATER`.

## Policies / business rules (canonical list)

These are the invariants the LogicBank rules in `middleware/` must enforce, and
the `domain/` package must reproduce:

1. **Contract percentage** is derived from `DriverType` (30 / 70) — never set
   directly on a settlement.
2. **Weekly load cap**: ≤ 4 loads per driver per dispatch week (target 3).
3. **Dispatch week** is Monday→Monday; a load belongs to exactly one.
4. **Non-negative money & distance.**
5. **Distinct pickup / drop-off.**
6. **Equipment/commodity compatibility.**
7. **Driver pay** = `rate × contract_percentage`.
8. **Cost responsibility** follows `DriverType` (company vs. owner).

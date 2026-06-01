# DDD Patterns in Fleet Dispatcher

How the tactical Domain-Driven Design building blocks map onto this codebase,
and how they survive a database-first middleware (ApiLogicServer).

## Layering

```
domain/                      ← Domain layer (pure, no framework imports)
  fleet_dispatcher_domain/
    shared/                  ← building blocks + cross-context value objects
    fleet/                   ← Fleet bounded context
    dispatch/                ← Dispatch bounded context
    settlement/              ← Settlement bounded context

middleware/                  ← Application + Infrastructure
  ApiLogicServer models      ← persistence model (SQLAlchemy, generated)
  LogicBank rules            ← invariants enforced at the transaction boundary

database/                    ← the persisted shape of the model
```

The **domain layer has zero dependencies** on ApiLogicServer, SQLAlchemy, Flask,
or the database. It can be unit-tested in isolation and is the authoritative
description of behavior and invariants. The middleware is the *infrastructure*
that persists aggregates and re-states their invariants as LogicBank rules so
they hold even for clients that bypass the Python domain layer.

## Building blocks

| Pattern              | Where                                                  | Examples                                         |
| -------------------- | ------------------------------------------------------ | ------------------------------------------------ |
| **Value Object**     | `shared/value_objects.py`                              | `Money`, `Percentage`, `Distance`, `Address`, `DateRange`, `EquipmentConfiguration` |
| **Entity**           | base in `shared/model.py`                              | `Driver`, `Equipment`, `Load`, `Shipper`, `Receiver`, `Commodity`, `User` |
| **Aggregate Root**   | `AggregateRoot` base in `shared/model.py`              | `Driver`, `Equipment`, `Load`, `DispatchWeek`, `Settlement` |
| **Domain Service**   | `*/services.py`                                        | `DispatchService`, `SettlementCalculator`        |
| **Repository (port)**| `*/repositories.py` (abstract)                         | `DriverRepository`, `LoadRepository`, …          |
| **Domain Event**     | `shared/events.py`                                     | `LoadDispatched`, `LoadDelivered`, `SettlementCreated` |
| **Enumeration**      | `shared/enums.py`                                      | `DriverType`, `RunType`, `LoadStatus`, `Role`, … |
| **Factory**          | aggregate `@classmethod create(...)`                   | `Load.draft(...)`, `Driver.hire(...)`            |

### Value Objects
Immutable (`@dataclass(frozen=True)`), compared by value, self-validating in
`__post_init__`. A `Money` can never be negative; a `Percentage` is always
0–100; `Percentage.for_driver_type` encodes the 30/70 contract rule so it lives
in exactly one place.

### Entities & Aggregate Roots
Entities have identity (`id`) and a lifecycle. An **aggregate root** is the only
entity referenced from outside its aggregate; cross-aggregate references are by
**id**, never by object pointer (e.g. `Load.driver_id`, not `Load.driver`). This
keeps consistency boundaries crisp and matches how JSON:API exposes
relationships by resource id.

### Domain Services
Logic that spans aggregates or doesn't belong to one entity. `DispatchService`
needs the *set* of a driver's loads in a week (which lives across `Load`
instances and the `DispatchWeek`), so the weekly-cap rule belongs to a service,
not to a single `Load`.

### Repositories (ports & adapters)
The domain declares **abstract** repositories (ports). Concrete adapters live in
the middleware and are backed by SQLAlchemy / ApiLogicServer. The domain never
imports an adapter — dependency points inward.

### Factories
Construction that must guarantee a valid initial state goes through a
`@classmethod` factory (`Load.draft(...)`), so an aggregate cannot be created in
an invalid state.

## Keeping three representations in sync

Because ApiLogicServer is database-first, the *same* invariant is stated in two
enforced places plus one descriptive one:

| Invariant                          | `domain/` (descriptive + testable) | LogicBank (enforced at runtime) | `schema.sql` (structural) |
| ---------------------------------- | ---------------------------------- | ------------------------------- | ------------------------- |
| Driver % from type (30/70)         | `Percentage.for_driver_type`       | formula/derivation rule         | `CHECK` / generated column |
| ≤ 4 loads / driver / week          | `DispatchService.assign`           | `Rule.count` + `Rule.constraint`| FK + app-level            |
| `driver_pay = rate × %`            | `SettlementCalculator`             | `Rule.formula`                  | —                         |
| non-negative money/distance        | `Money` / `Distance` ctor          | `Rule.constraint`               | `CHECK (... >= 0)`        |

**Workflow when the model changes:** edit `docs/domain-model.md` →
update the `domain/` package and its tests → mirror in `database/schema.sql` →
add/adjust the LogicBank rule in the middleware. The ubiquitous language stays
identical across all four.

## What we deliberately did *not* do (yet)

- No CQRS / event sourcing — events exist as a seam (`shared/events.py`) but are
  not yet dispatched to a bus.
- No anti-corruption layers between contexts — they share one database and one
  language for now.
- Settlement does not yet deduct fuel/tolls/maintenance; it records *who is
  responsible* per `DriverType` and computes the percentage split only.

These are intentional baseline choices, recorded here so the next increment has
a clear starting point.

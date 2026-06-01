# Fleet Dispatcher — Domain layer

The pure-Python, framework-free expression of the Fleet Dispatcher domain. No
dependency on ApiLogicServer, SQLAlchemy, Flask, or PostgreSQL.

This package is the **reference for behavior and invariants**. The middleware
(ApiLogicServer + LogicBank) re-states the same invariants at the persistence
boundary so they hold even for clients that never touch this code. See
[`../docs/ddd-patterns.md`](../docs/ddd-patterns.md).

## Layout

```
fleet_dispatcher_domain/
  shared/        building blocks (Entity, AggregateRoot), value objects, enums,
                 policies, events, errors
  fleet/         Driver, Equipment + repository ports
  dispatch/      Load, DispatchWeek, Shipper, Receiver, Commodity,
                 DispatchService + repository ports
  settlement/    Settlement, SettlementCalculator + repository ports
```

## Develop & test

```bash
cd domain
python -m venv .venv && . .venv/bin/activate
pip install -e ".[dev]"
pytest
```

## Example

```python
from datetime import date
from decimal import Decimal

from fleet_dispatcher_domain.fleet import Driver, Equipment
from fleet_dispatcher_domain.dispatch import Load, DispatchWeek, Commodity
from fleet_dispatcher_domain.settlement import SettlementCalculator
from fleet_dispatcher_domain.shared import (
    DriverType, RunType, PowerUnit, TrailerType,
    Money, Distance, Address, EquipmentConfiguration,
)

driver = Driver.hire("Pat Diesel", DriverType.OWNER_OPERATOR)
rig = Equipment.register("T-101", EquipmentConfiguration(
    PowerUnit.TRACTOR, TrailerType.RGN_LOWBOY))

week = DispatchWeek.containing(date.today())
load = Load.draft(
    dispatch_week_id=week.id,
    shipper_id=driver.id, receiver_id=rig.id, commodity_id=rig.id,  # demo ids
    pickup=Address("1 Yard Rd", "Dallas", "TX", "75001"),
    dropoff=Address("9 Dock St", "Denver", "CO", "80202"),
    rate=Money(Decimal("4200")), run_type=RunType.LONG_HAUL,
    deadhead=Distance(Decimal("85")), loaded=Distance(Decimal("780")),
)
load.assign_to(driver.id, rig.id)

pay = SettlementCalculator().settle(load, driver)
print(pay.driver_pay)   # 2940.00 USD  (70% of 4200)
```

"""Behavioral tests for the Fleet Dispatcher domain layer.

These exercise the invariants documented in ``docs/domain-model.md`` and serve
as executable specification for the LogicBank rules in the middleware.
"""

from __future__ import annotations

from datetime import date, datetime
from decimal import Decimal
from uuid import uuid4

import pytest

from fleet_dispatcher_domain.dispatch import (
    Commodity, DispatchService, DispatchWeek, Load,
)
from fleet_dispatcher_domain.dispatch.repositories import LoadRepository
from fleet_dispatcher_domain.fleet import Driver, Equipment
from fleet_dispatcher_domain.settlement import SettlementCalculator
from fleet_dispatcher_domain.shared import (
    MAX_LOADS_PER_WEEK, Address, Distance, DriverType, EquipmentConfiguration,
    InvariantViolation, Money, Percentage, PowerUnit, RunType, TrailerType,
)
from fleet_dispatcher_domain.shared.errors import CompatibilityError, SchedulingError


# --- value objects -----------------------------------------------------------

def test_money_rejects_negative():
    with pytest.raises(InvariantViolation):
        Money(Decimal("-1"))


def test_percentage_for_driver_type():
    assert Percentage.for_driver_type(DriverType.COMPANY).value == Decimal(30)
    assert Percentage.for_driver_type(DriverType.OWNER_OPERATOR).value == Decimal(70)


def test_money_times_percentage_rounds_to_cents():
    assert (Money(Decimal("4200")) * Percentage(Decimal(70))).amount == Decimal("2940.00")


def test_dispatch_week_is_monday_to_monday():
    # 2026-06-01 is a Monday.
    rng = DispatchWeek.containing(date(2026, 6, 3)).range
    assert rng.start == datetime(2026, 6, 1)
    assert rng.end == datetime(2026, 6, 8)
    assert rng.contains(datetime(2026, 6, 1))
    assert not rng.contains(datetime(2026, 6, 8))


def test_equipment_configuration_rejects_invalid_combo():
    with pytest.raises(InvariantViolation):
        EquipmentConfiguration(PowerUnit.RAM_3500, TrailerType.RGN_LOWBOY)


# --- load lifecycle ----------------------------------------------------------

def _addr(city):
    return Address("1 Main St", city, "TX", "75001")


def _draft_load(week):
    return Load.draft(
        dispatch_week_id=week.id,
        shipper_id=uuid4(), receiver_id=uuid4(), commodity_id=uuid4(),
        pickup=_addr("Dallas"), dropoff=_addr("Denver"),
        rate=Money(Decimal("4200")), run_type=RunType.LONG_HAUL,
        deadhead=Distance(Decimal("85")), loaded=Distance(Decimal("780")),
    )


def test_load_distinct_pickup_dropoff():
    week = DispatchWeek.containing(date(2026, 6, 1))
    with pytest.raises(InvariantViolation):
        Load.draft(
            dispatch_week_id=week.id,
            shipper_id=uuid4(), receiver_id=uuid4(), commodity_id=uuid4(),
            pickup=_addr("Dallas"), dropoff=_addr("Dallas"),
            rate=Money(Decimal("100")), run_type=RunType.REGIONAL,
        )


def test_load_lifecycle_transitions():
    week = DispatchWeek.containing(date(2026, 6, 1))
    load = _draft_load(week)
    load.assign_to(uuid4(), uuid4())
    load.start_transit()
    load.mark_delivered()
    load.mark_settled()
    # cannot settle twice
    with pytest.raises(InvariantViolation):
        load.mark_settled()


# --- dispatch service (cross-aggregate rules) --------------------------------

class FakeLoadRepo(LoadRepository):
    def __init__(self):
        self._by_id = {}

    def get(self, load_id):
        return self._by_id.get(load_id)

    def add(self, load):
        self._by_id[load.id] = load

    def loads_for_driver_in_week(self, driver_id, dispatch_week_id):
        return [l for l in self._by_id.values()
                if l.driver_id == driver_id and l.dispatch_week_id == dispatch_week_id]


def _rig():
    return Equipment.register("T-101", EquipmentConfiguration(
        PowerUnit.TRACTOR, TrailerType.RGN_LOWBOY))


def test_dispatch_rejects_incompatible_equipment():
    repo = FakeLoadRepo()
    svc = DispatchService(repo)
    week = DispatchWeek.containing(date(2026, 6, 1))
    load = _draft_load(week)
    repo.add(load)
    driver = Driver.hire("Pat", DriverType.COMPANY)
    flatbed = Equipment.register("F-1", EquipmentConfiguration(
        PowerUnit.TRACTOR, TrailerType.FLATBED_52))
    heavy = Commodity(description="excavator", category="heavy_equipment")
    with pytest.raises(CompatibilityError):
        svc.assign(load, driver, flatbed, heavy)


def test_dispatch_enforces_weekly_cap():
    repo = FakeLoadRepo()
    svc = DispatchService(repo)
    week = DispatchWeek.containing(date(2026, 6, 1))
    driver = Driver.hire("Pat", DriverType.OWNER_OPERATOR)
    rig = _rig()
    heavy = Commodity(description="dozer", category="heavy_equipment")

    for _ in range(MAX_LOADS_PER_WEEK):
        load = _draft_load(week)
        repo.add(load)
        svc.assign(load, driver, rig, heavy)

    overflow = _draft_load(week)
    repo.add(overflow)
    with pytest.raises(SchedulingError):
        svc.assign(overflow, driver, rig, heavy)


# --- settlement --------------------------------------------------------------

def test_settlement_pays_contract_percentage():
    week = DispatchWeek.containing(date(2026, 6, 1))
    load = _draft_load(week)
    owner = Driver.hire("Pat", DriverType.OWNER_OPERATOR)
    s = SettlementCalculator().settle(load, owner)
    assert s.driver_pay.amount == Decimal("2940.00")   # 70% of 4200
    assert s.company_pay.amount == Decimal("1260.00")
    assert s.costs_borne_by_owner is True

    company_driver = Driver.hire("Sam", DriverType.COMPANY)
    s2 = SettlementCalculator().settle(load, company_driver)
    assert s2.driver_pay.amount == Decimal("1260.00")   # 30% of 4200
    assert s2.costs_borne_by_owner is False

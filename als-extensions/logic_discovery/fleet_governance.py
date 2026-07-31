"""Fleet Dispatcher — fleet business governance (auto-discovered LogicBank logic).

Business INVARIANTS for the fleet aggregate live here, not in the C++/Python
clients, so mobile, desktop, the assistant, and any raw API caller are all
governed identically at the one place every write funnels through (the ALS
commit). This is the sibling of comms_governance.py; same install path
(als-extensions/install.sh copies logic_discovery/*.py; re-run `make
als-extensions` after every ALS rebuild).

WHY LOGICBANK (design note, see docs/LOGICBANK_RULES.md):
  A rule belongs here when it must hold no matter which client writes. LogicBank
  forward-chains: change any attribute and every dependent derivation recomputes
  transitively. TIME is modeled as data (sys_clock) so even clock-driven
  transitions are LogicBank rules — a thin "tick" agent advances sys_clock.today
  and that single row change cascades through the temporal formulas below. The
  agent is a pulse; the logic is declarative.

NOTE (LogicBank/ALS version-sensitive): model class names come from ALS
generation (vehicle -> Vehicle, rig_bundle -> RigBundle, vehicle_lease ->
VehicleLease, sys_clock -> SysClock, tractor_spec -> TractorSpec). Rule.formula /
Rule.constraint signatures and the parent-cascade wiring (sys_clock -> lease ->
vehicle) can vary by version — validate on the Linux box the way we flag Wt
spots. Rules that need Smitty-mirror tables (service records / maintenance
schedule) are stubbed until Phase 3 lands those tables.
"""

from __future__ import annotations

import logging

from logic_bank.exec_row_logic.logic_row import LogicRow
from logic_bank.logic_bank import Rule  # Rule lives here (matches ALS declare_logic.py)

from database import models

log = logging.getLogger(__name__)

# Lookup ids — must match database/seed_data.sql.
VEHICLE_STATUS_IN_SERVICE = 1        # 2 out_of_service · 3 in_maintenance · 4 retired
ASSET_TYPE_TRACTOR = 1
ASSET_TYPE_TRAILER = 2


# --- helpers ----------------------------------------------------------------

def _vehicle(session, vehicle_id):
    if not vehicle_id:
        return None
    return (session.query(models.Vehicle)
            .filter_by(id=vehicle_id).one_or_none())


def _dispatchable(vehicle) -> bool:
    """A vehicle can be dispatched only when it's in service (not in the shop,
    out of service, or retired)."""
    return vehicle is None or vehicle.vehicle_status_id == VEHICLE_STATUS_IN_SERVICE


# --- constraints (reject invalid writes) ------------------------------------

def _load_rig_available(row, old_row, logic_row: LogicRow) -> bool:
    """Don't assign a load to a vehicle that's in the shop / out of service.
    The closed-loop payoff of the Smitty integration: Smitty flips a rig to
    in_maintenance, and dispatch is blocked here for every client at once."""
    session = logic_row.session
    return (_dispatchable(_vehicle(session, row.power_vehicle_id))
            and _dispatchable(_vehicle(session, row.trailer_vehicle_id)))


def _trip_rig_available(row, old_row, logic_row: LogicRow) -> bool:
    session = logic_row.session
    return (_dispatchable(_vehicle(session, row.power_vehicle_id))
            and _dispatchable(_vehicle(session, row.trailer_vehicle_id)))


def _bundle_roles_valid(row, old_row, logic_row: LogicRow) -> bool:
    """A rig bundle's power vehicle must be a tractor and its trailer a trailer."""
    session = logic_row.session
    power = _vehicle(session, row.power_vehicle_id)
    trailer = _vehicle(session, row.trailer_vehicle_id)
    if power is not None and power.asset_type_id != ASSET_TYPE_TRACTOR:
        return False
    if trailer is not None and trailer.asset_type_id != ASSET_TYPE_TRAILER:
        return False
    return True


def _tractor_spec_matches(row, old_row, logic_row: LogicRow) -> bool:
    v = _vehicle(logic_row.session, row.vehicle_id)
    return v is None or v.asset_type_id == ASSET_TYPE_TRACTOR


def _trailer_spec_matches(row, old_row, logic_row: LogicRow) -> bool:
    v = _vehicle(logic_row.session, row.vehicle_id)
    return v is None or v.asset_type_id == ASSET_TYPE_TRAILER


def _odometer_monotonic(row, old_row, logic_row: LogicRow) -> bool:
    """Odometer never decreases (guards bad telematics / typos)."""
    if logic_row.is_updated() and old_row is not None:
        if row.odometer_miles is not None and old_row.odometer_miles is not None:
            return row.odometer_miles >= old_row.odometer_miles
    return True


# --- temporal formula (tick-driven via sys_clock) ---------------------------

def _lease_is_active(row, old_row, logic_row: LogicRow):
    """is_active = start_date <= sys_clock.today <= end_date.

    Depends on the parent sys_clock: when the tick agent advances
    sys_clock.today, LogicBank forward-chains this formula for every lease
    (child), flipping expired leases to inactive with no scheduled Python job.
    (Reads the clock via the SysClock singleton.)"""
    clock = (logic_row.session.query(models.SysClock)
             .filter_by(id=row.sys_clock_id or 1).one_or_none())
    if clock is None or clock.today is None:
        return None
    if row.start_date and clock.today < row.start_date:
        return False
    if row.end_date and clock.today > row.end_date:
        return False
    return True


# --- registration -----------------------------------------------------------

def declare_logic() -> None:
    """Registered by ALS logic discovery on server start."""
    Rule.constraint(validate=models.Load, calling=_load_rig_available,
                    error_msg="That vehicle is out of service / in the shop.")
    Rule.constraint(validate=models.Trip, calling=_trip_rig_available,
                    error_msg="That vehicle is out of service / in the shop.")
    Rule.constraint(validate=models.RigBundle, calling=_bundle_roles_valid,
                    error_msg="A bundle needs a tractor as power and a trailer as trailer.")
    Rule.constraint(validate=models.TractorSpec, calling=_tractor_spec_matches,
                    error_msg="tractor_spec is only valid for a tractor vehicle.")
    Rule.constraint(validate=models.TrailerSpec, calling=_trailer_spec_matches,
                    error_msg="trailer_spec is only valid for a trailer vehicle.")
    Rule.constraint(validate=models.Vehicle, calling=_odometer_monotonic,
                    error_msg="Odometer can't decrease.")

    # Temporal: tick-driven lease activity (sys_clock -> lease cascade).
    Rule.formula(derive=models.VehicleLease.is_active, calling=_lease_is_active)

    log.info("Fleet Dispatcher fleet governance registered "
             "(dispatch-lock + bundle/spec/odometer integrity + tick-driven lease activity)")

    # ---- PLANNED (need Phase-3 Smitty-mirror tables) -------------------------
    # Rule.copy(...)  maint_responsibility snapshot onto an ingested service
    #                 record (allocate at-cost to the party responsible at
    #                 service time).
    # Rule.formula(models.MaintenanceSchedule.status, ...)  upcoming/due/overdue
    #                 from sys_clock.today (date) + vehicle.odometer (mileage).
    # Rule.formula(models.Vehicle.maint_responsibility_id, ...)  re-derive from
    #                 ownership + the active lease (lease wins), tick-driven.
    # Rule.row_event(models.Vehicle, ...)  set vehicle_status from a Smitty
    #                 in-shop / out-of-service ingest.

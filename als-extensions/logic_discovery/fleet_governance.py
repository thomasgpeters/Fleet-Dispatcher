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
DUE_SOON_DAYS = 7                    # maintenance "due" window (date side)
DUE_SOON_MILES = 1000                # maintenance "due" window (mileage side)


# --- helpers ----------------------------------------------------------------

def _vehicle(session, vehicle_id):
    if not vehicle_id:
        return None
    return (session.query(models.Vehicle)
            .filter_by(id=vehicle_id).one_or_none())


def _active_oos(session, vehicle_id) -> bool:
    """True if the vehicle has an open out-of-service window (to_ts NULL) — the
    Smitty in-shop signal, mirrored Fleet-side."""
    if not vehicle_id:
        return False
    return (session.query(models.VehicleOutOfService)
            .filter_by(vehicle_id=vehicle_id, to_ts=None).first() is not None)


def _dispatchable(session, vehicle) -> bool:
    """A vehicle can be dispatched only when it's in service AND not in an open
    out-of-service window (e.g. sitting in Smitty's shop)."""
    if vehicle is None:
        return True
    return (vehicle.vehicle_status_id == VEHICLE_STATUS_IN_SERVICE
            and not _active_oos(session, vehicle.id))


# --- constraints (reject invalid writes) ------------------------------------

def _load_rig_available(row, old_row, logic_row: LogicRow) -> bool:
    """Don't assign a load to a vehicle that's in the shop / out of service.
    The closed-loop payoff of the Smitty integration: Smitty opens an OOS window
    (or flips status), and dispatch is blocked here for every client at once."""
    session = logic_row.session
    return (_dispatchable(session, _vehicle(session, row.power_vehicle_id))
            and _dispatchable(session, _vehicle(session, row.trailer_vehicle_id)))


def _trip_rig_available(row, old_row, logic_row: LogicRow) -> bool:
    session = logic_row.session
    return (_dispatchable(session, _vehicle(session, row.power_vehicle_id))
            and _dispatchable(session, _vehicle(session, row.trailer_vehicle_id)))


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


def _schedule_status(row, old_row, logic_row: LogicRow):
    """Maintenance status: upcoming | due | overdue, from sys_clock.today (date)
    and the vehicle odometer (mileage), whichever is closer. Tick-driven on the
    date side; recomputed on odometer pushes on the mileage side."""
    session = logic_row.session
    clock = (session.query(models.SysClock)
             .filter_by(id=row.sys_clock_id or 1).one_or_none())
    today = clock.today if clock else None
    veh = _vehicle(session, row.vehicle_id)
    odo = veh.odometer_miles if veh else None

    overdue = due = False
    if row.next_due_on and today:
        if today > row.next_due_on:
            overdue = True
        elif (row.next_due_on - today).days <= DUE_SOON_DAYS:
            due = True
    if row.next_due_odometer and odo is not None:
        if odo >= row.next_due_odometer:
            overdue = True
        elif row.next_due_odometer - odo <= DUE_SOON_MILES:
            due = True
    return "overdue" if overdue else ("due" if due else "upcoming")


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

    # Smitty service mirror (Fleet-side consumer):
    # Cost allocation — snapshot who was responsible AT SERVICE TIME onto the
    # ingested service record (Rule.copy from the vehicle parent), so the at-cost
    # amount is allocated correctly even if responsibility later changes.
    Rule.copy(derive=models.ServiceRecord.maint_responsibility_id,
              from_parent=models.Vehicle.maint_responsibility_id)
    # Maintenance status — upcoming/due/overdue from the clock (date) + odometer.
    Rule.formula(derive=models.MaintenanceSchedule.status, calling=_schedule_status)

    log.info("Fleet Dispatcher fleet governance registered "
             "(dispatch-lock incl. OOS + bundle/spec/odometer integrity + "
             "tick-driven lease activity + service-mirror cost-allocation/maint-status)")

    # ---- PLANNED -------------------------------------------------------------
    # Rule.formula(models.Vehicle.maint_responsibility_id, ...)  re-derive from
    #                 ownership + the active lease (lease wins), tick-driven —
    #                 currently stored authoritatively; make it a formula once the
    #                 owner/lease chain is finalized.

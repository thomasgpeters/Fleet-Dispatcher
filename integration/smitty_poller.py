#!/usr/bin/env python3
"""Fleet ← Smitty service poller — the thin fetch driver (SCAFFOLD).

Per the LogicBank doctrine (docs/LOGICBANK_RULES.md) this agent carries **no
business logic**: it fetches Smitty's service data and writes it through **Fleet's
ALS JSON:API** (not direct SQL), so Fleet's LogicBank rules fire on ingest —
cost-allocation snapshot, maintenance status, and the OOS dispatch-lock all happen
declaratively in `fleet_governance.py`. The poller just moves rows.

Boundary rules it enforces (docs/INTEGRATION_SMITTY.md B.1–B.3):
  * Only Fleet-owned VINs are ingested — anything Fleet can't resolve to a
    `vehicle` is dropped (third-party data never lands).
  * Requests are scoped to Fleet's Smitty **customer_id** + VIN set.
  * Costs are **at-cost only** — the retail/markup fields are never read/stored.

STATUS: scaffold. It runs once Smitty ships its Phase-1 endpoints + the
`X-Service-Token` middleware (sequencing: RMA first — see INTEGRATION_SMITTY.md).
The exact Smitty JSON shapes (`/api/Job`, `/api/MaintenanceSchedule`,
`/api/VehicleOutOfService`) are marked TODO where field names must be confirmed.

Config (env):
  FLEET_API_BASE      default http://localhost:5659/api
  FLEET_TOKEN         Fleet ALS JWT (a service account)
  SMITTY_API_BASE     e.g. http://localhost:5655/api
  SMITTY_SERVICE_TOKEN shared token sent as X-Service-Token (see B.4)
  SMITTY_CUSTOMER_ID  Fleet's "house customer" id on the Smitty side (scoping)
"""

from __future__ import annotations

import os
import uuid
from typing import Any, Optional

import requests  # thin HTTP; add to integration/requirements.txt

FLEET_API_BASE = os.environ.get("FLEET_API_BASE", "http://localhost:5659/api")
FLEET_TOKEN = os.environ.get("FLEET_TOKEN", "")
SMITTY_API_BASE = os.environ.get("SMITTY_API_BASE", "http://localhost:5655/api")
SMITTY_SERVICE_TOKEN = os.environ.get("SMITTY_SERVICE_TOKEN", "")
SMITTY_CUSTOMER_ID = os.environ.get("SMITTY_CUSTOMER_ID", "")

_fleet_hdr = {"Authorization": f"Bearer {FLEET_TOKEN}",
              "Content-Type": "application/vnd.api+json"}
_smitty_hdr = {"X-Service-Token": SMITTY_SERVICE_TOKEN}


# --- Smitty side (read; scoped to Fleet's customer) -------------------------

def _smitty_get(resource: str) -> list[dict[str, Any]]:
    """GET a Smitty JSON:API collection, scoped to Fleet's customer_id so only
    Fleet-owned rows come back (never third-party). Returns the `data` list."""
    params = {}
    if SMITTY_CUSTOMER_ID:
        params["filter[customer_id]"] = SMITTY_CUSTOMER_ID
    r = requests.get(f"{SMITTY_API_BASE}/{resource}", headers=_smitty_hdr,
                     params=params, timeout=30)
    r.raise_for_status()
    return r.json().get("data", [])


# --- Fleet side (resolve VIN, upsert through ALS so LogicBank fires) ---------

def _vehicle_id_for_vin(vin: str) -> Optional[str]:
    """Resolve a VIN to a Fleet vehicle id. None = Fleet doesn't own it → drop
    the row (third-party). (Cache this in a real run.)"""
    if not vin:
        return None
    r = requests.get(f"{FLEET_API_BASE}/Vehicle",
                     headers={"Authorization": f"Bearer {FLEET_TOKEN}"},
                     params={"filter[vin]": vin}, timeout=15)
    r.raise_for_status()
    data = r.json().get("data", [])
    return data[0]["id"] if data else None


def _upsert(resource: str, correlation_attr: str, correlation_val: str,
            attributes: dict[str, Any]) -> None:
    """Idempotent upsert through Fleet's ALS: find an existing row by its Smitty
    correlation id, PATCH it, else POST a new one (with a client-generated id —
    ALS needs it for UUID PKs; see the createResource fix)."""
    hdr_get = {"Authorization": f"Bearer {FLEET_TOKEN}"}
    existing = requests.get(f"{FLEET_API_BASE}/{resource}", headers=hdr_get,
                            params={f"filter[{correlation_attr}]": correlation_val},
                            timeout=15).json().get("data", [])
    if existing:
        rid = existing[0]["id"]
        body = {"data": {"type": resource, "id": rid, "attributes": attributes}}
        requests.patch(f"{FLEET_API_BASE}/{resource}/{rid}", headers=_fleet_hdr,
                       json=body, timeout=15).raise_for_status()
    else:
        body = {"data": {"type": resource, "id": str(uuid.uuid4()),
                         "attributes": attributes}}
        requests.post(f"{FLEET_API_BASE}/{resource}", headers=_fleet_hdr,
                      json=body, timeout=15).raise_for_status()


# --- sync passes ------------------------------------------------------------

def _job_at_cost(job_id: str) -> Optional[float]:
    """At-cost total for a Smitty Job, summed from the LINE-level at-cost columns
    (job_parts.unit_cost x quantity + job_labor_items.hourly_cost x hours).
    Retail/markup (job_total, unit_price, rate) is NEVER read; quantities/hours are
    service facts and do cross. Per Fleet response FR.2: a **NULL cost line = cost
    unknown**, so return None (at-cost incomplete) rather than silently counting it
    as $0 — Fleet's valuation only trusts complete at-cost. TODO: confirm the exact
    JobPart/JobLaborItem field + filter names against Smitty's v2 shapes."""
    total = 0.0
    for p in _smitty_get(f"JobPart?filter[job_id]={job_id}"):
        a = p.get("attributes", {})
        if a.get("unit_cost") is None:
            return None
        total += a["unit_cost"] * (a.get("quantity") or 0)
    for l in _smitty_get(f"JobLaborItem?filter[job_id]={job_id}"):
        a = l.get("attributes", {})
        if a.get("hourly_cost") is None:
            return None
        total += a["hourly_cost"] * (a.get("hours") or 0)
    return round(total, 2)


def sync_service_records() -> int:
    """Smitty Job -> Fleet service_record. AT-COST ONLY: the at-cost figure is
    summed from the line-level unit_cost/hourly_cost (Smitty response v2); the
    retail/markup totals are never read. LogicBank Rule.copy snapshots the
    responsible party from the vehicle on insert."""
    n = 0
    for job in _smitty_get("Job"):
        a = job.get("attributes", {})
        vin = a.get("vin")                      # TODO: confirm Smitty exposes vin on Job
        vehicle_id = _vehicle_id_for_vin(vin)
        if not vehicle_id:
            continue  # not a Fleet vehicle — drop
        _upsert("ServiceRecord", "smitty_job_id", str(job["id"]), {
            "smitty_job_id": str(job["id"]),
            "vin": vin,
            "vehicle_id": vehicle_id,
            "service_type": a.get("service_type"),
            "complaint": a.get("complaint"),
            "opened_at": a.get("opened_at"),
            "closed_at": a.get("closed_at"),
            "odometer_at_service": a.get("odometer_at_service"),
            # AT-COST ONLY — summed from lines; do NOT read job_total/parts_total/etc.
            "at_cost_amount": _job_at_cost(str(job["id"])),
            "vendor": "Smitty Services",
            "status": a.get("status"),
            # maint_responsibility_id is set by LogicBank (Rule.copy) — don't send.
        })
        n += 1
    return n


def sync_maintenance_schedules() -> int:
    n = 0
    for s in _smitty_get("MaintenanceSchedule"):
        a = s.get("attributes", {})
        vehicle_id = _vehicle_id_for_vin(a.get("vin"))
        if not vehicle_id:
            continue
        _upsert("MaintenanceSchedule", "smitty_schedule_id", str(s["id"]), {
            "smitty_schedule_id": str(s["id"]),
            "vin": a.get("vin"),
            "vehicle_id": vehicle_id,
            "service_type": a.get("service_type"),
            "interval_miles": a.get("interval_miles"),
            "interval_days": a.get("interval_days"),
            "next_due_on": a.get("next_due_on"),
            "next_due_odometer": a.get("next_due_odometer"),
            # status is a LogicBank formula — don't send.
        })
        n += 1
    return n


def sync_out_of_service() -> int:
    n = 0
    for o in _smitty_get("VehicleOutOfService"):
        a = o.get("attributes", {})
        vehicle_id = _vehicle_id_for_vin(a.get("vin"))
        if not vehicle_id:
            continue
        _upsert("VehicleOutOfService", "smitty_oos_id", str(o["id"]), {
            "smitty_oos_id": str(o["id"]),
            "vin": a.get("vin"),
            "vehicle_id": vehicle_id,
            "from_ts": a.get("from_ts"),
            "to_ts": a.get("to_ts"),
            "reason": a.get("reason"),
        })
        n += 1
    return n


def main() -> None:
    jobs = sync_service_records()
    scheds = sync_maintenance_schedules()
    oos = sync_out_of_service()
    print(f"Smitty sync: {jobs} service records, {scheds} schedules, {oos} OOS.")


if __name__ == "__main__":
    main()

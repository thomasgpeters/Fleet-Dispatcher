-- Migration 2026-08-01 — Smitty service-mirror tables (Fleet-side consumer).
--
-- These three tables are already defined in database/schema.sql (the source of
-- truth) and verified there. This migration applies them INCREMENTALLY to an
-- existing live database that predates them, so the Smitty integration can be
-- turned on without a full schema reset (which would destroy live data).
--
-- They are Fleet's READ-ONLY projections of Smitty service data, correlated by
-- VIN, kept fresh by integration/smitty_poller.py. At-cost only; no retail/markup
-- crosses the boundary. See docs/INTEGRATION_SMITTY.md.
--
-- Prerequisites (all present since Feature 7): vehicle, maint_responsibility,
-- sys_clock. Idempotent (IF NOT EXISTS) and transactional — safe to re-run.
--
-- After applying: regenerate ALS + `make als-extensions` + restart, so the
-- currently-dormant (hasattr-guarded) LogicBank rules in fleet_governance.py
-- activate (service-record cost allocation, maintenance status, OOS dispatch lock).
--
-- Apply:
--   psql "$DATABASE_URL" -f database/migrations/2026-08-01_smitty_mirror_tables.sql

BEGIN;
SET search_path = fleet, public;

CREATE TABLE IF NOT EXISTS service_record (
    id                      UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    smitty_job_id           TEXT,                              -- Smitty Job id (correlation)
    vin                     TEXT,                              -- correlation key
    vehicle_id              UUID REFERENCES vehicle(id),       -- resolved from VIN (NULL until matched)
    service_type            TEXT,
    complaint               TEXT,
    opened_at               TIMESTAMPTZ,
    closed_at               TIMESTAMPTZ,
    odometer_at_service     INTEGER CHECK (odometer_at_service IS NULL OR odometer_at_service >= 0),
    at_cost_amount          NUMERIC(12,2) CHECK (at_cost_amount IS NULL OR at_cost_amount >= 0),  -- at-cost only (NO markup/retail)
    vendor                  TEXT,
    status                  TEXT,                              -- new | in_progress | complete | invoiced
    maint_responsibility_id INTEGER REFERENCES maint_responsibility(id),
    created_at              TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_service_record_vin ON service_record (vin);
CREATE INDEX IF NOT EXISTS idx_service_record_vehicle ON service_record (vehicle_id, opened_at DESC);

CREATE TABLE IF NOT EXISTS maintenance_schedule (
    id                UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    smitty_schedule_id TEXT,                                   -- Smitty schedule id (correlation)
    vin               TEXT,
    vehicle_id        UUID REFERENCES vehicle(id),
    service_type      TEXT,
    interval_miles    INTEGER,
    interval_days     INTEGER,
    next_due_on       DATE,
    next_due_odometer INTEGER,
    status            TEXT,                                    -- LogicBank: upcoming | due | overdue
    sys_clock_id      INTEGER NOT NULL REFERENCES sys_clock(id) DEFAULT 1,  -- clock hook
    created_at        TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_maintenance_schedule_vehicle ON maintenance_schedule (vehicle_id, status);

CREATE TABLE IF NOT EXISTS vehicle_out_of_service (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    smitty_oos_id TEXT,                                        -- Smitty OOS id (correlation)
    vin           TEXT,
    vehicle_id    UUID REFERENCES vehicle(id),
    from_ts       TIMESTAMPTZ,
    to_ts         TIMESTAMPTZ,                                 -- NULL = still out of service
    reason        TEXT,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT vehicle_oos_period CHECK (to_ts IS NULL OR from_ts IS NULL OR to_ts >= from_ts)
);
CREATE INDEX IF NOT EXISTS idx_vehicle_oos_vehicle ON vehicle_out_of_service (vehicle_id, to_ts);

COMMIT;

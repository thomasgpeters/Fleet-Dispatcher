-- Fleet Dispatcher — PostgreSQL schema
--
-- The physical realization of the domain model in docs/domain-model.md.
-- ApiLogicServer reads this schema to generate SQLAlchemy models and the
-- JSON:API. CHECK constraints encode the structural invariants; cross-row /
-- computed invariants (weekly load cap, driver_pay = rate × %) are enforced by
-- LogicBank rules in the middleware (see middleware/README.md).
--
-- Apply with:  psql "$DATABASE_URL" -f database/schema.sql

BEGIN;

-- ---------------------------------------------------------------------------
-- Enumerated types (mirror fleet_dispatcher_domain.shared.enums)
-- ---------------------------------------------------------------------------
CREATE TYPE driver_type   AS ENUM ('company', 'owner_operator');
CREATE TYPE run_type       AS ENUM ('long_haul', 'regional');
CREATE TYPE power_unit     AS ENUM ('tractor', 'ram_3500', 'ram_4500');
CREATE TYPE trailer_type   AS ENUM ('step_deck_52', 'rgn_lowboy', 'flatbed_52', 'car_carrier', 'none');
CREATE TYPE load_status    AS ENUM ('draft', 'dispatched', 'in_transit', 'delivered', 'settled', 'cancelled');
CREATE TYPE app_role       AS ENUM ('dispatcher', 'driver', 'updater');

-- ---------------------------------------------------------------------------
-- Identity & Access
-- ---------------------------------------------------------------------------
CREATE TABLE app_user (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username      TEXT NOT NULL UNIQUE,
    full_name     TEXT NOT NULL,
    email         TEXT,
    role          app_role NOT NULL,
    active        BOOLEAN NOT NULL DEFAULT TRUE,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- ---------------------------------------------------------------------------
-- Fleet context
-- ---------------------------------------------------------------------------
CREATE TABLE driver (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name          TEXT NOT NULL,
    driver_type   driver_type NOT NULL,
    phone         TEXT,
    email         TEXT,
    home_base     TEXT,
    active        BOOLEAN NOT NULL DEFAULT TRUE,
    -- Contract percentage is derived from driver_type (Company 30 / Owner-Op 70).
    -- Stored as a generated column so the rule has one source of truth in SQL too.
    contract_percent SMALLINT GENERATED ALWAYS AS (
        CASE driver_type WHEN 'company' THEN 30 ELSE 70 END
    ) STORED,
    -- A user account may be linked to a driver (Driver role).
    user_id       UUID REFERENCES app_user(id),
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE equipment (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    unit_number     TEXT NOT NULL UNIQUE,
    power_unit      power_unit NOT NULL,
    trailer         trailer_type NOT NULL,
    has_ramps       BOOLEAN NOT NULL DEFAULT FALSE,
    deck_length_ft  SMALLINT CHECK (deck_length_ft IS NULL OR deck_length_ft > 0),
    has_duals       BOOLEAN NOT NULL DEFAULT FALSE,
    in_service      BOOLEAN NOT NULL DEFAULT TRUE,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Which equipment a driver is qualified/assigned to operate (many-to-many).
CREATE TABLE driver_equipment (
    driver_id     UUID NOT NULL REFERENCES driver(id) ON DELETE CASCADE,
    equipment_id  UUID NOT NULL REFERENCES equipment(id) ON DELETE CASCADE,
    PRIMARY KEY (driver_id, equipment_id)
);

-- ---------------------------------------------------------------------------
-- Dispatch context
-- ---------------------------------------------------------------------------
CREATE TABLE location (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    line1         TEXT NOT NULL,
    line2         TEXT,
    city          TEXT NOT NULL,
    state         TEXT NOT NULL,
    postal_code   TEXT NOT NULL,
    country       TEXT NOT NULL DEFAULT 'USA'
);

CREATE TABLE shipper (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name          TEXT NOT NULL,
    location_id   UUID REFERENCES location(id),
    contact       TEXT
);

CREATE TABLE receiver (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name          TEXT NOT NULL,
    location_id   UUID REFERENCES location(id),
    contact       TEXT
);

CREATE TABLE commodity (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    description   TEXT NOT NULL,
    category      TEXT NOT NULL,   -- vehicles | heavy_equipment | farm_equipment | generators | lifts | ...
    weight_lbs    INTEGER CHECK (weight_lbs IS NULL OR weight_lbs >= 0)
);

-- A dispatch week, identified by its opening Monday (Monday -> Monday).
CREATE TABLE dispatch_week (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    week_start    DATE NOT NULL UNIQUE,   -- must be a Monday (enforced below)
    CONSTRAINT week_start_is_monday CHECK (EXTRACT(DOW FROM week_start) = 1)
);

CREATE TABLE load (
    id               UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    dispatch_week_id UUID NOT NULL REFERENCES dispatch_week(id),
    driver_id        UUID REFERENCES driver(id),
    equipment_id     UUID REFERENCES equipment(id),
    shipper_id       UUID NOT NULL REFERENCES shipper(id),
    receiver_id      UUID NOT NULL REFERENCES receiver(id),
    commodity_id     UUID NOT NULL REFERENCES commodity(id),
    pickup_id        UUID NOT NULL REFERENCES location(id),
    dropoff_id       UUID NOT NULL REFERENCES location(id),
    run_type         run_type NOT NULL,
    deadhead_miles   NUMERIC(8,1) NOT NULL DEFAULT 0 CHECK (deadhead_miles >= 0),
    loaded_miles     NUMERIC(8,1) NOT NULL DEFAULT 0 CHECK (loaded_miles >= 0),
    rate             NUMERIC(12,2) NOT NULL DEFAULT 0 CHECK (rate >= 0),  -- post-broker
    currency         TEXT NOT NULL DEFAULT 'USD',
    status           load_status NOT NULL DEFAULT 'draft',
    created_at       TIMESTAMPTZ NOT NULL DEFAULT now(),
    CONSTRAINT distinct_pickup_dropoff CHECK (pickup_id <> dropoff_id)
);

CREATE INDEX idx_load_driver_week ON load (driver_id, dispatch_week_id)
    WHERE status <> 'cancelled';

-- ---------------------------------------------------------------------------
-- Settlement context
-- ---------------------------------------------------------------------------
CREATE TABLE settlement (
    id                   UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    driver_id            UUID NOT NULL REFERENCES driver(id),
    load_id              UUID NOT NULL REFERENCES load(id) UNIQUE,
    gross_rate           NUMERIC(12,2) NOT NULL CHECK (gross_rate >= 0),
    contract_percent     SMALLINT NOT NULL CHECK (contract_percent BETWEEN 0 AND 100),
    driver_pay           NUMERIC(12,2) NOT NULL CHECK (driver_pay >= 0),
    costs_borne_by_owner BOOLEAN NOT NULL,
    currency             TEXT NOT NULL DEFAULT 'USD',
    created_at           TIMESTAMPTZ NOT NULL DEFAULT now()
);

COMMIT;

-- Notes for the middleware (LogicBank), enforced at the transaction boundary:
--   * load: at most 4 non-cancelled loads per (driver_id, dispatch_week_id).
--   * settlement.driver_pay = round(gross_rate * contract_percent / 100, 2).
--   * settlement.contract_percent must match driver.contract_percent.
--   * equipment/commodity compatibility on load assignment.

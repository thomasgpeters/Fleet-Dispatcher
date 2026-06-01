-- Fleet Dispatcher — sample seed data
--
-- Small, illustrative data set for local development and demos.
-- Apply AFTER schema.sql:  psql "$DATABASE_URL" -f database/seed.sql

BEGIN;

-- Users (one per role) ------------------------------------------------------
INSERT INTO app_user (id, username, full_name, email, role) VALUES
  ('11111111-1111-1111-1111-111111111111', 'dispatch1', 'Dana Dispatcher', 'dana@example.com', 'dispatcher'),
  ('22222222-2222-2222-2222-222222222222', 'driver1',   'Pat Diesel',      'pat@example.com',  'driver'),
  ('33333333-3333-3333-3333-333333333333', 'updater1',  'Uma Updater',     'uma@example.com',  'updater');

-- Drivers (company + owner-operator) ----------------------------------------
INSERT INTO driver (id, name, driver_type, phone, home_base, user_id) VALUES
  ('aaaaaaaa-0000-0000-0000-000000000001', 'Pat Diesel',  'owner_operator', '555-0101', 'Dallas, TX', '22222222-2222-2222-2222-222222222222'),
  ('aaaaaaaa-0000-0000-0000-000000000002', 'Sam Hauler',  'company',        '555-0102', 'Denver, CO', NULL);

-- Equipment (varied rigs) ---------------------------------------------------
INSERT INTO equipment (id, unit_number, power_unit, trailer, has_ramps, deck_length_ft, has_duals) VALUES
  ('bbbbbbbb-0000-0000-0000-000000000001', 'T-101', 'tractor',  'step_deck_52', TRUE,  52, FALSE),
  ('bbbbbbbb-0000-0000-0000-000000000002', 'T-102', 'tractor',  'rgn_lowboy',   FALSE, NULL, FALSE),
  ('bbbbbbbb-0000-0000-0000-000000000003', 'T-103', 'tractor',  'flatbed_52',   FALSE, 52, FALSE),
  ('bbbbbbbb-0000-0000-0000-000000000004', 'R-201', 'ram_3500', 'car_carrier',  FALSE, NULL, TRUE);

INSERT INTO driver_equipment (driver_id, equipment_id) VALUES
  ('aaaaaaaa-0000-0000-0000-000000000001', 'bbbbbbbb-0000-0000-0000-000000000002'),
  ('aaaaaaaa-0000-0000-0000-000000000002', 'bbbbbbbb-0000-0000-0000-000000000004');

-- Locations -----------------------------------------------------------------
INSERT INTO location (id, line1, city, state, postal_code) VALUES
  ('cccccccc-0000-0000-0000-000000000001', '1 Yard Rd',  'Dallas',  'TX', '75001'),
  ('cccccccc-0000-0000-0000-000000000002', '9 Dock St',  'Denver',  'CO', '80202'),
  ('cccccccc-0000-0000-0000-000000000003', '40 Farm Ln', 'Lubbock', 'TX', '79401');

-- Parties & commodities -----------------------------------------------------
INSERT INTO shipper (id, name, location_id, contact) VALUES
  ('dddddddd-0000-0000-0000-000000000001', 'AgriCorp', 'cccccccc-0000-0000-0000-000000000003', '555-0200');

INSERT INTO receiver (id, name, location_id, contact) VALUES
  ('eeeeeeee-0000-0000-0000-000000000001', 'Mountain Rentals', 'cccccccc-0000-0000-0000-000000000002', '555-0300');

INSERT INTO commodity (id, description, category, weight_lbs) VALUES
  ('ffffffff-0000-0000-0000-000000000001', 'John Deere tractor', 'farm_equipment', 18000),
  ('ffffffff-0000-0000-0000-000000000002', 'Sedan x4',           'vehicles',        12000);

-- Dispatch week (Monday 2026-06-01) -----------------------------------------
INSERT INTO dispatch_week (id, week_start) VALUES
  ('99999999-0000-0000-0000-000000000001', DATE '2026-06-01');

-- A dispatched load: RGN low-boy hauling farm equipment, long-haul ----------
INSERT INTO load (
  id, dispatch_week_id, driver_id, equipment_id, shipper_id, receiver_id,
  commodity_id, pickup_id, dropoff_id, run_type, deadhead_miles, loaded_miles,
  rate, status
) VALUES (
  '88888888-0000-0000-0000-000000000001',
  '99999999-0000-0000-0000-000000000001',
  'aaaaaaaa-0000-0000-0000-000000000001',  -- Pat (owner-operator)
  'bbbbbbbb-0000-0000-0000-000000000002',  -- RGN low-boy
  'dddddddd-0000-0000-0000-000000000001',
  'eeeeeeee-0000-0000-0000-000000000001',
  'ffffffff-0000-0000-0000-000000000001',  -- farm equipment
  'cccccccc-0000-0000-0000-000000000003',  -- pickup: Lubbock
  'cccccccc-0000-0000-0000-000000000002',  -- dropoff: Denver
  'long_haul', 85.0, 780.0, 4200.00, 'dispatched'
);

COMMIT;

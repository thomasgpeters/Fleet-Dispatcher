-- Fleet Dispatcher — seed data
--
-- Loads the lookup/reference tables, then a small illustrative set of domain
-- objects. Apply AFTER schema.sql:
--   psql "$DATABASE_URL" -f database/seed_data.sql

BEGIN;

-- ===========================================================================
-- Lookup tables (explicit sequential ids so domain FKs are stable)
-- ===========================================================================
INSERT INTO driver_type (id, code, name, default_percent, owner_bears_costs) VALUES
  (1, 'company',        'Company Driver', 30, FALSE),
  (2, 'owner_operator', 'Owner-Operator', 70, TRUE);

INSERT INTO run_type (id, code, name) VALUES
  (1, 'long_haul', 'Long-haul'),
  (2, 'regional',  'Regional');

INSERT INTO load_status (id, code, name) VALUES
  (1, 'draft',      'Draft'),
  (2, 'dispatched', 'Dispatched'),
  (3, 'in_transit', 'In transit'),
  (4, 'delivered',  'Delivered'),
  (5, 'settled',    'Settled'),
  (6, 'cancelled',  'Cancelled');

INSERT INTO power_unit (id, code, name) VALUES
  (1, 'tractor',  'Tractor (semi)'),
  (2, 'ram_3500', 'Dodge RAM 3500'),
  (3, 'ram_4500', 'Dodge RAM 4500');

INSERT INTO trailer_type (id, code, name) VALUES
  (1, 'step_deck_52', 'Step-deck 52ft'),
  (2, 'rgn_lowboy',   'RGN low-boy'),
  (3, 'flatbed_52',   'Flatbed 52ft'),
  (4, 'car_carrier',  'Car carrier'),
  (5, 'none',         'None (power unit only)');

INSERT INTO app_role (id, code, name) VALUES
  (1, 'dispatcher', 'Dispatcher'),
  (2, 'driver',     'Driver'),
  (3, 'updater',    'Updater');

INSERT INTO commodity_category (id, code, name) VALUES
  (1, 'vehicles',        'Vehicles'),
  (2, 'heavy_equipment', 'Heavy equipment'),
  (3, 'farm_equipment',  'Farm equipment'),
  (4, 'generators',      'Power generators'),
  (5, 'lifts',           'Lifts');

-- Keep IDENTITY sequences ahead of the explicit ids inserted above.
SELECT setval(pg_get_serial_sequence('driver_type', 'id'),         (SELECT max(id) FROM driver_type));
SELECT setval(pg_get_serial_sequence('run_type', 'id'),            (SELECT max(id) FROM run_type));
SELECT setval(pg_get_serial_sequence('load_status', 'id'),         (SELECT max(id) FROM load_status));
SELECT setval(pg_get_serial_sequence('power_unit', 'id'),          (SELECT max(id) FROM power_unit));
SELECT setval(pg_get_serial_sequence('trailer_type', 'id'),        (SELECT max(id) FROM trailer_type));
SELECT setval(pg_get_serial_sequence('app_role', 'id'),            (SELECT max(id) FROM app_role));
SELECT setval(pg_get_serial_sequence('commodity_category', 'id'),  (SELECT max(id) FROM commodity_category));

-- ===========================================================================
-- Domain objects (explicit UUIDs for readable cross-references)
-- ===========================================================================

-- Users (one per role) ------------------------------------------------------
INSERT INTO app_user (id, username, full_name, email, app_role_id) VALUES
  ('11111111-1111-1111-1111-111111111111', 'dispatch1', 'Dana Dispatcher', 'dana@example.com', 1),
  ('22222222-2222-2222-2222-222222222222', 'driver1',   'Pat Diesel',      'pat@example.com',  2),
  ('33333333-3333-3333-3333-333333333333', 'updater1',  'Uma Updater',     'uma@example.com',  3);

-- Drivers (company + owner-operator) ----------------------------------------
INSERT INTO driver (id, name, driver_type_id, phone, home_base, user_id) VALUES
  ('aaaaaaaa-0000-0000-0000-000000000001', 'Pat Diesel', 2, '555-0101', 'Dallas, TX', '22222222-2222-2222-2222-222222222222'),
  ('aaaaaaaa-0000-0000-0000-000000000002', 'Sam Hauler', 1, '555-0102', 'Denver, CO', NULL);

-- Equipment (varied rigs) ---------------------------------------------------
INSERT INTO equipment (id, unit_number, power_unit_id, trailer_type_id, has_ramps, deck_length_ft, has_duals) VALUES
  ('bbbbbbbb-0000-0000-0000-000000000001', 'T-101', 1, 1, TRUE,  52,   FALSE),  -- tractor + step-deck 52 w/ ramps
  ('bbbbbbbb-0000-0000-0000-000000000002', 'T-102', 1, 2, FALSE, NULL, FALSE),  -- tractor + RGN low-boy
  ('bbbbbbbb-0000-0000-0000-000000000003', 'T-103', 1, 3, FALSE, 52,   FALSE),  -- tractor + flatbed 52
  ('bbbbbbbb-0000-0000-0000-000000000004', 'R-201', 2, 4, FALSE, NULL, TRUE);   -- RAM 3500 duals + car carrier

INSERT INTO driver_equipment (id, driver_id, equipment_id) VALUES
  ('a1a1a1a1-0000-0000-0000-000000000001', 'aaaaaaaa-0000-0000-0000-000000000001', 'bbbbbbbb-0000-0000-0000-000000000002'),
  ('a1a1a1a1-0000-0000-0000-000000000002', 'aaaaaaaa-0000-0000-0000-000000000002', 'bbbbbbbb-0000-0000-0000-000000000004');

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

INSERT INTO commodity (id, description, commodity_category_id, weight_lbs) VALUES
  ('ffffffff-0000-0000-0000-000000000001', 'John Deere tractor', 3, 18000),  -- farm_equipment
  ('ffffffff-0000-0000-0000-000000000002', 'Sedan x4',           1, 12000);  -- vehicles

-- Dispatch week (Monday 2026-06-01) -----------------------------------------
INSERT INTO dispatch_week (id, week_start) VALUES
  ('99999999-0000-0000-0000-000000000001', DATE '2026-06-01');

-- A dispatched load: RGN low-boy hauling farm equipment, long-haul ----------
INSERT INTO load (
  id, dispatch_week_id, driver_id, equipment_id, shipper_id, receiver_id,
  commodity_id, pickup_id, dropoff_id, run_type_id, load_status_id,
  deadhead_miles, loaded_miles, rate
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
  1,  -- run_type = long_haul
  2,  -- load_status = dispatched
  85.0, 780.0, 4200.00
);

COMMIT;

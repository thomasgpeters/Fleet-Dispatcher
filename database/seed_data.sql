-- Fleet Dispatcher — seed data
--
-- Loads the lookup/reference tables, then a small illustrative set of domain
-- objects. Apply AFTER schema.sql:
--   psql "$DATABASE_URL" -f database/seed_data.sql

BEGIN;

-- Fleet Dispatcher lives in its own schema (shared instance).
SET search_path TO fleet, public;

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

-- Trailer types carry a colour so rigs are colour-coded by type on the board.
INSERT INTO trailer_type (id, code, name, color_hex) VALUES
  (1, 'step_deck_52', 'Step-deck 52ft',         '#3b82c4'),  -- blue
  (2, 'rgn_lowboy',   'RGN low-boy',            '#8a5cf6'),  -- violet
  (3, 'flatbed_52',   'Flatbed 52ft',           '#3fa66a'),  -- green
  (4, 'car_carrier',  'Car carrier',            '#e07b39'),  -- orange
  (5, 'none',         'None (power unit only)', '#6b7a90');  -- steel

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

-- Avatar palette — curated, accessible on both light and dark panels. Users
-- self-pick in their profile; admins assign colours to login-less drivers.
INSERT INTO avatar_color (id, code, name, hex) VALUES
  (1,  'steel',  'Steel',  '#6b7a90'),
  (2,  'red',    'Red',    '#d9534f'),
  (3,  'orange', 'Orange', '#e07b39'),
  (4,  'gold',   'Gold',   '#c9a227'),
  (5,  'green',  'Green',  '#3fa66a'),
  (6,  'teal',   'Teal',   '#2f9e94'),
  (7,  'sky',    'Sky',    '#3b82c4'),
  (8,  'indigo', 'Indigo', '#5b6bd6'),
  (9,  'violet', 'Violet', '#8a5cf6'),
  (10, 'rose',   'Rose',   '#d6608f');

-- Keep IDENTITY sequences ahead of the explicit ids inserted above.
SELECT setval(pg_get_serial_sequence('avatar_color', 'id'),        (SELECT max(id) FROM avatar_color));
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
-- DEV CREDENTIALS: all three demo users have password 'fleet123'. The hashes are
-- werkzeug pbkdf2:sha256 (ALS's default check_password_hash verifies them).
-- Regenerate / change for any real deployment — see docs/AUTHENTICATION.md.
INSERT INTO app_user (id, username, full_name, email, app_role_id, password_hash, phone, title, avatar_color_id) VALUES
  ('11111111-1111-1111-1111-111111111111', 'dispatch1', 'Dana Dispatcher', 'dana@example.com', 1,
   'pbkdf2:sha256:600000$711621b8ed38739b$1bcab2036eda81044bb0e6841a19f09683bedcb97f0c27dacb6e52db981e9d34',
   '555-0100', 'Lead Dispatcher', 8),   -- indigo
  ('22222222-2222-2222-2222-222222222222', 'driver1',   'Pat Diesel',      'pat@example.com',  2,
   'pbkdf2:sha256:600000$3d4791e76e547105$b5a2b998f49605430828ad1adc0f76458546d255111d8378fe2ca13bf7f4d33b',
   '555-0101', 'Owner-Operator', 6),    -- teal
  ('33333333-3333-3333-3333-333333333333', 'updater1',  'Uma Updater',     'uma@example.com',  3,
   'pbkdf2:sha256:600000$f99fb81bfd5651ff$05475d4d2da19ccbfc98bf1164fce3afb830fe1ed07c7495323da703c27000f8',
   '555-0102', 'Dispatch Updater', 4);  -- gold

-- Drivers (company + owner-operator) ----------------------------------------
INSERT INTO driver (id, name, driver_type_id, phone, home_base, user_id, avatar_color_id) VALUES
  ('aaaaaaaa-0000-0000-0000-000000000001', 'Pat Diesel', 2, '555-0101', 'Dallas, TX', '22222222-2222-2222-2222-222222222222', 6),   -- teal (matches login)
  ('aaaaaaaa-0000-0000-0000-000000000002', 'Sam Hauler', 1, '555-0102', 'Denver, CO', NULL, 2);                                     -- red (admin-assigned)

-- Equipment (varied rigs) ---------------------------------------------------
INSERT INTO equipment (id, unit_number, power_unit_id, trailer_type_id, has_ramps, deck_length_ft, weight_capacity_lbs, has_duals) VALUES
  ('bbbbbbbb-0000-0000-0000-000000000001', 'T-101', 1, 1, TRUE,  52,   48000, FALSE),  -- tractor + step-deck 52 w/ ramps
  ('bbbbbbbb-0000-0000-0000-000000000002', 'T-102', 1, 2, FALSE, 29,   80000, FALSE),  -- tractor + RGN low-boy
  ('bbbbbbbb-0000-0000-0000-000000000003', 'T-103', 1, 3, FALSE, 52,   48000, FALSE),  -- tractor + flatbed 52
  ('bbbbbbbb-0000-0000-0000-000000000004', 'R-201', 2, 4, FALSE, 24,   12000, TRUE);   -- RAM 3500 duals + car carrier

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
  deadhead_miles, loaded_miles, rate, deck_feet, weight_lbs, pickup_date, delivery_date
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
  85.0, 780.0, 4200.00,
  29.0, 42000,                             -- footprint: fills the low-boy deck
  DATE '2026-06-02', DATE '2026-06-04'    -- within week of 2026-06-01
);

-- ===========================================================================
-- Board demo fleet — a fuller roster so the desktop week board is populated.
-- The week board (portals/dispatcher-desktop) rows drivers and places each load
-- in the column matching its pickup_date within the CURRENT (Sunday→Saturday)
-- week. So the loads below use pickup dates computed relative to CURRENT_DATE —
-- they always land in the visible week, no matter when the seed is applied.
-- (Static dates would fall outside the week and the board would look empty.)
--
-- 10 drivers total (2 above + 8 here). Eight carry loads this week; two —
-- Sam Hauler and Nadia Petrov — are intentionally left load-free to show the
-- "available driver" case.
-- ===========================================================================

-- 8 more drivers (→ 10 total). Mix of company + owner-operator. --------------
INSERT INTO driver (id, name, driver_type_id, phone, home_base, user_id, avatar_color_id) VALUES
  ('aaaaaaaa-0000-0000-0000-000000000003', 'Marcus Reyes',   1, '555-0110', 'Oklahoma City, OK', NULL, 7),   -- sky
  ('aaaaaaaa-0000-0000-0000-000000000004', 'Dwayne Kolb',    2, '555-0111', 'El Paso, TX',       NULL, 9),   -- violet
  ('aaaaaaaa-0000-0000-0000-000000000005', 'Tanya Brooks',   1, '555-0112', 'Denver, CO',        NULL, 10),  -- rose
  ('aaaaaaaa-0000-0000-0000-000000000006', 'Hector Alvarez', 2, '555-0113', 'El Paso, TX',       NULL, 3),   -- orange
  ('aaaaaaaa-0000-0000-0000-000000000007', 'Jill Sorensen',  1, '555-0114', 'Wichita, KS',       NULL, 5),   -- green
  ('aaaaaaaa-0000-0000-0000-000000000008', 'Ravi Nair',      1, '555-0115', 'Lubbock, TX',       NULL, 4),   -- gold
  ('aaaaaaaa-0000-0000-0000-000000000009', 'Bill Tran',      2, '555-0116', 'Houston, TX',       NULL, 8),   -- indigo
  ('aaaaaaaa-0000-0000-0000-00000000000a', 'Nadia Petrov',   1, '555-0117', 'Salt Lake City, UT',NULL, 1);   -- steel

-- 6 more rigs (→ 10 total). Power units 1=tractor, 2=RAM; trailer types match. -
INSERT INTO equipment (id, unit_number, power_unit_id, trailer_type_id, has_ramps, deck_length_ft, weight_capacity_lbs, has_duals) VALUES
  ('bbbbbbbb-0000-0000-0000-000000000005', 'T-104', 1, 1, TRUE,  52, 48000, FALSE),  -- step-deck 52 w/ ramps
  ('bbbbbbbb-0000-0000-0000-000000000006', 'T-105', 1, 3, FALSE, 52, 48000, FALSE),  -- flatbed 52
  ('bbbbbbbb-0000-0000-0000-000000000007', 'T-106', 1, 2, FALSE, 29, 80000, FALSE),  -- RGN low-boy
  ('bbbbbbbb-0000-0000-0000-000000000008', 'T-107', 1, 1, TRUE,  48, 45000, FALSE),  -- step-deck 48 w/ ramps
  ('bbbbbbbb-0000-0000-0000-000000000009', 'T-108', 1, 3, FALSE, 48, 45000, FALSE),  -- flatbed 48
  ('bbbbbbbb-0000-0000-0000-00000000000a', 'R-202', 2, 4, FALSE, 24, 12000, TRUE);   -- RAM 3500 + car carrier

INSERT INTO driver_equipment (id, driver_id, equipment_id) VALUES
  ('a1a1a1a1-0000-0000-0000-000000000003', 'aaaaaaaa-0000-0000-0000-000000000003', 'bbbbbbbb-0000-0000-0000-000000000005'),
  ('a1a1a1a1-0000-0000-0000-000000000004', 'aaaaaaaa-0000-0000-0000-000000000004', 'bbbbbbbb-0000-0000-0000-000000000006'),
  ('a1a1a1a1-0000-0000-0000-000000000005', 'aaaaaaaa-0000-0000-0000-000000000005', 'bbbbbbbb-0000-0000-0000-000000000007'),
  ('a1a1a1a1-0000-0000-0000-000000000006', 'aaaaaaaa-0000-0000-0000-000000000006', 'bbbbbbbb-0000-0000-0000-00000000000a'),
  ('a1a1a1a1-0000-0000-0000-000000000007', 'aaaaaaaa-0000-0000-0000-000000000007', 'bbbbbbbb-0000-0000-0000-000000000008'),
  ('a1a1a1a1-0000-0000-0000-000000000008', 'aaaaaaaa-0000-0000-0000-000000000008', 'bbbbbbbb-0000-0000-0000-000000000009'),
  ('a1a1a1a1-0000-0000-0000-000000000009', 'aaaaaaaa-0000-0000-0000-000000000009', 'bbbbbbbb-0000-0000-0000-000000000003');

-- More locations for lane variety. ------------------------------------------
INSERT INTO location (id, line1, city, state, postal_code) VALUES
  ('cccccccc-0000-0000-0000-000000000004', '200 Rail Yard', 'Oklahoma City', 'OK', '73102'),
  ('cccccccc-0000-0000-0000-000000000005', '55 Depot Ave',  'Amarillo',      'TX', '79101'),
  ('cccccccc-0000-0000-0000-000000000006', '12 Logistics Pkwy', 'Kansas City','MO', '64106'),
  ('cccccccc-0000-0000-0000-000000000007', '8 Mesa Rd',     'Albuquerque',   'NM', '87102'),
  ('cccccccc-0000-0000-0000-000000000008', '77 Sunbelt Dr', 'Phoenix',       'AZ', '85004'),
  ('cccccccc-0000-0000-0000-000000000009', '400 Bayport',   'Houston',       'TX', '77002'),
  ('cccccccc-0000-0000-0000-00000000000a', '19 Wasatch Way','Salt Lake City','UT', '84101'),
  ('cccccccc-0000-0000-0000-00000000000b', '3 Border Blvd', 'El Paso',       'TX', '79901');

-- More shippers / receivers. -------------------------------------------------
INSERT INTO shipper (id, name, location_id, contact) VALUES
  ('dddddddd-0000-0000-0000-000000000002', 'Permian Machinery',   'cccccccc-0000-0000-0000-000000000005', '555-0210'),
  ('dddddddd-0000-0000-0000-000000000003', 'Sooner Equipment',    'cccccccc-0000-0000-0000-000000000004', '555-0211'),
  ('dddddddd-0000-0000-0000-000000000004', 'Rio Grande Motors',   'cccccccc-0000-0000-0000-00000000000b', '555-0212'),
  ('dddddddd-0000-0000-0000-000000000005', 'Front Range Rentals', 'cccccccc-0000-0000-0000-000000000002', '555-0213'),
  ('dddddddd-0000-0000-0000-000000000006', 'Gulf Coast Supply',   'cccccccc-0000-0000-0000-000000000009', '555-0214');

INSERT INTO receiver (id, name, location_id, contact) VALUES
  ('eeeeeeee-0000-0000-0000-000000000002', 'Desert Logistics', 'cccccccc-0000-0000-0000-000000000008', '555-0310'),
  ('eeeeeeee-0000-0000-0000-000000000003', 'Prairie Dealers',  'cccccccc-0000-0000-0000-000000000006', '555-0311'),
  ('eeeeeeee-0000-0000-0000-000000000004', 'Wasatch Rentals',  'cccccccc-0000-0000-0000-00000000000a', '555-0312'),
  ('eeeeeeee-0000-0000-0000-000000000005', 'Llano Farms',      'cccccccc-0000-0000-0000-000000000003', '555-0313'),
  ('eeeeeeee-0000-0000-0000-000000000006', 'Bayou Haul',       'cccccccc-0000-0000-0000-000000000009', '555-0314');

-- More commodities across categories. ---------------------------------------
INSERT INTO commodity (id, description, commodity_category_id, weight_lbs) VALUES
  ('ffffffff-0000-0000-0000-000000000003', 'Excavator',            2, 34000),  -- heavy_equipment
  ('ffffffff-0000-0000-0000-000000000004', 'Skid steer',           2,  9000),  -- heavy_equipment
  ('ffffffff-0000-0000-0000-000000000005', 'Diesel generator 500kW',4, 16000),  -- generators
  ('ffffffff-0000-0000-0000-000000000006', 'Scissor lift x2',      5,  8000),  -- lifts
  ('ffffffff-0000-0000-0000-000000000007', 'Combine harvester',    3, 28000),  -- farm_equipment
  ('ffffffff-0000-0000-0000-000000000008', 'Pickup trucks x3',     1, 15000);  -- vehicles

-- A dispatch week anchored to the CURRENT week's Monday (schema requires a
-- Monday). date_trunc('week', …) is Monday-based, so this is always valid.
INSERT INTO dispatch_week (id, week_start) VALUES
  ('99999999-0000-0000-0000-000000000002', date_trunc('week', CURRENT_DATE)::date)
ON CONFLICT (week_start) DO NOTHING;

-- Loads for the current week. pickup_date = this week's Sunday + <offset> days
-- (Sunday=0 … Saturday=6), matching the board's Sunday-first columns. Each row
-- carries an <offset> and <transit> (delivery = pickup + transit). Eight drivers
-- get work; Sam Hauler (…0002) and Nadia Petrov (…000a) are left free.
WITH wk AS (
  -- Sunday of the current week (mirrors the desktop board's currentWeekDays()).
  SELECT (CURRENT_DATE - EXTRACT(DOW FROM CURRENT_DATE)::int) AS sun,
         '99999999-0000-0000-0000-000000000002'::uuid          AS week_id
)
INSERT INTO load (
  id, dispatch_week_id, driver_id, equipment_id, shipper_id, receiver_id,
  commodity_id, pickup_id, dropoff_id, run_type_id, load_status_id,
  deadhead_miles, loaded_miles, rate, deck_feet, weight_lbs, pickup_date, delivery_date
)
SELECT v.id, wk.week_id, v.driver_id, v.equipment_id, v.shipper_id, v.receiver_id,
       v.commodity_id, v.pickup_id, v.dropoff_id, v.run_type_id, v.load_status_id,
       v.deadhead_miles, v.loaded_miles, v.rate, v.deck_feet, v.weight_lbs,
       (wk.sun + v.day_offset),
       (wk.sun + v.day_offset + v.transit)
FROM wk, (VALUES
  -- id, driver, equipment, shipper, receiver, commodity, pickup, dropoff, run, status, deadhead, loaded, rate, deck_ft, wt, day_offset, transit
  ('88888888-0000-0000-0000-000000000101'::uuid, 'aaaaaaaa-0000-0000-0000-000000000001'::uuid, 'bbbbbbbb-0000-0000-0000-000000000002'::uuid, 'dddddddd-0000-0000-0000-000000000001'::uuid, 'eeeeeeee-0000-0000-0000-000000000001'::uuid, 'ffffffff-0000-0000-0000-000000000001'::uuid, 'cccccccc-0000-0000-0000-000000000003'::uuid, 'cccccccc-0000-0000-0000-000000000002'::uuid, 1, 2, 85.0, 780.0, 4200.00, 29.0, 42000, 1, 2),
  ('88888888-0000-0000-0000-000000000102'::uuid, 'aaaaaaaa-0000-0000-0000-000000000001'::uuid, 'bbbbbbbb-0000-0000-0000-000000000002'::uuid, 'dddddddd-0000-0000-0000-000000000002'::uuid, 'eeeeeeee-0000-0000-0000-000000000002'::uuid, 'ffffffff-0000-0000-0000-000000000003'::uuid, 'cccccccc-0000-0000-0000-000000000005'::uuid, 'cccccccc-0000-0000-0000-000000000008'::uuid, 1, 3, 40.0, 610.0, 5200.00, 29.0, 34000, 4, 2),
  ('88888888-0000-0000-0000-000000000103'::uuid, 'aaaaaaaa-0000-0000-0000-000000000003'::uuid, 'bbbbbbbb-0000-0000-0000-000000000005'::uuid, 'dddddddd-0000-0000-0000-000000000003'::uuid, 'eeeeeeee-0000-0000-0000-000000000003'::uuid, 'ffffffff-0000-0000-0000-000000000004'::uuid, 'cccccccc-0000-0000-0000-000000000004'::uuid, 'cccccccc-0000-0000-0000-000000000006'::uuid, 2, 2, 30.0, 350.0, 2100.00, 20.0,  9000, 1, 1),
  ('88888888-0000-0000-0000-000000000104'::uuid, 'aaaaaaaa-0000-0000-0000-000000000003'::uuid, 'bbbbbbbb-0000-0000-0000-000000000005'::uuid, 'dddddddd-0000-0000-0000-000000000001'::uuid, 'eeeeeeee-0000-0000-0000-000000000005'::uuid, 'ffffffff-0000-0000-0000-000000000007'::uuid, 'cccccccc-0000-0000-0000-000000000003'::uuid, 'cccccccc-0000-0000-0000-000000000005'::uuid, 2, 4, 25.0, 120.0, 1500.00, 40.0, 28000, 3, 1),
  ('88888888-0000-0000-0000-000000000105'::uuid, 'aaaaaaaa-0000-0000-0000-000000000004'::uuid, 'bbbbbbbb-0000-0000-0000-000000000006'::uuid, 'dddddddd-0000-0000-0000-000000000004'::uuid, 'eeeeeeee-0000-0000-0000-000000000006'::uuid, 'ffffffff-0000-0000-0000-000000000008'::uuid, 'cccccccc-0000-0000-0000-00000000000b'::uuid, 'cccccccc-0000-0000-0000-000000000009'::uuid, 1, 2, 60.0, 750.0, 3900.00, 45.0, 15000, 2, 2),
  ('88888888-0000-0000-0000-000000000106'::uuid, 'aaaaaaaa-0000-0000-0000-000000000005'::uuid, 'bbbbbbbb-0000-0000-0000-000000000007'::uuid, 'dddddddd-0000-0000-0000-000000000005'::uuid, 'eeeeeeee-0000-0000-0000-000000000004'::uuid, 'ffffffff-0000-0000-0000-000000000005'::uuid, 'cccccccc-0000-0000-0000-000000000002'::uuid, 'cccccccc-0000-0000-0000-00000000000a'::uuid, 1, 3, 50.0, 520.0, 4600.00, 29.0, 16000, 2, 2),
  ('88888888-0000-0000-0000-000000000107'::uuid, 'aaaaaaaa-0000-0000-0000-000000000005'::uuid, 'bbbbbbbb-0000-0000-0000-000000000007'::uuid, 'dddddddd-0000-0000-0000-000000000005'::uuid, 'eeeeeeee-0000-0000-0000-000000000001'::uuid, 'ffffffff-0000-0000-0000-000000000006'::uuid, 'cccccccc-0000-0000-0000-000000000002'::uuid, 'cccccccc-0000-0000-0000-000000000007'::uuid, 2, 2, 20.0, 440.0, 2800.00, 24.0,  8000, 5, 1),
  ('88888888-0000-0000-0000-000000000108'::uuid, 'aaaaaaaa-0000-0000-0000-000000000006'::uuid, 'bbbbbbbb-0000-0000-0000-00000000000a'::uuid, 'dddddddd-0000-0000-0000-000000000004'::uuid, 'eeeeeeee-0000-0000-0000-000000000002'::uuid, 'ffffffff-0000-0000-0000-000000000002'::uuid, 'cccccccc-0000-0000-0000-00000000000b'::uuid, 'cccccccc-0000-0000-0000-000000000008'::uuid, 2, 2, 35.0, 400.0, 2600.00, 24.0, 12000, 3, 1),
  ('88888888-0000-0000-0000-000000000109'::uuid, 'aaaaaaaa-0000-0000-0000-000000000007'::uuid, 'bbbbbbbb-0000-0000-0000-000000000008'::uuid, 'dddddddd-0000-0000-0000-000000000003'::uuid, 'eeeeeeee-0000-0000-0000-000000000003'::uuid, 'ffffffff-0000-0000-0000-000000000003'::uuid, 'cccccccc-0000-0000-0000-000000000004'::uuid, 'cccccccc-0000-0000-0000-000000000006'::uuid, 1, 1, 45.0, 330.0, 3100.00, 20.0, 34000, 4, 1),
  ('88888888-0000-0000-0000-00000000010a'::uuid, 'aaaaaaaa-0000-0000-0000-000000000008'::uuid, 'bbbbbbbb-0000-0000-0000-000000000009'::uuid, 'dddddddd-0000-0000-0000-000000000002'::uuid, 'eeeeeeee-0000-0000-0000-000000000005'::uuid, 'ffffffff-0000-0000-0000-000000000008'::uuid, 'cccccccc-0000-0000-0000-000000000005'::uuid, 'cccccccc-0000-0000-0000-000000000003'::uuid, 2, 4, 15.0, 120.0, 1400.00, 45.0, 15000, 2, 1),
  ('88888888-0000-0000-0000-00000000010b'::uuid, 'aaaaaaaa-0000-0000-0000-000000000008'::uuid, 'bbbbbbbb-0000-0000-0000-000000000009'::uuid, 'dddddddd-0000-0000-0000-000000000001'::uuid, 'eeeeeeee-0000-0000-0000-000000000006'::uuid, 'ffffffff-0000-0000-0000-000000000007'::uuid, 'cccccccc-0000-0000-0000-000000000003'::uuid, 'cccccccc-0000-0000-0000-000000000009'::uuid, 1, 2, 30.0, 540.0, 3600.00, 40.0, 28000, 5, 2),
  ('88888888-0000-0000-0000-00000000010c'::uuid, 'aaaaaaaa-0000-0000-0000-000000000009'::uuid, 'bbbbbbbb-0000-0000-0000-000000000003'::uuid, 'dddddddd-0000-0000-0000-000000000006'::uuid, 'eeeeeeee-0000-0000-0000-000000000002'::uuid, 'ffffffff-0000-0000-0000-000000000006'::uuid, 'cccccccc-0000-0000-0000-000000000009'::uuid, 'cccccccc-0000-0000-0000-000000000008'::uuid, 1, 3, 55.0, 680.0, 4100.00, 24.0,  8000, 3, 2)
) AS v(id, driver_id, equipment_id, shipper_id, receiver_id, commodity_id, pickup_id, dropoff_id, run_type_id, load_status_id, deadhead_miles, loaded_miles, rate, deck_feet, weight_lbs, day_offset, transit);

-- ===========================================================================
-- Messaging (lookups + a sample group channel with messages and an attachment)
-- ===========================================================================
INSERT INTO channel_type (id, code, name) VALUES
  (1, 'direct',    'Direct message'),
  (2, 'group',     'Group'),
  (3, 'broadcast', 'Broadcast');

INSERT INTO channel_member_role (id, code, name) VALUES
  (1, 'owner',  'Owner'),
  (2, 'member', 'Member'),
  (3, 'admin',  'Admin');

INSERT INTO channel_member_status (id, code, name) VALUES
  (1, 'active', 'Active'),
  (2, 'muted',  'Muted'),
  (3, 'banned', 'Banned');

INSERT INTO document_type (id, code, name) VALUES
  (1, 'document',         'Document'),
  (2, 'image',            'Image'),
  (3, 'bill_of_lading',   'Bill of Lading'),
  (4, 'invoice',          'Invoice'),
  (5, 'inspection_photo', 'Inspection photo'),
  (6, 'license',          'License / permit'),
  (7, 'other',            'Other');

INSERT INTO pin_scope (id, code, name) VALUES
  (1, 'self',     'Only me'),
  (2, 'channel',  'Everyone in the channel'),
  (3, 'everyone', 'Everyone');

SELECT setval(pg_get_serial_sequence('channel_type', 'id'),          (SELECT max(id) FROM channel_type));
SELECT setval(pg_get_serial_sequence('channel_member_role', 'id'),   (SELECT max(id) FROM channel_member_role));
SELECT setval(pg_get_serial_sequence('channel_member_status', 'id'), (SELECT max(id) FROM channel_member_status));
SELECT setval(pg_get_serial_sequence('document_type', 'id'),         (SELECT max(id) FROM document_type));
SELECT setval(pg_get_serial_sequence('pin_scope', 'id'),             (SELECT max(id) FROM pin_scope));

-- A group channel for the week's dispatch chatter.
INSERT INTO channel (id, name, channel_type_id, created_by) VALUES
  ('c4a11e10-0000-0000-0000-000000000001', 'Week of 2026-06-01 Dispatch', 2,
   '11111111-1111-1111-1111-111111111111');  -- created by Dana (dispatcher)

-- A broadcast channel: only owner/admins post; members read (enforced by the
-- LogicBank rule in als-extensions/). Models a fleet-wide announcement board.
INSERT INTO channel (id, name, channel_type_id, created_by) VALUES
  ('c4a11e10-0000-0000-0000-000000000002', 'Fleet Announcements', 3,
   '11111111-1111-1111-1111-111111111111');  -- created by Dana (dispatcher)

INSERT INTO channel_member (id, channel_id, user_id, member_role_id) VALUES
  ('c4e3b001-0000-0000-0000-000000000001', 'c4a11e10-0000-0000-0000-000000000001', '11111111-1111-1111-1111-111111111111', 1),  -- Dana owner
  ('c4e3b001-0000-0000-0000-000000000002', 'c4a11e10-0000-0000-0000-000000000001', '22222222-2222-2222-2222-222222222222', 2),  -- Pat (driver)
  ('c4e3b001-0000-0000-0000-000000000003', 'c4a11e10-0000-0000-0000-000000000001', '33333333-3333-3333-3333-333333333333', 2),  -- Uma (updater)
  -- Broadcast channel: Dana owns/posts; Pat & Uma are read-only members.
  ('c4e3b001-0000-0000-0000-000000000004', 'c4a11e10-0000-0000-0000-000000000002', '11111111-1111-1111-1111-111111111111', 1),  -- Dana owner
  ('c4e3b001-0000-0000-0000-000000000005', 'c4a11e10-0000-0000-0000-000000000002', '22222222-2222-2222-2222-222222222222', 2),  -- Pat (driver)
  ('c4e3b001-0000-0000-0000-000000000006', 'c4a11e10-0000-0000-0000-000000000002', '33333333-3333-3333-3333-333333333333', 2);  -- Uma (updater)

-- A forum topic in the group channel (focused thread for one lane).
INSERT INTO channel_topic (id, channel_id, name, created_by) VALUES
  ('70b1c000-0000-0000-0000-000000000001', 'c4a11e10-0000-0000-0000-000000000001',
   'Lubbock -> Denver', '11111111-1111-1111-1111-111111111111');

-- Group-channel messages: the two below sit inside the lane topic; topic_id NULL
-- would place a message in the channel's General stream.
INSERT INTO message (id, channel_id, topic_id, author_id, body) VALUES
  ('5e55a6e0-0000-0000-0000-000000000001', 'c4a11e10-0000-0000-0000-000000000001', '70b1c000-0000-0000-0000-000000000001', '11111111-1111-1111-1111-111111111111', 'Pat, your Lubbock -> Denver load is dispatched. BOL attached.'),
  ('5e55a6e0-0000-0000-0000-000000000002', 'c4a11e10-0000-0000-0000-000000000001', '70b1c000-0000-0000-0000-000000000001', '22222222-2222-2222-2222-222222222222', 'Got it, rolling out Monday AM.');

-- A broadcast announcement (posted by the owner).
INSERT INTO message (id, channel_id, author_id, body) VALUES
  ('5e55a6e0-0000-0000-0000-000000000003', 'c4a11e10-0000-0000-0000-000000000002', '11111111-1111-1111-1111-111111111111', 'Reminder: ELD logs are due by 18:00 daily. Drive safe out there.');

-- A CMS document (a tiny text artifact stands in for a scanned BOL).
INSERT INTO document (id, title, document_type_id, filename, content_type, byte_size, checksum, data, uploaded_by) VALUES
  ('d0c00000-0000-0000-0000-000000000001', 'Bill of Lading — Lubbock to Denver', 3,
   'bol.txt', 'text/plain', 9, md5('BOL: scan'), decode('Qk9MOiBzY2Fu', 'base64'),
   '11111111-1111-1111-1111-111111111111');

-- The same document, reachable from the message AND from the load (CMS reuse).
INSERT INTO message_document (id, message_id, document_id) VALUES
  ('111d0c00-0000-0000-0000-000000000001', '5e55a6e0-0000-0000-0000-000000000001', 'd0c00000-0000-0000-0000-000000000001');

INSERT INTO load_document (id, load_id, document_id) VALUES
  ('222d0c00-0000-0000-0000-000000000001', '88888888-0000-0000-0000-000000000001', 'd0c00000-0000-0000-0000-000000000001');

-- Dana pins the dispatch message for the whole channel (scope = channel).
INSERT INTO message_pin (id, message_id, channel_id, pinned_by, pin_scope_id) VALUES
  ('9171ed00-0000-0000-0000-000000000001', '5e55a6e0-0000-0000-0000-000000000001',
   'c4a11e10-0000-0000-0000-000000000001', '11111111-1111-1111-1111-111111111111', 2);

-- Pat saves the BOL message to their personal archive with a note.
INSERT INTO saved_message (id, user_id, message_id, note) VALUES
  ('5a7ed000-0000-0000-0000-000000000001', '22222222-2222-2222-2222-222222222222',
   '5e55a6e0-0000-0000-0000-000000000001', 'BOL for the Denver run');

-- ===========================================================================
-- Telemetry — truck locations
-- ===========================================================================
INSERT INTO location_source (id, code, name) VALUES
  (1, 'airtag',        'Apple AirTag'),
  (2, 'google_device', 'Google device'),
  (3, 'phone_push',    'Driver phone (push)');

SELECT setval(pg_get_serial_sequence('location_source', 'id'), (SELECT max(id) FROM location_source));

-- A short track for Pat's RGN low-boy (T-102), Lubbock area heading north.
INSERT INTO position_report
  (equipment_id, driver_id, location_source_id, lat, lng, heading_deg, speed_mph, accuracy_m, recorded_at) VALUES
  ('bbbbbbbb-0000-0000-0000-000000000002', 'aaaaaaaa-0000-0000-0000-000000000001', 3, 33.5779, -101.8552,   5.0, 62.0,  8.0, now() - interval '20 minutes'),
  ('bbbbbbbb-0000-0000-0000-000000000002', 'aaaaaaaa-0000-0000-0000-000000000001', 3, 34.1850, -101.7060,   8.0, 64.0,  6.0, now() - interval '5 minutes'),
  -- Sam's RAM car carrier (R-201), Denver area.
  ('bbbbbbbb-0000-0000-0000-000000000004', 'aaaaaaaa-0000-0000-0000-000000000002', 1, 39.7392, -104.9903, 180.0,  0.0, 12.0, now() - interval '2 minutes');

-- ===========================================================================
-- Navigation (trip for Pat's load, waypoints, a truck-stop POI, a route)
-- ===========================================================================
INSERT INTO trip_status (id, code, name) VALUES
  (1, 'planned', 'Planned'), (2, 'active', 'Active'),
  (3, 'completed', 'Completed'), (4, 'cancelled', 'Cancelled');

INSERT INTO stop_type (id, code, name) VALUES
  (1, 'origin', 'Origin'), (2, 'destination', 'Destination'),
  (3, 'waypoint', 'Waypoint'), (4, 'fuel', 'Fuel'),
  (5, 'rest', 'Rest'), (6, 'truck_stop', 'Truck stop'),
  (7, 'lunch', 'Lunch'), (8, 'load_stop', 'Load stop');

INSERT INTO poi_category (id, code, name) VALUES
  (1, 'fuel', 'Fuel'), (2, 'rest_area', 'Rest area'),
  (3, 'weigh_station', 'Weigh station'), (4, 'truck_stop', 'Truck stop'),
  (5, 'customer', 'Customer');

SELECT setval(pg_get_serial_sequence('trip_status', 'id'),  (SELECT max(id) FROM trip_status));
SELECT setval(pg_get_serial_sequence('stop_type', 'id'),    (SELECT max(id) FROM stop_type));
SELECT setval(pg_get_serial_sequence('poi_category', 'id'), (SELECT max(id) FROM poi_category));

INSERT INTO trip (id, driver_id, equipment_id, load_id, trip_status_id, name, started_at) VALUES
  ('72190000-0000-0000-0000-000000000001',
   'aaaaaaaa-0000-0000-0000-000000000001',  -- Pat
   'bbbbbbbb-0000-0000-0000-000000000002',  -- RGN low-boy
   '88888888-0000-0000-0000-000000000001',  -- the Lubbock→Denver load
   2, 'Lubbock → Denver', now() - interval '20 minutes');

INSERT INTO waypoint (id, trip_id, seq, stop_type_id, label, lat, lng) VALUES
  ('7a900000-0000-0000-0000-000000000001', '72190000-0000-0000-0000-000000000001', 1, 1, 'Lubbock yard',   33.5779, -101.8552),
  ('7a900000-0000-0000-0000-000000000002', '72190000-0000-0000-0000-000000000001', 2, 4, 'Fuel — Amarillo', 35.2220, -101.8313),
  ('7a900000-0000-0000-0000-000000000003', '72190000-0000-0000-0000-000000000001', 3, 2, 'Denver dock',     39.7392, -104.9903);

INSERT INTO point_of_interest (id, name, poi_category_id, lat, lng, amenities) VALUES
  ('90100000-0000-0000-0000-000000000001', 'Big Rig Travel Center', 4, 35.1990, -101.8450, 'diesel,DEF,parking,showers');

INSERT INTO route (id, trip_id, origin_lat, origin_lng, dest_lat, dest_lng, distance_mi, drive_minutes, provider) VALUES
  ('209e0000-0000-0000-0000-000000000001', '72190000-0000-0000-0000-000000000001',
   33.5779, -101.8552, 39.7392, -104.9903, 780.0, 690, 'here');

COMMIT;

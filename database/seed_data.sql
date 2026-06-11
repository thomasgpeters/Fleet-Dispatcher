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
  deadhead_miles, loaded_miles, rate, pickup_date, delivery_date
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
  DATE '2026-06-02', DATE '2026-06-04'    -- within week of 2026-06-01
);

-- ===========================================================================
-- Messaging (lookups + a sample group channel with messages and an attachment)
-- ===========================================================================
INSERT INTO channel_type (id, code, name) VALUES
  (1, 'direct',    'Direct message'),
  (2, 'group',     'Group'),
  (3, 'broadcast', 'Broadcast');

INSERT INTO channel_member_role (id, code, name) VALUES
  (1, 'owner',  'Owner'),
  (2, 'member', 'Member');

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

SELECT setval(pg_get_serial_sequence('channel_type', 'id'),        (SELECT max(id) FROM channel_type));
SELECT setval(pg_get_serial_sequence('channel_member_role', 'id'), (SELECT max(id) FROM channel_member_role));
SELECT setval(pg_get_serial_sequence('document_type', 'id'),       (SELECT max(id) FROM document_type));
SELECT setval(pg_get_serial_sequence('pin_scope', 'id'),           (SELECT max(id) FROM pin_scope));

-- A group channel for the week's dispatch chatter.
INSERT INTO channel (id, name, channel_type_id, created_by) VALUES
  ('c4a11e10-0000-0000-0000-000000000001', 'Week of 2026-06-01 Dispatch', 2,
   '11111111-1111-1111-1111-111111111111');  -- created by Dana (dispatcher)

INSERT INTO channel_member (id, channel_id, user_id, member_role_id) VALUES
  ('c4e3b001-0000-0000-0000-000000000001', 'c4a11e10-0000-0000-0000-000000000001', '11111111-1111-1111-1111-111111111111', 1),  -- Dana owner
  ('c4e3b001-0000-0000-0000-000000000002', 'c4a11e10-0000-0000-0000-000000000001', '22222222-2222-2222-2222-222222222222', 2),  -- Pat (driver)
  ('c4e3b001-0000-0000-0000-000000000003', 'c4a11e10-0000-0000-0000-000000000001', '33333333-3333-3333-3333-333333333333', 2);  -- Uma (updater)

INSERT INTO message (id, channel_id, author_id, body) VALUES
  ('5e55a6e0-0000-0000-0000-000000000001', 'c4a11e10-0000-0000-0000-000000000001', '11111111-1111-1111-1111-111111111111', 'Pat, your Lubbock -> Denver load is dispatched. BOL attached.'),
  ('5e55a6e0-0000-0000-0000-000000000002', 'c4a11e10-0000-0000-0000-000000000001', '22222222-2222-2222-2222-222222222222', 'Got it, rolling out Monday AM.');

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
  (5, 'rest', 'Rest'), (6, 'truck_stop', 'Truck stop');

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

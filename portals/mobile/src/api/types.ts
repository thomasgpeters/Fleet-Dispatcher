// Domain resource types as exposed by the JSON:API (ApiLogicServer / SAFRS).
// These mirror the tables in ../../database/schema.sql: domain objects carry
// UUID ids and reference lookup tables by integer FK (…_id). Use JSON:API
// `include` to pull related lookup/resource rows when you need their labels.

// --- Lookup resources (sequential integer ids) ---
export interface DriverType {
  id: number;
  code: "company" | "owner_operator";
  name: string;
  default_percent: number; // 30 | 70
  owner_bears_costs: boolean;
}

export interface RunType {
  id: number;
  code: "long_haul" | "regional";
  name: string;
}

export interface LoadStatus {
  id: number;
  code:
    | "draft"
    | "dispatched"
    | "in_transit"
    | "delivered"
    | "settled"
    | "cancelled";
  name: string;
}

// --- Identity & access ---
export interface AppRole {
  id: number;
  code: "dispatcher" | "driver" | "updater";
  name: string;
}

export interface AppUser {
  id: string;
  username: string;
  full_name: string;
  email?: string;
  app_role_id: number; // -> AppRole
  active: boolean;
  // Profile (editable); password_hash is never exposed to the client.
  phone?: string;
  title?: string;
  timezone?: string;
  avatar_document_id?: string | null; // -> Document (profile photo via CMS)
  last_login_at?: string | null;
}

// Join: which equipment a driver is assigned (drives equipment context).
export interface DriverEquipment {
  id: string;
  driver_id: string;
  equipment_id: string;
}

// --- Domain resources (UUID ids, lookup FKs) ---
export interface Driver {
  id: string;
  name: string;
  driver_type_id: number; // -> DriverType
  phone?: string;
  email?: string;
  home_base?: string;
  active: boolean;
}

export interface Load {
  id: string;
  dispatch_week_id: string;
  driver_id?: string;
  equipment_id?: string;
  shipper_id: string;
  receiver_id: string;
  commodity_id: string;
  pickup_id: string;
  dropoff_id: string;
  run_type_id: number; // -> RunType
  load_status_id: number; // -> LoadStatus
  deadhead_miles: number;
  loaded_miles: number;
  rate: number; // post-broker
  currency: string;
  // Footprint for multi-load (shared-trailer) route optimization.
  deck_feet?: number;
  weight_lbs?: number;
  pickup_date?: string; // YYYY-MM-DD, within the dispatch week
  delivery_date?: string;
}

export interface Settlement {
  id: string;
  driver_id: string;
  load_id: string;
  gross_rate: number;
  contract_percent: number;
  driver_pay: number;
  costs_borne_by_owner: boolean;
  currency: string;
}

// --- Messaging context ---
export interface Channel {
  id: string;
  name: string;
  channel_type_id: number; // -> ChannelType (direct | group | broadcast)
  created_by: string;
  is_archived: boolean;
}

export interface Message {
  id: string;
  channel_id: string;
  author_id: string;
  reply_to_id?: string;
  body: string;
  posted_at: string;
  edited_at?: string;
}

export interface ChannelMember {
  id: string;
  channel_id: string;
  user_id: string;
  member_role_id: number;
  joined_at?: string;
  last_read_at?: string | null;
}

// --- Content (CMS) context ---
export interface Document {
  id: string;
  title: string;
  document_type_id: number; // -> DocumentType
  filename: string;
  content_type: string; // MIME type
  byte_size: number;
  checksum?: string;
  // `data` (bytea) is returned base64-encoded by JSON:API; present when a single
  // document is fetched, omitted from list views to keep payloads small.
  data?: string;
  uploaded_by: string;
}

// Link between a message and a CMS document (FK join).
export interface MessageDocument {
  id: string;
  message_id: string;
  document_id: string;
}

// Visibility a user picks when pinning a message.
export interface PinScope {
  id: number;
  code: "self" | "channel" | "everyone";
  name: string;
}

// A pinned message. `pin_scope_id` controls who sees it: self (just the pinner),
// channel (channel members), everyone (org-wide).
export interface MessagePin {
  id: string;
  message_id: string;
  channel_id: string;
  pinned_by: string;
  pin_scope_id: number; // -> PinScope
  pinned_at: string;
}

// A user's personal saved/archive entry for a message (cross-channel).
export interface SavedMessage {
  id: string;
  user_id: string;
  message_id: string;
  note?: string | null;
  saved_at: string;
}

// --- Telemetry (truck locations) ---
export interface PositionReport {
  id: string;
  equipment_id?: string;
  driver_id?: string;
  location_source_id: number; // -> LocationSource (airtag|google_device|phone_push)
  lat: number;
  lng: number;
  heading_deg?: number;
  speed_mph?: number;
  accuracy_m?: number;
  recorded_at: string;
}

// --- Navigation (trips & waypoints) ---
export interface Trip {
  id: string;
  driver_id?: string;
  equipment_id?: string;
  load_id?: string;
  trip_status_id: number; // 1 planned · 2 active · 3 completed · 4 cancelled
  name?: string;
  started_at?: string;
  ended_at?: string;
  // TRUE once the driver manually edits the stop order; the optimizer then
  // leaves this trip's order alone (geometry still recomputes).
  route_locked?: boolean;
}

export interface Waypoint {
  id: string;
  trip_id: string;
  seq: number;
  // 1 origin · 2 destination · 3 waypoint · 4 fuel · 5 rest · 6 truck_stop ·
  // 7 lunch · 8 load_stop
  stop_type_id: number;
  label?: string;
  lat: number;
  lng: number;
  planned_arrival?: string;
  arrived_at?: string;
}

// The computed GPS route for a trip (polyline/distance derived by geospatial/
// from the trip's waypoints; recomputed when waypoints change).
export interface Route {
  id: string;
  trip_id?: string;
  origin_lat?: number;
  origin_lng?: number;
  dest_lat?: number;
  dest_lng?: number;
  polyline?: string;
  distance_mi?: number;
  drive_minutes?: number;
  provider?: string;
}

// JSON:API envelope (subset).
export interface JsonApiResource<T> {
  id: string;
  type: string;
  attributes: Omit<T, "id">;
}

export interface JsonApiDocument<T> {
  data: JsonApiResource<T> | JsonApiResource<T>[];
}

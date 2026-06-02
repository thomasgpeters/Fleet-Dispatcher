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

// --- Content (CMS) context ---
export interface Document {
  id: string;
  title: string;
  document_type_id: number; // -> DocumentType
  filename: string;
  content_type: string; // MIME type
  byte_size: number;
  checksum?: string;
  // `data` (bytea) is returned base64-encoded by JSON:API; omit from list views.
  uploaded_by: string;
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

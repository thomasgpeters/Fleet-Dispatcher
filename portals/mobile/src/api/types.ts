// Domain resource types as exposed by the JSON:API (ApiLogicServer / SAFRS).
// These mirror the aggregates in docs/domain-model.md and the `domain/` package.

export type DriverType = "company" | "owner_operator";
export type RunType = "long_haul" | "regional";
export type LoadStatus =
  | "draft"
  | "dispatched"
  | "in_transit"
  | "delivered"
  | "settled"
  | "cancelled";
export type Role = "dispatcher" | "driver" | "updater";

export interface Driver {
  id: string;
  name: string;
  driver_type: DriverType;
  contract_percent: number; // derived: 30 (company) | 70 (owner-operator)
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
  run_type: RunType;
  deadhead_miles: number;
  loaded_miles: number;
  rate: number; // post-broker
  currency: string;
  status: LoadStatus;
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

// JSON:API envelope (subset).
export interface JsonApiResource<T> {
  id: string;
  type: string;
  attributes: Omit<T, "id">;
}

export interface JsonApiDocument<T> {
  data: JsonApiResource<T> | JsonApiResource<T>[];
}

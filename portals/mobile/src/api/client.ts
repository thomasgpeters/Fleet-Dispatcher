// Thin JSON:API client for the Fleet Dispatcher middleware (ApiLogicServer).
//
// Base URL comes from Vite env (VITE_API_BASE_URL, default :5659/api). All
// portals share this one API; this client speaks JSON:API conventions
// (resource collections, filter[...] query params).

import type {
  AppUser,
  Channel,
  ChannelMember,
  Document,
  Driver,
  DriverEquipment,
  JsonApiDocument,
  Load,
  Message,
  MessageDocument,
  MessagePin,
  PositionReport,
  Route,
  SavedMessage,
  Trip,
  Waypoint,
} from "./types";

export const API_BASE_URL: string =
  import.meta.env.VITE_API_BASE_URL ?? "http://localhost:5659/api";

/** pin_scope ids (match database/seed_data.sql). */
export const PIN_SCOPE = { self: 1, channel: 2, everyone: 3 } as const;

// --- Auth token (set by the auth context after login) ------------------------

let authToken: string | null = null;
let onUnauthorized: (() => void) | null = null;

/** Set/clear the bearer token sent on every request. */
export function setAuthToken(token: string | null): void {
  authToken = token;
}

/** Register a callback invoked on a 401/403 (e.g. force logout). */
export function setUnauthorizedHandler(fn: (() => void) | null): void {
  onUnauthorized = fn;
}

/** Request headers with Accept (+ optional extras) and Authorization if set. */
function authHeaders(extra: Record<string, string> = {}): Record<string, string> {
  const base: Record<string, string> = {
    Accept: "application/vnd.api+json",
    ...extra,
  };
  return authToken ? { ...base, Authorization: `Bearer ${authToken}` } : base;
}

/** Trip the unauthorized handler on a 401/403 before throwing. */
function guard(res: Response): void {
  if (res.status === 401 || res.status === 403) onUnauthorized?.();
}

async function getCollection<T>(
  resource: string,
  filters: Record<string, string> = {},
): Promise<T[]> {
  const params = new URLSearchParams();
  for (const [key, value] of Object.entries(filters)) {
    params.set(`filter[${key}]`, value);
  }
  const qs = params.toString();
  const res = await fetch(`${API_BASE_URL}/${resource}${qs ? `?${qs}` : ""}`, {
    headers: authHeaders(),
  });
  if (!res.ok) {
    guard(res);
    throw new Error(`JSON:API ${resource} request failed: ${res.status}`);
  }
  const doc = (await res.json()) as JsonApiDocument<T>;
  const data = Array.isArray(doc.data) ? doc.data : [doc.data];
  return data.map((r) => ({ id: r.id, ...r.attributes }) as T);
}

/**
 * Create a resource via JSON:API (POST). SAFRS/ApiLogicServer exposes foreign
 * keys as attributes, so we pass `*_id` columns directly in `attributes`. If the
 * API is configured to require JSON:API `relationships`, switch to that form.
 */
async function createResource<T>(
  resource: string,
  attributes: Record<string, unknown>,
): Promise<T> {
  const res = await fetch(`${API_BASE_URL}/${resource}`, {
    method: "POST",
    headers: authHeaders({ "Content-Type": "application/vnd.api+json" }),
    body: JSON.stringify({ data: { type: resource, attributes } }),
  });
  if (!res.ok) {
    guard(res);
    throw new Error(`JSON:API create ${resource} failed: ${res.status}`);
  }
  const doc = (await res.json()) as JsonApiDocument<T>;
  const r = Array.isArray(doc.data) ? doc.data[0] : doc.data;
  return { id: r.id, ...r.attributes } as T;
}

/** Fetch a single resource by id (GET /Resource/{id}), optional sparse fields. */
async function getOne<T>(
  resource: string,
  id: string,
  fields?: string[],
): Promise<T> {
  const qs = fields ? `?${new URLSearchParams({ [`fields[${resource}]`]: fields.join(",") })}` : "";
  const res = await fetch(`${API_BASE_URL}/${resource}/${id}${qs}`, {
    headers: authHeaders(),
  });
  if (!res.ok) {
    guard(res);
    throw new Error(`JSON:API ${resource}/${id} failed: ${res.status}`);
  }
  const doc = (await res.json()) as JsonApiDocument<T>;
  const r = Array.isArray(doc.data) ? doc.data[0] : doc.data;
  return { id: r.id, ...r.attributes } as T;
}

/** Update a resource via JSON:API (PATCH /Resource/{id}). */
async function updateResource<T>(
  resource: string,
  id: string,
  attributes: Record<string, unknown>,
): Promise<T> {
  const res = await fetch(`${API_BASE_URL}/${resource}/${id}`, {
    method: "PATCH",
    headers: authHeaders({ "Content-Type": "application/vnd.api+json" }),
    body: JSON.stringify({ data: { type: resource, id, attributes } }),
  });
  if (!res.ok) {
    guard(res);
    throw new Error(`JSON:API update ${resource}/${id} failed: ${res.status}`);
  }
  const doc = (await res.json()) as JsonApiDocument<T>;
  const r = Array.isArray(doc.data) ? doc.data[0] : doc.data;
  return { id: r.id, ...r.attributes } as T;
}

/** Delete a resource via JSON:API (DELETE /Resource/{id}). */
async function deleteResource(resource: string, id: string): Promise<void> {
  const res = await fetch(`${API_BASE_URL}/${resource}/${id}`, {
    method: "DELETE",
    headers: authHeaders(),
  });
  if (!res.ok) {
    guard(res);
    throw new Error(`JSON:API delete ${resource}/${id} failed: ${res.status}`);
  }
}

export const api = {
  // --- Identity & profile ---

  /** Look up a user by login username (used after auth to load the profile). */
  async getUserByUsername(username: string): Promise<AppUser | null> {
    const rows = await getCollection<AppUser>("AppUser", { username });
    return rows[0] ?? null;
  },

  getUser(userId: string): Promise<AppUser> {
    return getOne<AppUser>("AppUser", userId);
  },

  /** Update editable profile fields (PATCH AppUser). */
  updateUser(
    userId: string,
    attrs: Partial<
      Pick<
        AppUser,
        "full_name" | "email" | "phone" | "title" | "timezone" | "avatar_document_id"
      >
    >,
  ): Promise<AppUser> {
    return updateResource<AppUser>("AppUser", userId, attrs);
  },

  /** The Driver row linked to a user (for driver-role context), if any. */
  async driverForUser(userId: string): Promise<Driver | null> {
    const rows = await getCollection<Driver>("Driver", { user_id: userId });
    return rows[0] ?? null;
  },

  /** A driver's first assigned equipment id (drives the locate/telemetry tab). */
  async equipmentForDriver(driverId: string): Promise<string | null> {
    const rows = await getCollection<DriverEquipment>("DriverEquipment", {
      driver_id: driverId,
    });
    return rows[0]?.equipment_id ?? null;
  },

  /** All loads (the dispatch board). */
  listLoads(): Promise<Load[]> {
    return getCollection<Load>("Load");
  },

  /** A single load (detail page). */
  getLoad(loadId: string): Promise<Load> {
    return getOne<Load>("Load", loadId);
  },

  /** Loads a driver holds in a given dispatch week (the driver's board). */
  loadsForDriverWeek(driverId: string, dispatchWeekId: string): Promise<Load[]> {
    return getCollection<Load>("Load", {
      driver_id: driverId,
      dispatch_week_id: dispatchWeekId,
    });
  },

  activeDrivers(): Promise<Driver[]> {
    return getCollection<Driver>("Driver", { active: "true" });
  },

  /** Channels (message board: groups, direct threads, broadcasts). */
  listChannels(): Promise<Channel[]> {
    return getCollection<Channel>("Channel");
  },

  /** A single channel (detail page header). */
  getChannel(channelId: string): Promise<Channel> {
    return getOne<Channel>("Channel", channelId);
  },

  /** Messages in a channel (oldest-first; sort handled client-side for now). */
  messagesForChannel(channelId: string): Promise<Message[]> {
    return getCollection<Message>("Message", { channel_id: channelId });
  },

  /** A single message by id (used by the saved/archive view). */
  getMessage(messageId: string): Promise<Message> {
    return getOne<Message>("Message", messageId);
  },

  /** Post a new text message to a channel; pass `replyToId` to thread it under
   * another message (renders a quoted snippet in the timeline). */
  createMessage(
    channelId: string,
    authorId: string,
    body: string,
    replyToId?: string,
  ): Promise<Message> {
    return createResource<Message>("Message", {
      channel_id: channelId,
      author_id: authorId,
      body,
      ...(replyToId ? { reply_to_id: replyToId } : {}),
    });
  },

  /** The current user's membership row for a channel (for read state). */
  async myMembership(
    channelId: string,
    userId: string,
  ): Promise<ChannelMember | null> {
    const rows = await getCollection<ChannelMember>("ChannelMember", {
      channel_id: channelId,
      user_id: userId,
    });
    return rows[0] ?? null;
  },

  /** Mark a channel read for a user by stamping last_read_at = now. */
  async markChannelRead(channelId: string, userId: string): Promise<void> {
    const membership = await this.myMembership(channelId, userId);
    if (!membership) return;
    await updateResource<ChannelMember>("ChannelMember", membership.id, {
      last_read_at: new Date().toISOString(),
    });
  },

  // --- Pins ---

  /** All pins on a channel (any scope); filter for visibility client-side. */
  pinsForChannel(channelId: string): Promise<MessagePin[]> {
    return getCollection<MessagePin>("MessagePin", { channel_id: channelId });
  },

  /**
   * Pins in a channel that `userId` should see: their own `self` pins, plus all
   * `channel`- and `everyone`-scoped pins. (A production rule would enforce this
   * server-side; we filter here while auth/LogicBank are pending.)
   */
  async visiblePinsForChannel(
    channelId: string,
    userId: string,
  ): Promise<MessagePin[]> {
    const pins = await this.pinsForChannel(channelId);
    return pins.filter(
      (p) =>
        p.pin_scope_id !== PIN_SCOPE.self || p.pinned_by === userId,
    );
  },

  /** The current user's own pin on a message, if any. */
  async myPin(messageId: string, userId: string): Promise<MessagePin | null> {
    const rows = await getCollection<MessagePin>("MessagePin", {
      message_id: messageId,
      pinned_by: userId,
    });
    return rows[0] ?? null;
  },

  /** Pin a message at a chosen visibility scope. */
  pinMessage(
    messageId: string,
    channelId: string,
    userId: string,
    pinScopeId: number,
  ): Promise<MessagePin> {
    return createResource<MessagePin>("MessagePin", {
      message_id: messageId,
      channel_id: channelId,
      pinned_by: userId,
      pin_scope_id: pinScopeId,
    });
  },

  /** Change the scope of an existing pin. */
  repinMessage(pinId: string, pinScopeId: number): Promise<MessagePin> {
    return updateResource<MessagePin>("MessagePin", pinId, {
      pin_scope_id: pinScopeId,
    });
  },

  unpinMessage(pinId: string): Promise<void> {
    return deleteResource("MessagePin", pinId);
  },

  // --- Saved messages (personal archive) ---

  /** The current user's saved/archived messages (newest first by saved_at). */
  async savedForUser(userId: string): Promise<SavedMessage[]> {
    const rows = await getCollection<SavedMessage>("SavedMessage", {
      user_id: userId,
    });
    return rows.sort((a, b) => b.saved_at.localeCompare(a.saved_at));
  },

  /** The current user's saved entry for a message, if any. */
  async mySaved(messageId: string, userId: string): Promise<SavedMessage | null> {
    const rows = await getCollection<SavedMessage>("SavedMessage", {
      message_id: messageId,
      user_id: userId,
    });
    return rows[0] ?? null;
  },

  saveMessage(
    userId: string,
    messageId: string,
    note?: string,
  ): Promise<SavedMessage> {
    return createResource<SavedMessage>("SavedMessage", {
      user_id: userId,
      message_id: messageId,
      ...(note ? { note } : {}),
    });
  },

  unsaveMessage(savedId: string): Promise<void> {
    return deleteResource("SavedMessage", savedId);
  },

  // --- Content (CMS) ---

  /** Attachment links for a message (FK join rows). */
  attachmentsForMessage(messageId: string): Promise<MessageDocument[]> {
    return getCollection<MessageDocument>("MessageDocument", {
      message_id: messageId,
    });
  },

  /** Document metadata only (no `data`) — light, for list rendering. */
  getDocumentMeta(documentId: string): Promise<Document> {
    return getOne<Document>("Document", documentId, [
      "title",
      "filename",
      "content_type",
      "byte_size",
      "document_type_id",
      "uploaded_by",
    ]);
  },

  /** A single document, including its base64 `data` (for download/preview). */
  getDocument(documentId: string): Promise<Document> {
    return getOne<Document>("Document", documentId);
  },

  /** Create a CMS document from base64-encoded bytes. */
  createDocument(attrs: {
    title: string;
    document_type_id: number;
    filename: string;
    content_type: string;
    byte_size: number;
    data: string; // base64 (maps to bytea)
    uploaded_by: string;
  }): Promise<Document> {
    return createResource<Document>("Document", attrs);
  },

  /** Link an existing document to a message. */
  linkMessageDocument(
    messageId: string,
    documentId: string,
  ): Promise<MessageDocument> {
    return createResource<MessageDocument>("MessageDocument", {
      message_id: messageId,
      document_id: documentId,
    });
  },

  // --- Telemetry ---

  /** Push a position report (truck location). */
  postPosition(attrs: {
    location_source_id: number;
    lat: number;
    lng: number;
    equipment_id?: string;
    driver_id?: string;
    heading_deg?: number;
    speed_mph?: number;
    accuracy_m?: number;
  }): Promise<PositionReport> {
    return createResource<PositionReport>("PositionReport", attrs);
  },

  // --- Navigation ---

  /** Trips for a driver (the driver's trip list). */
  tripsForDriver(driverId: string): Promise<Trip[]> {
    return getCollection<Trip>("Trip", { driver_id: driverId });
  },

  getTrip(tripId: string): Promise<Trip> {
    return getOne<Trip>("Trip", tripId);
  },

  createTrip(attrs: {
    driver_id: string;
    equipment_id?: string;
    load_id?: string;
    trip_status_id: number;
    name?: string;
  }): Promise<Trip> {
    return createResource<Trip>("Trip", attrs);
  },

  /** Update trip fields (status, name, route_locked, …). */
  updateTrip(
    id: string,
    attrs: Partial<Pick<Trip, "trip_status_id" | "name" | "route_locked">>,
  ): Promise<Trip> {
    return updateResource<Trip>("Trip", id, attrs);
  },

  waypointsForTrip(tripId: string): Promise<Waypoint[]> {
    return getCollection<Waypoint>("Waypoint", { trip_id: tripId });
  },

  addWaypoint(attrs: {
    trip_id: string;
    seq: number;
    stop_type_id: number;
    lat: number;
    lng: number;
    label?: string;
  }): Promise<Waypoint> {
    return createResource<Waypoint>("Waypoint", attrs);
  },

  /** Update a waypoint (re-label, re-sequence, or mark arrived). */
  updateWaypoint(
    id: string,
    attrs: Partial<Pick<Waypoint, "seq" | "stop_type_id" | "label" | "arrived_at">>,
  ): Promise<Waypoint> {
    return updateResource<Waypoint>("Waypoint", id, attrs);
  },

  /** Remove a waypoint from a trip's route. */
  deleteWaypoint(id: string): Promise<void> {
    return deleteResource("Waypoint", id);
  },

  /** The computed route for a trip, if one exists. */
  async routeForTrip(tripId: string): Promise<Route | null> {
    const rows = await getCollection<Route>("Route", { trip_id: tripId });
    return rows[0] ?? null;
  },
};

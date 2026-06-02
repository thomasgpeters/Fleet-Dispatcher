// Thin JSON:API client for the Fleet Dispatcher middleware (ApiLogicServer).
//
// Base URL comes from Vite env (VITE_API_BASE_URL, default :5656/api). All
// portals share this one API; this client speaks JSON:API conventions
// (resource collections, filter[...] query params).

import type { Channel, Driver, JsonApiDocument, Load, Message } from "./types";

export const API_BASE_URL: string =
  import.meta.env.VITE_API_BASE_URL ?? "http://localhost:5656/api";

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
    headers: { Accept: "application/vnd.api+json" },
  });
  if (!res.ok) {
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
    headers: {
      Accept: "application/vnd.api+json",
      "Content-Type": "application/vnd.api+json",
    },
    body: JSON.stringify({ data: { type: resource, attributes } }),
  });
  if (!res.ok) {
    throw new Error(`JSON:API create ${resource} failed: ${res.status}`);
  }
  const doc = (await res.json()) as JsonApiDocument<T>;
  const r = Array.isArray(doc.data) ? doc.data[0] : doc.data;
  return { id: r.id, ...r.attributes } as T;
}

export const api = {
  /** All loads (the dispatch board). */
  listLoads(): Promise<Load[]> {
    return getCollection<Load>("Load");
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

  /** Messages in a channel (oldest-first; sort handled client-side for now). */
  messagesForChannel(channelId: string): Promise<Message[]> {
    return getCollection<Message>("Message", { channel_id: channelId });
  },

  /** Post a new text message to a channel. */
  createMessage(
    channelId: string,
    authorId: string,
    body: string,
  ): Promise<Message> {
    return createResource<Message>("Message", {
      channel_id: channelId,
      author_id: authorId,
      body,
    });
  },
};

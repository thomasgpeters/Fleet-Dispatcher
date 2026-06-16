// Client for the "Hey Dispatch" assistant service (FastAPI + Claude tool use).
//
// Base URL comes from Vite env (VITE_ASSISTANT_BASE_URL, default :5710). Separate
// from the JSON:API base — this is the voice-assistant service, not ALS.

export const ASSISTANT_BASE_URL: string =
  import.meta.env.VITE_ASSISTANT_BASE_URL ?? "http://localhost:5710";

/** One push-to-talk turn. `transcript` is on-device STT; the rest is the
 * driver's trusted context the app already holds (never the model's to pick). */
export interface AssistRequest {
  transcript: string;
  driver_id?: string;
  driver_name?: string;
  author_user_id?: string;
  dispatcher_channel_id?: string;
  lat?: number;
  lng?: number;
  dest_lat?: number;
  dest_lng?: number;
  dest_label?: string;
  truck_height_m?: number;
  truck_weight_kg?: number;
}

export interface AssistAction {
  tool: string;
  ok: boolean;
  detail: string;
}

export interface AssistResponse {
  reply: string;
  actions: AssistAction[];
  cache_read_tokens: number;
  stopped: string | null;
}

/** Send a transcribed utterance to the assistant; get back a spoken reply. */
export async function assist(req: AssistRequest): Promise<AssistResponse> {
  const res = await fetch(`${ASSISTANT_BASE_URL}/assist`, {
    method: "POST",
    headers: { "Content-Type": "application/json", Accept: "application/json" },
    body: JSON.stringify(req),
  });
  if (!res.ok) {
    throw new Error(`Assistant request failed: ${res.status}`);
  }
  return (await res.json()) as AssistResponse;
}

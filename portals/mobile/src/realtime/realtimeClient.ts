// Robust WebSocket client for the realtime bridge (realtime/ service).
//
// Design for non-fragility:
//   * auto-reconnect with exponential backoff + jitter (capped),
//   * remembers desired topics and re-subscribes on every (re)connect,
//   * surfaces status so the UI can show a live/offline hint,
//   * it is an accelerator only — pages keep their normal fetch/reconcile, so if
//     the bridge is down the app still works (just less instantly).

export interface RealtimeEvent {
  type: string; // "message" | "position" | ...
  [key: string]: unknown;
}

export type RealtimeStatus = "connecting" | "open" | "closed";
type EventListener = (evt: RealtimeEvent) => void;
type StatusListener = (s: RealtimeStatus) => void;

const MAX_BACKOFF_MS = 30_000;

export class RealtimeClient {
  private ws: WebSocket | null = null;
  private readonly url: string;
  private readonly token: string;
  private readonly topics = new Set<string>();
  private readonly listeners = new Set<EventListener>();
  private readonly statusListeners = new Set<StatusListener>();
  private shouldRun = false;
  private backoff = 1000;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  status: RealtimeStatus = "closed";

  constructor(url: string, token: string) {
    this.url = url;
    this.token = token;
  }

  start(): void {
    this.shouldRun = true;
    this.open();
  }

  stop(): void {
    this.shouldRun = false;
    if (this.reconnectTimer) clearTimeout(this.reconnectTimer);
    this.reconnectTimer = null;
    if (this.ws) {
      this.ws.onclose = null; // don't trigger reconnect on an intentional close
      try {
        this.ws.close();
      } catch {
        /* ignore */
      }
      this.ws = null;
    }
    this.setStatus("closed");
  }

  /** Add topics; (re)sends the subscription if connected. Safe before connect. */
  subscribe(topics: string[]): void {
    let changed = false;
    for (const t of topics) {
      if (!this.topics.has(t)) {
        this.topics.add(t);
        changed = true;
      }
    }
    if (changed) this.sendSubscribe();
  }

  addListener(fn: EventListener): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  onStatus(fn: StatusListener): () => void {
    this.statusListeners.add(fn);
    return () => this.statusListeners.delete(fn);
  }

  private setStatus(s: RealtimeStatus): void {
    this.status = s;
    this.statusListeners.forEach((f) => f(s));
  }

  private sendSubscribe(): void {
    if (this.ws?.readyState === WebSocket.OPEN && this.topics.size > 0) {
      this.ws.send(JSON.stringify({ action: "subscribe", topics: [...this.topics] }));
    }
  }

  private open(): void {
    if (!this.shouldRun) return;
    this.setStatus("connecting");
    const sep = this.url.includes("?") ? "&" : "?";
    const ws = new WebSocket(`${this.url}${sep}token=${encodeURIComponent(this.token)}`);
    this.ws = ws;

    ws.onopen = () => {
      this.backoff = 1000; // reset backoff on a good connection
      this.setStatus("open");
      this.sendSubscribe(); // re-assert topics after a (re)connect
    };
    ws.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data as string);
        if (msg?.type === "event" && msg.event) {
          this.listeners.forEach((f) => f(msg.event as RealtimeEvent));
        }
      } catch {
        /* ignore malformed frames */
      }
    };
    ws.onclose = () => {
      this.ws = null;
      this.setStatus("closed");
      this.scheduleReconnect();
    };
    ws.onerror = () => {
      try {
        ws.close();
      } catch {
        /* onclose will handle reconnect */
      }
    };
  }

  private scheduleReconnect(): void {
    if (!this.shouldRun) return;
    const jitter = Math.random() * 300;
    const delay = Math.min(this.backoff, MAX_BACKOFF_MS) + jitter;
    this.reconnectTimer = setTimeout(() => {
      if (this.shouldRun) this.open();
    }, delay);
    this.backoff = Math.min(this.backoff * 2, MAX_BACKOFF_MS);
  }
}

export const REALTIME_URL: string =
  import.meta.env.VITE_REALTIME_URL ?? "ws://localhost:8765";

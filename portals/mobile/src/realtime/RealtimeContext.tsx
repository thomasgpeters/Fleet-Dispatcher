import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
} from "react";
import type { ReactNode } from "react";

import { useAuth } from "../auth/AuthContext";
import {
  RealtimeClient,
  REALTIME_URL,
  type RealtimeEvent,
  type RealtimeStatus,
} from "./realtimeClient";

interface RealtimeContextValue {
  status: RealtimeStatus;
  subscribe: (topics: string[]) => void;
  addListener: (fn: (evt: RealtimeEvent) => void) => () => void;
}

const RealtimeContext = createContext<RealtimeContextValue | null>(null);

/**
 * Connects to the realtime bridge when authenticated. The client is created
 * during render (useMemo) so child effects can subscribe/listen before the
 * provider's own effect starts the socket. Tearing down on logout/token change.
 */
export function RealtimeProvider({ children }: { children: ReactNode }) {
  const { token } = useAuth();
  const [status, setStatus] = useState<RealtimeStatus>("closed");

  const client = useMemo(
    () => (token ? new RealtimeClient(REALTIME_URL, token) : null),
    [token],
  );

  useEffect(() => {
    if (!client) return;
    const off = client.onStatus(setStatus);
    client.start();
    return () => {
      off();
      client.stop();
    };
  }, [client]);

  const subscribe = useCallback(
    (topics: string[]) => client?.subscribe(topics),
    [client],
  );
  const addListener = useCallback(
    (fn: (evt: RealtimeEvent) => void) =>
      client ? client.addListener(fn) : () => undefined,
    [client],
  );

  const value = useMemo<RealtimeContextValue>(
    () => ({ status, subscribe, addListener }),
    [status, subscribe, addListener],
  );

  return <RealtimeContext.Provider value={value}>{children}</RealtimeContext.Provider>;
}

export function useRealtime(): RealtimeContextValue {
  const ctx = useContext(RealtimeContext);
  if (!ctx) throw new Error("useRealtime must be used within <RealtimeProvider>");
  return ctx;
}

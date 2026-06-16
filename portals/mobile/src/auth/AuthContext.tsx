import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
} from "react";
import type { ReactNode } from "react";

import { api, setAuthToken, setUnauthorizedHandler } from "../api/client";
import { login as apiLogin } from "../api/auth";
import type { AppUser } from "../api/types";

const TOKEN_KEY = "fleet.auth.token";
const USERNAME_KEY = "fleet.auth.username";

interface AuthState {
  ready: boolean; // initial token check done
  token: string | null;
  user: AppUser | null;
  driverId: string | null; // resolved Driver linked to this user (driver role)
  equipmentId: string | null; // first equipment assigned to that driver
  login: (username: string, password: string) => Promise<void>;
  logout: () => void;
  refreshProfile: () => Promise<void>;
}

const AuthContext = createContext<AuthState | null>(null);

/** Load the user record + driver/equipment context for a signed-in username. */
async function loadContext(username: string): Promise<{
  user: AppUser | null;
  driverId: string | null;
  equipmentId: string | null;
}> {
  const user = await api.getUserByUsername(username);
  let driverId: string | null = null;
  let equipmentId: string | null = null;
  if (user) {
    const driver = await api.driverForUser(user.id).catch(() => null);
    driverId = driver?.id ?? null;
    if (driverId) {
      equipmentId = await api.equipmentForDriver(driverId).catch(() => null);
    }
  }
  return { user, driverId, equipmentId };
}

export function AuthProvider({ children }: { children: ReactNode }) {
  const [ready, setReady] = useState(false);
  const [token, setToken] = useState<string | null>(null);
  const [user, setUser] = useState<AppUser | null>(null);
  const [driverId, setDriverId] = useState<string | null>(null);
  const [equipmentId, setEquipmentId] = useState<string | null>(null);

  const logout = useCallback(() => {
    localStorage.removeItem(TOKEN_KEY);
    localStorage.removeItem(USERNAME_KEY);
    setAuthToken(null);
    setToken(null);
    setUser(null);
    setDriverId(null);
    setEquipmentId(null);
  }, []);

  // Force logout on any 401/403 from the API.
  useEffect(() => {
    setUnauthorizedHandler(logout);
    return () => setUnauthorizedHandler(null);
  }, [logout]);

  // Restore a persisted session on boot.
  useEffect(() => {
    const saved = localStorage.getItem(TOKEN_KEY);
    const savedUser = localStorage.getItem(USERNAME_KEY);
    if (!saved || !savedUser) {
      setReady(true);
      return;
    }
    setAuthToken(saved);
    setToken(saved);
    loadContext(savedUser)
      .then(({ user, driverId, equipmentId }) => {
        if (!user) {
          logout(); // token present but user not found — clear it
          return;
        }
        setUser(user);
        setDriverId(driverId);
        setEquipmentId(equipmentId);
      })
      .catch(() => logout())
      .finally(() => setReady(true));
  }, [logout]);

  const login = useCallback(async (username: string, password: string) => {
    const { token } = await apiLogin(username, password);
    setAuthToken(token);
    localStorage.setItem(TOKEN_KEY, token);
    localStorage.setItem(USERNAME_KEY, username);
    const ctx = await loadContext(username);
    setToken(token);
    setUser(ctx.user);
    setDriverId(ctx.driverId);
    setEquipmentId(ctx.equipmentId);
  }, []);

  const refreshProfile = useCallback(async () => {
    if (!user) return;
    const fresh = await api.getUser(user.id);
    setUser(fresh);
  }, [user]);

  const value = useMemo<AuthState>(
    () => ({ ready, token, user, driverId, equipmentId, login, logout, refreshProfile }),
    [ready, token, user, driverId, equipmentId, login, logout, refreshProfile],
  );

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth(): AuthState {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error("useAuth must be used within <AuthProvider>");
  return ctx;
}

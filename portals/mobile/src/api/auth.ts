// Login against ApiLogicServer's built-in JWT auth.
//
// ALS exposes `POST {API_BASE_URL}/auth/login` taking {username, password} and
// returning an access token. We tolerate a couple of common field names for the
// token so this works across ALS auth configurations.

import { API_BASE_URL } from "./client";

export interface LoginResult {
  token: string;
}

export async function login(
  username: string,
  password: string,
): Promise<LoginResult> {
  const res = await fetch(`${API_BASE_URL}/auth/login`, {
    method: "POST",
    headers: { "Content-Type": "application/json", Accept: "application/json" },
    body: JSON.stringify({ username, password }),
  });
  if (!res.ok) {
    if (res.status === 401 || res.status === 400) {
      throw new Error("Incorrect username or password.");
    }
    throw new Error(`Login failed: ${res.status}`);
  }
  const data = (await res.json()) as Record<string, unknown>;
  const token =
    (data.access_token as string) ??
    (data.token as string) ??
    (data.accessToken as string);
  if (!token) {
    throw new Error("Login response did not include a token.");
  }
  return { token };
}

// Appearance preference for the mobile app: follow the OS, or force light/dark.
// The choice persists in localStorage and is applied as a class on <html>
// (.fd-light / .fd-dark) which our theme variables react to (see variables.css).

export type ThemePref = "system" | "light" | "dark";

const KEY = "fd.theme";

export function getThemePref(): ThemePref {
  const v = localStorage.getItem(KEY);
  return v === "light" || v === "dark" ? v : "system";
}

export function applyTheme(pref: ThemePref): void {
  const root = document.documentElement.classList;
  root.remove("fd-light", "fd-dark");
  if (pref === "light") root.add("fd-light");
  else if (pref === "dark") root.add("fd-dark");
  // "system" => no class; the prefers-color-scheme media query decides.
}

export function setThemePref(pref: ThemePref): void {
  if (pref === "system") localStorage.removeItem(KEY);
  else localStorage.setItem(KEY, pref);
  applyTheme(pref);
}

/** Apply the saved preference at startup (call once before render). */
export function initTheme(): void {
  applyTheme(getThemePref());
}

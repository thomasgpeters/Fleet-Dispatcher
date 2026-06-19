# Design system — Fleet Dispatcher

One visual language across all three clients: the **mobile** app (Ionic/React),
the dispatcher **desktop** console (Wt), and the **HUD** (Wt). The medium they
share is CSS, so the palette lives here as the source of truth and is implemented
as design tokens in each client:

- Desktop + HUD: `portals/dispatcher-desktop/docroot/css/fleet-dispatcher.css`
  (`--fd-*` custom properties).
- Mobile: `portals/mobile/src/theme/variables.css` (Ionic `--ion-*` variables
  derived from the same palette).

## Palette

Subtle blues, used generously; **white/light** surfaces for the standard theme;
**orange** used sparingly as the highlight/accent. A complementary **dark** theme
mirrors every token.

| Token (role) | Light | Dark |
| --- | --- | --- |
| Page background (light grey) | `#eceef2` | `#0f1722` |
| Surface / panel (bright white) | `#ffffff` | `#16212e` |
| Surface 2 (chips, bubbles) | `#f3f4f6` | `#1b2937` |
| Text | `#1b2430` | `#e6edf4` |
| Muted text | `#5f6a78` | `#9fb1c2` |
| Border (rarely used) | `#dcdfe5` | `#29384a` |
| **Primary blue** | `#2c6fb3` | `#4a90d9` |
| Primary strong | `#1f4e79` | `#6aa6e0` |
| **Accent orange** | `#e8833a` | `#f0954e` |
| Header (nav) bg | `#1f4e79` | `#0d1925` |
| Link | `#2c6fb3` | `#6aa6e0` |

Use orange only for highlights: the load-card accent bar, toast status bars,
small status/CTA emphasis. Keep semantic colors (success/danger/warning) at their
framework defaults.

## Typography & panels

- **Font:** **Barlow Semi Condensed** — a professional, slightly-compressed UI
  face that packs well on laptop screens. Stack falls back to Inter / Segoe UI /
  system. Loaded from Google Fonts (`@import` in the desktop CSS; `<link>` in the
  mobile `index.html`); self-host for offline. Base size 15px, dense-but-legible.
- **Panels:** **rounded** (`--fd-radius: 12px`) surfaces on the **light-grey
  page** — separated by spacing, **not** borders or shadows. Surfaces are bright
  **white** in the **light** theme only; in **dark** they use the dark surface
  token (never force white). House rules: **no shadows** (inner or outer); **no
  borders** unless ≤ 0.25px hairline; **generous padding**. Accent *bars* (load
  card, toast status) are status indicators, not panel borders. On mobile the
  same look is achieved the Ionic way (`ion-card`/`ion-item` background bound to
  the theme-aware `--ion-card-background`/`--ion-item-background`, shadow removed,
  rounded) while keeping list-separator hairlines, a mobile convention.
- **Data tables** live inside a rounded surface container (desktop:
  `.fd-tablecard`; the container carries the background + radius, `overflow:hidden`
  clips rows to the corners). No cell borders; header band on surface-2; faint
  zebra rows.
- **Buttons:** **pill** shape (`border-radius: 999px`). **Active** toggle =
  filled bright **blue** (`--fd-primary`); **warning** = filled **orange**
  (`--fd-accent`), not framework yellow. Mobile mirrors this (`ion-button`
  pill radius; `--ion-color-warning` remapped to the orange accent).

## Theming mechanism

- **Default = follow the OS** via `@media (prefers-color-scheme: dark)`.
- **Manual override** wins over the OS:
  - Desktop/HUD: `data-fd-theme="light|dark"` on `<html>` (toggle in the header;
    persisted in `localStorage` as `fd-theme`). The HUD forces `dark` (wall
    display).
  - Mobile: `.fd-dark` / `.fd-light` class on `<html>` (Appearance picker in
    Profile; persisted as `fd.theme`).

## Desktop layout

The console uses an app frame: **full-width header** (brand + navigation + theme
toggle + user/role + sign out), a **three-column body** — collapsible **left
panel** (filters/context), **center work panel** (active view), collapsible
**right panel** (details/inspector) — and a **full-width footer** (copyright +
links). Panels hide/show via header toggles (`.fd-collapsed`) and stack under the
center column on narrow viewports.

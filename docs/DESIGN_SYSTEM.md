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
| Page background | `#f4f7fb` | `#0f1722` |
| Surface (cards/panels) | `#ffffff` | `#16212e` |
| Surface 2 (side panels) | `#eef3f9` | `#1b2937` |
| Text | `#18222e` | `#e6edf4` |
| Muted text | `#5b6b7b` | `#9fb1c2` |
| Border | `#d8e1ec` | `#29384a` |
| **Primary blue** | `#2c6fb3` | `#4a90d9` |
| Primary strong | `#1f4e79` | `#6aa6e0` |
| **Accent orange** | `#e8833a` | `#f0954e` |
| Header (nav) bg | `#1f4e79` | `#0d1925` |
| Footer bg | `#eef3f9` | `#0d1925` |
| Link | `#2c6fb3` | `#6aa6e0` |

Use orange only for highlights: active-nav underline, the load-card accent bar,
small status/CTA emphasis. Keep semantic colors (success/danger/warning) at their
framework defaults.

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

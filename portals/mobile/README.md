# Fleet Dispatcher — Mobile

The mobile client for the **Driver** and **Updater** roles.

**Stack:** React 18 · Ionic 8 · Capacitor 6 · TypeScript 5.5 · Vite 5.

This is a **self-contained module**. It lives in the Fleet Dispatcher monorepo
under `portals/mobile/` during development, and is also published on its own to
the **`Fleet-Dispatcher-Mobile`** repository, from which VCP builds and deploys
it. Nothing here depends on files outside this folder.

It talks to the shared middleware over **JSON:API** (ApiLogicServer / SAFRS) at
`VITE_API_BASE_URL` (default `http://localhost:5656/api`) and never touches
PostgreSQL directly.

## What this portal does

- **Driver:** view this week's assigned loads, take/decline loads, mark
  in-transit / delivered, record post-trip inspection, log fuel/DEF/oil, manage
  own schedule (ELD-adjacent).
- **Updater:** relay status and messages between drivers and dispatchers.

## Develop

```bash
npm install
cp .env.example .env.local      # point VITE_API_BASE_URL at your middleware
npm run dev                     # Vite dev server on http://localhost:5173
```

## Build

```bash
npm run build                   # type-checks (tsc) then builds to dist/
npm run preview                 # serve the production build locally
```

`dist/` is a static SPA — the artifact VCP serves in its managed container.

Native shells (Capacitor):

```bash
npx cap add ios
npx cap add android
npm run cap:sync
```

## Layout

```
index.html                Vite entry
src/
  main.tsx                Ionic React bootstrap
  App.tsx                 app shell — tabbed nav; each tab is a list → detail stack
  pages/
    LoadsPage.tsx         loads list (taps push the detail)
    LoadDetailPage.tsx    load detail (back button)
    ChannelsPage.tsx      message board — channel list
    ChannelPage.tsx       channel detail — messages, attachments, composer + upload
  api/
    client.ts             thin JSON:API client (VITE_API_BASE_URL)
    types.ts              resource types mirroring the database schema
capacitor.config.ts       Capacitor app id / web dir
```

Navigation is **list → detail** with native transitions and `IonBackButton`
(KISS). See [`../../docs/MOBILE_UI_WIDGETS.md`](../../docs/MOBILE_UI_WIDGETS.md)
for the UX widget vocabulary.

## JSON:API integration

Resources map 1:1 to the domain objects exposed by ApiLogicServer (`Driver`,
`Load`, `DispatchWeek`, `Settlement`, …); lookup tables (`DriverType`,
`RunType`, `LoadStatus`, …) are referenced by integer `*_id` and can be pulled
in with JSON:API `include`. Example — a driver's loads for a week:

```
GET /api/Load?filter[driver_id]=<id>&filter[dispatch_week_id]=<id>
```

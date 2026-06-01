# Mobile portal — Drivers & Updaters

The mobile client for the **Driver** and **Updater** roles.

**Stack:** React 18 · Ionic 8 · Capacitor 6 · TypeScript 5.5 · Vite 5.

It talks to the shared middleware over **JSON:API** (ApiLogicServer / SAFRS) at
`VITE_API_BASE_URL` (default `http://localhost:5656/api`). It never touches
PostgreSQL directly.

## What this portal does

- **Driver:** view this week's assigned loads, take/decline loads, mark
  in-transit / delivered, record post-trip inspection, log fuel/DEF/oil, manage
  own schedule (ELD-adjacent).
- **Updater:** relay status and messages between drivers and dispatchers.

## Develop

```bash
cd portals/mobile
npm install
cp ../../.env.example .env.local   # ensure VITE_API_BASE_URL points at the API
npm run dev                        # Vite dev server
```

Native shells (after `npm run build`):

```bash
npx cap add ios
npx cap add android
npm run cap:sync
```

## JSON:API integration

A thin typed client lives in [`src/api/`](src/api). JSON:API resources map 1:1
to the domain aggregates exposed by ApiLogicServer (`Driver`, `Load`,
`DispatchWeek`, `Settlement`, …). Example: a driver's loads for the current week

```
GET /api/Load?filter[driver_id]=<id>&filter[dispatch_week_id]=<id>
```

> This directory is a scaffold: `package.json`, `tsconfig`, `vite.config.ts`,
> and a starter API client. Run `npm install` to materialize `node_modules`,
> then build out the Ionic pages.

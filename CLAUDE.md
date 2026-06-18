# Fleet Dispatcher — agent brief

Trucking dispatch platform: a PostgreSQL schema is the **source of truth**, an
**ApiLogicServer (ALS)** middleware reflects it into a JSON:API, and multiple
clients consume that one API. See `README.md` and `docs/` for depth — this file
is the orientation an agent needs first.

## Architecture (monorepo)

| Path | What | Stack |
| --- | --- | --- |
| `database/` | `schema.sql` (source of truth) + `seed_data.sql` | PostgreSQL 16 |
| middleware | ApiLogicServer reflects the schema → JSON:API (`:5659/api`) | Python/ALS, **generated outside this repo** |
| `portals/mobile/` | Driver/dispatcher app | Ionic + React + TypeScript + Vite |
| `portals/dispatcher-desktop/` | Dispatcher console + HUD | C++ / Wt |
| `assistant/` | "Hey Dispatch" voice assistant service | Python |
| `realtime/` | WebSocket bridge: ALS→Kafka events → clients | Python |
| `als-extensions/` | ALS customizations to (re)install post-generate (Kafka producers) | Python |
| `geospatial/` | Spatial helpers | Python |
| `docs/` | Domain model, architecture, per-feature docs | Markdown |

All clients talk **only** to the JSON:API — never directly to PostgreSQL.

## Golden rules (this project's conventions)

1. **The schema drives everything.** Express every relationship as a real
   `FOREIGN KEY` so ALS generates the resource relationships. Lookup/enum tables
   use **sequential INTEGER** identity keys; domain objects use **UUID** keys.
   Fleet lives in its own `fleet` schema (not `public`).
2. **Don't write middleware Python.** ALS generates the API + LogicBank rules
   from the schema. Adding a redundant service is wrong unless explicitly asked.
   A schema change means **ALS must be regenerated** — say so. After a fresh ALS
   generate, re-install our customizations with `make als-extensions` (the Kafka
   event producers in `als-extensions/`; rebuilds preserve them).
3. **Verify DB changes on a throwaway Postgres before committing.** Apply
   `schema.sql` + `seed_data.sql` to a fresh PG16 cluster and check it's clean.
   Use the `/verify-db` command. Never test against a persistent database.
4. **The Wt desktop is NOT compiled in this sandbox** (no Wt). Write it
   carefully and **flag version-sensitive spots** in comments (e.g. the
   `Http::Client::done()` `error_code` type, `Json::Type::Null`,
   `Http::Method::Patch`). Ask the user to build on their Linux box.
5. **Check the mobile build** after changing `portals/mobile`:
   `cd portals/mobile && npm run build` (runs `tsc && vite build`). Keep it clean
   (no unused vars — `tsc` is strict).
6. **KISS + DDD.** Match the surrounding code's altitude, naming, and comment
   density. Aggregates/bounded contexts are documented in `docs/domain-model.md`.
7. **Keep docs in sync.** Update `docs/DEVELOPMENT_LOG.md` and `docs/TODO.md`
   with every feature; update `docs/domain-model.md` when the model changes.

## Auth & identity

ApiLogicServer built-in **JWT** auth verifies against `app_user`
(`password_hash` = werkzeug `pbkdf2:sha256`). Mobile uses `useAuth()`
(`src/auth/AuthContext.tsx`); desktop uses `LoginView`/`ProfileView`. Clients
send `Authorization: Bearer`. Demo users: `dispatch1`/`driver1`/`updater1`,
password `fleet123`. Full setup in `docs/AUTHENTICATION.md`.

## Workflow

- **Develop on the designated feature branch**; never push to the default branch
  without explicit permission.
- Commit with clear messages; run `/verify-db` (schema/seed) and the mobile
  build (mobile) before committing those areas.
- Push with `git push -u origin <branch>` (retry on network errors).
- Only open a PR when the user asks.

## Common commands

```bash
# Mobile build / lint
cd portals/mobile && npm run build
cd portals/mobile && npm run lint

# DB verification (throwaway PG16) — prefer the /verify-db command
# Local DB standup (real DB): scripts/db-setup.sh   (see docs/DEPLOYMENT.md)

# Mobile subtree publish to the standalone repo
make publish-mobile
```

## Key docs

- `docs/domain-model.md` — entities, aggregates, bounded contexts, lookups
- `docs/architecture.md` — system architecture (+ `architecture.svg`)
- `docs/DESIGN_SYSTEM.md` — shared palette/theme (mobile + desktop + HUD), layout
- `docs/REALTIME.md` — realtime WebSocket bridge over ALS→Kafka (topic strategy)
- `docs/AUTHENTICATION.md` — ALS JWT auth + profiles
- `docs/MIDDLEWARE_SETUP.md` — generating/running ALS
- `docs/DISPATCHER_DESKTOP.md` — the Wt console + HUD
- `docs/DEPLOYMENT.md` — DB + service standup
- `docs/TODO.md` / `docs/DEVELOPMENT_LOG.md` — live status + history

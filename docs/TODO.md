# Fleet Dispatcher — TODO / Plan of approach

Working plan, sequenced. Check items off as they ship and add a dated entry to
[`DEVELOPMENT_LOG.md`](DEVELOPMENT_LOG.md). `[~]` = in progress, `[ ]` = queued,
`[x]` = done.

## Conventions (how we work)

- Domain language lives in [`domain-model.md`](domain-model.md); the schema
  (`database/schema.sql`) is the physical model ALS generates the API from.
- Lookup tables = sequential integer keys; domain objects = UUID/GUID keys;
  relationships = real FK constraints (ALS derives JSON:API relationships).
- Middleware is generated/run outside this repo (see
  [`MIDDLEWARE_SETUP.md`](MIDDLEWARE_SETUP.md)).
- Mobile is published to `Fleet-Dispatcher-Mobile` via `make publish-mobile`.
- Validate schema/seed against PostgreSQL 16; build the mobile app (`tsc` +
  Vite) before every push.
- **Desktop C++/Wt is not compiled in the dev sandbox (no Wt); it is built and
  run on the Linux box.** Standing decision — version-sensitive lines are flagged
  inline; no per-change "not compiled" disclaimer.

## Foundation  ✅

- [x] Three-tier scaffold, docs, architecture (+ SVG)
- [x] Schema: lookup tables + GUID domain keys + FK relationships
- [x] Desktop (C++/Wt) Bootstrap theme + full Wt `resources/` deployment
- [x] Mobile: self-contained, buildable TS app (React/Ionic/Capacitor/Vite)
- [x] Mobile publishing via git subtree (`make publish-mobile`, CMake target)
- [x] Middleware setup offloaded to `docs/MIDDLEWARE_SETUP.md`

## Feature 1 — Messaging & Content (CMS)  — Phase 1 CLOSED (2026-06-02)

Mobile messaging + CMS vertical is complete for what doesn't depend on auth /
realtime / the desktop portal. Those are carried over below to their owners.

Done:
- [x] Schema: `channel`, `channel_member`, `message`; CMS `document` + FK link
      tables (`message_document`, `load_document`)
- [x] Mobile: list → detail navigation (channels list + channel detail) with
      native transitions and back buttons (KISS); same pattern for Loads
- [x] Mobile: compose & send a message (write-side)
- [x] Mobile: attachment upload (create `document` + `message_document` link)
- [x] Mobile: attachment preview / download (bytea ↔ base64, data URL)
- [x] Optimize attachment loading (sparse fieldsets; lazy `data` on tap)
- [x] Unread counts via `channel_member.last_read_at` + mark-as-read (Badge)
- [x] Pull-to-refresh (`IonRefresher`) on channel list + detail

Phase 2 — pins & personal archive (2026-06-11):
- [x] Schema: `pin_scope` lookup + `message_pin` (user-selectable scope:
      self/channel/everyone) + `saved_message` (per-user archive, optional note).
      Verified on PostgreSQL 16 (40 tables in `fleet`).
- [x] Mobile: pin a message with a scope picker; visible-pins strip atop the
      channel; swipe to pin/unpin or save/unsave; inline pin/saved markers
- [x] Mobile: personal **Saved** archive view (cross-channel), reached from the
      Message Board header; remove items
- [x] **Emoji** in the composer: dependency-free picker (trucking-relevant set);
      storage/transport already UTF-8 (verified round-trip), so no schema change
- [x] **Reply to message**: swipe → Reply, composer quote banner, threaded quote
      snippet in the timeline (uses existing `message.reply_to_id`; no schema change)
- [ ] Emoji in the desktop messaging view (Wt WText/WLineEdit are UTF-8 native) →
      lands with that view
- [ ] Server-side pin-visibility enforcement (currently filtered client-side) →
      auth / LogicBank
- [ ] Desktop (dispatcher) pins & saved views → desktop portal work

Carried over (blocked / owned elsewhere):
- [ ] Direct (1:1) channel creation + membership management — needs a user
      directory (→ auth)
- [x] Current-user identity — replaced `src/currentUser.ts` placeholder with the
      authenticated user via `useAuth()` (Feature: Authentication)
- [ ] Clickable message **toasts** (`IonToast`, 2–3s, deep-link) → realtime
- [ ] Realtime delivery (websockets/push) — currently polling/pull-to-refresh
- [ ] Desktop (dispatcher) messaging view → desktop portal work
- [ ] Server-side `unread_count` (LogicBank) to replace client-side computation
- [ ] More CMS links as needed: `driver_document`, `equipment_document`,
      `inspection_document`

## Dispatcher Desktop Portal (C++/Wt)  — NEXT

Pivot here after Phase 1. Talks to the same JSON:API; Bootstrap theme + Wt
`resources/` deploy already wired. Design: [`DISPATCHER_DESKTOP.md`](DISPATCHER_DESKTOP.md).

- [x] Schema: `load.pickup_date` / `delivery_date` (drives Today vs Week)
- [x] **Shell**: app bar (API/health), nav (Board/Loads/Drivers/Messages),
      command bar, content outlet; owns the JSON:API client + mode state
      (not compiled in-sandbox — no Wt; see note below)
- [x] **Board** with a **Today | Week** toggle (today's runs vs Mon→Mon grid),
      fed by an async `ApiClient` (Wt::Http::Client + Wt::Json)
- [x] App-bar **user + role + Sign out** (auth); console gated by a Wt login
- [x] **App frame + design system** ([`DESIGN_SYSTEM.md`](DESIGN_SYSTEM.md)):
      full-width header (nav) + collapsible left/right panels + center work panel
      + full-width footer; subtle-blue/orange light + dark themes (toggle); HUD
      forced dark. Shared with mobile (Ionic vars) — same palette across clients
- [ ] Add app-bar week selector + health indicator
- [x] **Left menu** (Board · New Load · Drivers · Communications) + work-panel
      toggles (Today|Week, Compact); **right Communications rail** (`CommPanel`:
      channels + live conversation + composer); **toasts** (`Toaster`, top-right);
      panel toggles as disclosure arrows (▼/▶/◀); Communications menu item takes
      the full work area
- [x] **Views**: Fleet (drivers + equipment list), **Map** (geo-positioning of
      fleet locations, Leaflet + table), Settings (appearance + info) — center
      swaps per menu item
- [x] **Comms push/WebSockets**: `CommBus` + Wt server push delivers sent
      messages instantly; `wt_config.xml` enables WebSockets; 30 s reconcile poll
      bridges off-server messages
- [~] **Cross-client realtime** via a **WebSocket bridge over ALS → Kafka**
      (`realtime/`, [`REALTIME.md`](REALTIME.md)). DONE: bridge service
      (Kafka consumer → WS, JWT, reconnect) + mobile client (live messages/unread,
      reconcile fallback); topic-per-type + channel_id correlation id (key for
      ordering); ALS producer snippet + post-generate install automation in
      `als-extensions/` (`make als-extensions`); **desktop `RealtimeClient`**
      (Boost.Beast, opt-in `-DFD_REALTIME_CLIENT=ON`) → `CommBus` with id de-dup.
      config-driven multi-topic routing (`DEFAULT_ROUTES` / `KAFKA_TOPIC_ROUTES`)
      so new purposes (loads/trips/alerts…) are config, not code. Realtime data
      plane: mobile applies message events directly; desktop `PositionBus` drives
      live HUD/Map fleet locations. REMAINING: enable `KAFKA_CONNECT` in ALS;
      build/verify the desktop client on Linux
- [ ] Wire left-panel filters + a right-panel selected-load inspector to data
- [x] Load intake form (new load) + driver/equipment assignment (POST /Load)
- [x] **HUD** surface (`/hud`) + `HudControlBus` (Wt server push): the console's
      Today/Week toggle publishes `SetMode`; HUD sessions auto-switch
- [x] **Distributed HUD:** schema `hud_command` (+ POST from the console) so a
      remote HUD can be driven; `command_type`/`arg` event log
- [ ] Remote HUD: subscribe to `hud_command` (poll/stream) when run off-server
- [ ] Extend the command bus to more commands (`FocusDriver`, `HighlightLoad`)
      (enum + HUD handlers stubbed)
- [ ] Dispatcher messaging view (mirrors mobile Feature 1)
- [x] **HUD truck-locations panel**: latest position per rig (15s poll), fed by
      `position_report` (Feature 2)
- [ ] **HUD map tiles** (Leaflet/HERE) layered over the positions data

> Wt isn't installable in the dev sandbox, so the C++ above is **not compiled
> here**. Version-sensitive spots are flagged in `ApiClient.cpp` (Http `done()`
> error_code type; `Json::Type::Null` enum) and `main.cpp`/theme (`WBootstrap5Theme`
> needs Wt ≥ 4.5).

## Feature 2 — Truck Location & Dispatcher HUD

Powers the HUD's map. See "Planned" in `domain-model.md`.
**Decided (2026-06-02):** PostGIS spatial data is separated from the ALS APIs
(see [`SPATIAL_GIS_DATA_CONSIDERATIONS.md`](SPATIAL_GIS_DATA_CONSIDERATIONS.md)).

- [x] Schema: `location_source` lookup (`airtag`, `google_device`,
      `phone_push`) + `position_report` time series — **lat/lng numeric on ALS
      tables** (no geometry). Verified on PostgreSQL 16.
- [x] Mobile: driver phone-push location (Locate tab → POST `/PositionReport`)
- [x] Desktop HUD: truck-locations panel (latest per rig, 15s poll)
- [x] Schema: navigation — `trip`, `waypoint`, `point_of_interest`, `route` +
      lookups (`trip_status`, `stop_type`, `poi_category`), lat/lng. Verified.
- [x] Mobile Trips: list, start trip; **mutable per-trip route** — trip overview
      drills into an Edit-waypoints page to add (fuel/lunch/load-stop/…), remove,
      or **drag-reorder** stops; stop types `lunch`+`load_stop`; waypoint edits
      stream live. **Auto route recompute** in `geospatial/` (Kafka
      `waypoint`/`trip` consumer → upsert `route`; haversine now, HERE pluggable).
      **Manual edits set `trip.route_locked`** so the optimizer won't auto-reorder;
      a driver can **Unlock & re-optimize** (confirm dialog) to clear the lock and
      trigger immediate recompute/re-optimization via the trip event.
- [~] Trips: start/stop lifecycle, turn-by-turn navigate, POIs pending
- [~] **AI route optimization** (single + multi pickup/drop-off, shared-trailer
      capacity). INTERIM DONE: nearest-neighbor optimizer (`geospatial/optimize.py`,
      no API key) runs on recompute for non-`route_locked` trips — origin/dest
      anchored, distance-greedy, persists new seq. FOUNDATION: two capacity
      dimensions — `equipment.deck_length_ft` + `equipment.weight_capacity_lbs`
      (per tractor/trailer config) and `load.deck_feet` + `load.weight_lbs`.
      DEFERRED (user researching the engine): the full **capacitated
      pickup-and-delivery** solve (pickup-before-dropoff + both capacity
      dimensions) — OR-Tools (self-hosted) vs HERE Tour Planning vs LLM-as-caller.
      It replaces the NN heuristic and still skips `route_locked` trips.
- [x] `gis` bootstrap SQL (`database/gis_bootstrap.sql`): PostGIS into `gis` +
      derived geography views; **verified** `public` stays clean (ALS-safe).
      Full standup in [`DEPLOYMENT.md`](DEPLOYMENT.md).
- [x] Separate geospatial endpoint (FastAPI, `geospatial/`): /health,
      /truck-stops/nearest, /trucks/near, /positions/latest — **verified** live
      against PG16+PostGIS as `fleet_gis`. (view vs. mirror: using views for now)
- [ ] Ingestion adapters for AirTag / Google sources (phone-push done)
- [ ] HERE routing/maps integration (truck-legal routes, bridge heights, truck
      stops); persist as `route.polyline`
- [x] Desktop HUD **map** (`Wt::WLeafletMap`, OSM tiles) with a marker per rig's
      latest position (15s refresh)
- [ ] HUD map: overlay routes (`route.polyline`) and driver-focus/load-highlight
      commands

### HUD trips & live tracking (requested 2026-06-20)

Make the HUD a live operations wall, not just a board mirror:

- [ ] **Trips on the HUD**, with a **scope selector**: single **driver** /
      **region** / **whole fleet**. Filters both the trips shown and the map.
- [ ] **Per-driver locations on the map** from latest `position_report`, with the
      **zoom level auto-fit** to the active scope (whole-fleet = fit all; single
      driver = zoom in).
- [ ] **Follow-driver mode**: continuously pan/zoom the map to a chosen driver's
      latest fix as new positions stream in (extends `HudControlBus::FocusDriver`,
      currently stubbed).
- Mechanism: controls live on the **console** and drive the HUD via
  `HudControlBus` (the HUD is an operator-less wall display) — new commands
  `SetScope{driver|region|fleet, id}` and `FollowDriver{id}` alongside `SetMode`.
- DECIDED (2026-06-20): **"region" = US state derived from each driver's latest
  position lat/lng** (no schema change; approximate). Driver + fleet scopes can
  land first; the state lookup is a geospatial/client mapping.

## Feature 3 — Hey Dispatch driver voice assistant

Hands-free assistant: driver says "Hey Dispatch", push-to-talk, on-device speech.
A FastAPI service (`assistant/`) calls a **pluggable AI provider** with tool use.
Design: [`AI_ASSISTANT.md`](AI_ASSISTANT.md).
**Decided (2026-06-11):** push-to-talk first; STT/TTS on-device (Web Speech API);
AI provider is admin-selectable (Anthropic default / OpenAI / Ollama).

- [x] Service scaffold (`assistant/`): FastAPI `/assist` + `/health`, config,
      Dockerfile, README
- [x] Tool schemas + handlers: `send_message_to_dispatcher` (→ ALS JSON:API),
      `get_eta`, `validate_route`, `alternate_routes` (→ HERE, with fallbacks),
      `tech_help` (built-in KB)
- [x] **Pluggable AI provider** (`app/providers/`): Anthropic (default; adaptive
      thinking, effort, prompt caching) + OpenAI-compatible adapter (OpenAI /
      Ollama via base URL); selected by `ASSISTANT_PROVIDER`, lazy SDK imports
- [x] Mobile: "Dispatch" tab — push-to-talk (Web Speech STT → `/assist` → TTS),
      pulls dispatcher channel + active-trip destination for context
- [ ] Wake-word ("Hey Dispatch") hands-free activation (push-to-talk shipped)
- [ ] Real tech-help knowledge base / retrieval (replace placeholder KB)
- [ ] Per-driver truck profile (height/weight) feeding route tools — needs auth
- [ ] Live end-to-end test against a configured provider + HERE key

## Feature 4 — Telegram-style team communications (channels · groups · members)

Evolve the messaging core (Feature 1) toward the professional team-comms model
fleet operators already use on Telegram. Researched 2026-06-20 (channels vs
groups vs supergroups; granular admin rights; member permissions/restrictions;
topics/forums; invite links + join requests; reactions). Our `channel_type`
(direct/group/broadcast) already mirrors Telegram's three containers; the gaps
are roles, enforcement, organization, and onboarding.

Standing constraint: each item is a **schema change** → edit
`database/schema.sql` + `seed_data.sql`, run **`/verify-db`** on throwaway PG16,
then **regenerate ALS** (+ `make als-extensions`); update `docs/domain-model.md`.
Clients (mobile + desktop `CommPanel` directory/roles) follow.

Priority order (P1 = do first; small→large, value-weighted):

- [~] **P1 — Admin role + broadcast posting lock** *(smallest, highest value)*.
      DONE: `admin` added to `channel_member_role` (owner/admin/member); LogicBank
      rule `als-extensions/logic_discovery/comms_governance.py` blocks non-owner/
      admin posts in `broadcast` channels; seed has a "Fleet Announcements"
      broadcast channel. Verified on PG16. Desktop: composer is **hidden with a
      reason** for read-only members of a broadcast channel (`CommPanel`
      fetches `ChannelMember`, gates via `updatePostPermission`). Mobile:
      `ChannelPage` fetches the user's membership and replaces the composer with
      a read-only notice (`postBlockReason` in `api/client`). REMAINING: role
      badges in the directory; ALS regen on the Linux box to activate the rule.
- [~] **P2 — Member status & restriction**. DONE: `channel_member_status` lookup
      (active/muted/banned) + `channel_member.member_status_id` (default active) +
      `restricted_until` (expiry); LogicBank rule blocks posting while a mute/ban
      is active (NULL until = indefinite; past = expired). Verified on PG16.
      Desktop + mobile: composer also respects standing — hidden with "You are
      muted/banned" while a restriction is active. Mobile channel list shows
      role (Owner/Admin) + standing (Muted/Banned) + channel-type badges per row.
      REMAINING: admin UI to mute/ban/remove members; desktop directory badges.
- [~] **P3 — Topics (forum threads)**. DONE: `channel_topic` (channel_id, name,
      created_by, is_closed) + `message.topic_id` (nullable; General = NULL) +
      indexes; seed has a "Lubbock -> Denver" topic. Verified on PG16. Mobile:
      drill-in flow — Channel shows a **Topics** list + the General stream; a
      topic opens its own focused page (own timeline + compose-into-topic), back
      button to the channel (`ChannelPage` is topic-aware via route param;
      realtime carries `topic_id`; producer updated). **Topic creation is
      restricted to admins/dispatchers** (channel owner/admin or dispatcher
      app-role) — enforced by a LogicBank constraint on `ChannelTopic` and gated
      in both UIs (drivers/updaters only browse topics). Desktop: `CommPanel`
      shows a topic-chip bar (General + topics) that filters the timeline +
      composes into the selected topic, with a gated "+ Topic" dialog
      (`fetchTopics`/`createTopic`; `createMessage` takes a topicId). DONE both
      clients.
- [ ] **P4 — Invite links + join requests**. `channel_invite` (token, created_by,
      member_cap, expires_at, requires_approval, revoked) + a pending-join state
      on `channel_member` (or `channel_join_request`). Onboards carriers/drivers
      via a shareable link with optional admin approval. Needs auth/identity.
- [ ] **P5 — Reactions (acks)**. `message_reaction` (message_id, user_id, emoji,
      unique per (message,user,emoji)). 👍/✅ "got it" acks are genuinely useful
      for dispatch confirmation. Realtime via the existing Kafka `message` plane.
- [ ] **P6 — Read receipts / richer presence**. Surface per-member read state
      (we already track `channel_member.last_read_at`): "seen by N" in small
      groups; server-side `unread_count` (LogicBank) replacing client compute.
- [ ] **P7 — Channel ↔ discussion link (comments)**. Link a `broadcast` channel
      to a `group` channel so a broadcast post opens a comment thread (Telegram's
      channel-comments). Add `channel.linked_channel_id` (self-FK, nullable).
- [ ] **P8 — Polish**: slow mode (per-channel min seconds between posts),
      broadcast **view counts**, new-member **history visibility** toggle, channel
      **statistics**, author **signature** on broadcast posts. Each self-contained.

> Sequencing rationale: P1–P2 are one small, safe schema bump that unlocks the
> core broadcast/governance behavior; P3 is the headline organizational feature;
> P4 enables external onboarding; P5–P8 are independent enhancements layerable in
> any order once the role/topic foundation exists.

### Desktop message-board parity run (2026-06-20)

Close the remaining gaps where the desktop `CommPanel` trails the mobile board.
Sequenced cheapest→largest; each ships as its own commit (Wt builds on Linux).

- [x] **1 — Directory badges + unread**. Desktop directory rows show **unread**
      counts + **role/standing** badges (Owner/Admin, Muted/Banned); rail chips
      show unread in the label. `fetchMyMemberships` + per-channel unread; entering
      a channel clears it and stamps `markChannelRead`; incoming messages to other
      channels bump the badge. (Type is conveyed by the directory grouping.)
- [x] **2 — Reply/quote + emoji**. Per-message hover **Reply** → composer reply
      banner; the reply renders a quoted snippet in the timeline
      (`message.reply_to_id`; `createMessage` takes a replyToId). Composer **emoji**
      picker (toggle panel, trucking-relevant set, UTF-8). Wt builds on Linux.
- [x] **3 — Pins + Saved**. Desktop `CommPanel`: per-message pin (scope picker:
      self/channel/everyone) + a visible-pins strip + inline 📌/🔖 markers;
      save/unsave. New `SavedView` (left-menu "Saved") = cross-channel archive.
      ApiClient: MessagePin/SavedMessage CRUD + `deleteReq`. Mirrors mobile.
- [x] **4 — Attachments**. Desktop `CommPanel`: `WFileUpload` in the composer →
      base64 → `createDocument` + `linkMessageDocument` (message → document →
      link); attachment chips per message (metadata via sparse fieldset), click
      opens the full document in a new tab (data URL). Mirrors mobile. DONE →
      **desktop message-board parity run complete**.

### Message-board robustness (clarified 2026-06-20)

Additional requirements for a robust board. Decisions captured from the user:

- [ ] **Tag/mention users** (`@mention` in a group/board/channel). `message_mention`
      (message_id, mentioned_user_id) written on send; composer autocomplete from
      channel members; highlight + notification/toast + unread bump for the
      mentioned user.
- [ ] **Board stats + unread** in the comms toolbar. Per-channel + overall unread
      (from `channel_member.last_read_at`), message/member counts, most-active
      channels; server-side `unread_count` via LogicBank (replaces client compute).
- [ ] **User online status** (toolbar switch). DECIDED: it's the **user's online
      status** (presence/duty), not channel state. Add `app_user.user_status_id`
      (lookup: online/away/driving/off-duty) shown beside the user in comms +
      fleet view; live via the realtime plane.
- [~] **Backup & restore of comms data**. DECIDED: back up **messages, members,
      and related message data**. DONE: in-app **per-board export** on the desktop
      console — the comm-panel header (present in both the rail [reduced/icon] and
      the full take-over view [full/labeled]) has an **Export** action that bundles
      the board's channel meta + topics + members + messages (raw JSON:API docs)
      and downloads a `board-<name>-<date>.json` via a Blob URL
      (`ApiClient::fetchRaw`). Mobile: per-board **Export** action in the channel
      header (admins/dispatchers only) bundles channel + topics + members +
      messages to a downloaded JSON (Blob URL). REMAINING: **restore** (import a
      bundle), align the desktop raw-doc vs mobile structured format when restore
      lands, native Capacitor file save (Filesystem/Share), and the DB-level
      `pg_dump`/restore + runbook/timer path for very large boards.
- [ ] **Archive / revive channels**. DECIDED: an **archived channel is no longer
      visible to users**; **admins can revive** (unarchive) it. Wire up the
      existing `channel.is_archived`: filter archived channels out of normal
      client queries; admin-only "Archived" view with unarchive. Enforce
      visibility server-side (LogicBank/ALS grants), not just client filtering.

## Feature 5 — Load-board ingestion (dispatcher load capture)

Dispatchers spend their day **searching load boards** (DAT, Truckstop, 123Loadboard,
broker portals) and pulling matching loads into the dispatch board. Today that's
manual re-keying. This feature automates the "board → our `load`" hop. Researched
2026-07-13.

**How loads actually get pulled in (the mechanisms, cheapest → most integrated):**

1. **Manual copy/paste (baseline).** Dispatcher reads a posting (origin, dest,
   equipment, weight, rate, miles, broker + reference #, pickup/delivery dates)
   and re-keys it into the TMS. This is what our existing **New Load** form does.
   Universal, works with any board, but slow and error-prone.
2. **Paste / document parse (no partner account needed).** Dispatcher pastes the
   raw posting text — or drops the broker's **rate-confirmation PDF / email** —
   and we parse it into a structured **draft load**. Works across *every* source,
   needs no API partnership, and fits what we already have (the `assistant/` AI
   provider for extraction + the CMS `document` store for the source artifact).
   Best first automation.
3. **Load-board partner APIs (real integration).** The big boards expose REST
   APIs for **load search + import + book**, gated by a paid account/keys:
   - **DAT** — `developer.dat.com` REST APIs (Load Board, **BookNow**, Tracking,
     Freight Posting). Any subscription tier allows REST; production needs a
     service account (company + MC#) and a setup fee (~$500–$1,000).
   - **Truckstop** — `developer.truckstop.com` **Load Search** + Load Management,
     with **Book It Now**; offered as TMS integrations.
   - **123Loadboard** — documented post/find/move API.
   These let us search the board *inside* our console and one-click import (and,
   with BookNow/Book-It-Now, book) — subscription + partner onboarding required.
4. **EDI 204 load tender (established broker relationships).** For contracted
   freight, brokers/shippers **tender** loads via ANSI X12 **EDI 204** (pickup,
   drop, equipment, schedule); the carrier replies **EDI 990** accept/reject.
   Fully programmatic, no "searching" — the loads come to us. Heavier (EDI VAN /
   AS2 + trading-partner setup); worthwhile once we have direct broker accounts.

**Architecture fit (our rules):** the ingestion/parse logic is a **new adapter**
(a thin `intake/` service, or a route on the existing `assistant/` FastAPI which
already carries a pluggable AI provider) — but it must **write through the ALS
JSON:API** like every other client (never straight to Postgres). Imports land as
`load_status = draft` for the dispatcher to review/confirm before dispatch, and
must **dedup** shipper/receiver/location/commodity against existing rows.

Sequenced plan:

- [ ] **P1 — Paste-to-load intake (mechanism 2).** A console **"Import load"**
      action: paste posting text → parse (regex for the stable fields + the
      `assistant/` AI provider for the messy free-text) → prefilled **draft**
      New-Load form for confirm → `POST /Load` (+ dedup lookups for
      shipper/receiver/location/commodity). No partner account needed.
- [ ] **P2 — Rate-con document parse.** Same pipeline seeded from an uploaded
      **PDF/email** (store the artifact via the CMS `document`, link it to the
      created load via `load_document`); OCR/text-extract → AI structured extract.
- [ ] **P3 — Schema for provenance.** `load_source` lookup (manual · paste ·
      rate_con · dat · truckstop · edi_204) + `load.source_id`,
      `load.external_ref` (broker load/reference #), `load.broker_name`
      (or a `broker` party table if it grows). `/verify-db` on PG16; regen ALS.
      Lets us trace where a load came from and avoid re-importing duplicates.
- [ ] **P4 — Load-board API search + import (mechanism 3).** Start with **one**
      board (DAT *or* Truckstop) behind a provider interface mirroring the
      `assistant/` provider pattern: search from the console, preview results,
      one-click import to a draft load. Keys/subscription are deployment config
      (env), never committed. Add **Book It Now / BookNow** as a follow-on.
- [ ] **P5 — EDI 204 intake (mechanism 4).** Accept broker load tenders (204) →
      draft loads; respond 990. Requires an EDI VAN/AS2 + trading-partner setup;
      revisit when direct broker relationships exist.

> Decision needed from the user before P4: **which board(s)** we hold accounts
> with (drives DAT vs Truckstop first) and whether we want **auto-book** or
> **import-then-book-manually**. P1–P3 need no external accounts and deliver the
> bulk of the day-to-day time savings, so they lead.

## Feature 6 — Vehicle detail & lifecycle (asset management)

Turn each rig from a name-on-a-chip into a managed **asset**: a full detail page
with valuation, scheduled maintenance, mileage, and a for-sale flow. Requested
2026-07-13.

DONE (2026-07-13):
- [x] **Fleet vehicle toast** — clicking a rig on the Fleet page pops the same
      bottom-right info toast as the board loads (unit · trailer type, power unit,
      deck length + weight capacity, ramps/duals). Stacks when several are picked.
      (`FleetView` owns a bottom-right `Toaster`; `fetchEquipment` enriched with
      the spec fields.) No schema change.

Proposed data model (needs a quick confirm — see open questions):
- [ ] **Schema — equipment asset fields**: extend `equipment` with `year`,
      `make`, `model`, `vin`, `odometer_miles`, `in_service_date`,
      `purchase_price`, `current_value` (valuation), `status_id`
      (→ new `equipment_status` lookup: in_service / in_maintenance / for_sale /
      retired), `for_sale` (bool) + `for_sale_price`. `/verify-db` on PG16; regen ALS.
- [ ] **Schema — maintenance**: `maintenance_record` (equipment_id, service_type,
      performed_on, odometer_miles, cost, vendor, notes) for **history**, and
      `maintenance_schedule` (equipment_id, service_type, interval_miles and/or
      interval_days, next_due_on, next_due_odometer) for **upcoming** service.
      Optionally a `service_type` lookup (oil, DOT inspection, tires, brakes…).
- [ ] **Schema — valuation history** (optional): `valuation_entry` (equipment_id,
      as_of, value, source) so `current_value` has a trend, not just a point.
- [ ] **Desktop `VehicleDetailView`** — a full-page center view opened from the
      Fleet page (chip "Details" action or the toast), with tabs:
      **Overview** (specs + status + assigned driver) · **Valuation** (purchase
      price, current value, depreciation, **for-sale toggle + for-sale price**) ·
      **Maintenance** (upcoming schedule with due/overdue flags + service history) ·
      **Mileage** (odometer + recent trend, tie into `position_report` later) ·
      **Documents** (title/registration via the CMS `document` + a new
      `equipment_document` link).
- [ ] **Mobile** parity later (read-only vehicle detail for drivers).
- [ ] **Smitty Services integration** — share vehicle identity/specs/mileage/status
      and consume service/repair data (work orders, maintenance schedule, in-shop
      status) from Smitty. Full spec in
      [`INTEGRATION_SMITTY.md`](INTEGRATION_SMITTY.md) (canonical VIN key, per-field
      system-of-record, Kafka event contract + JSON:API reconcile). The Feature-6
      asset fields (VIN, odometer, status) are the Fleet side of this contract.

Open questions (blocking the schema build — answer any time):
1. **Valuation source** — manual `current_value` you set, or auto-depreciate from
   `purchase_price` + a rate/useful-life? (Start manual, add depreciation later?)
2. **Maintenance trigger** — schedule by **mileage interval**, **time interval**,
   or **both** (whichever comes first)?
3. **For-sale** — just a price + flag on the rig, or a fuller listing (photos,
   description, contact) that could feed a public/marketplace view down the road?
4. Any **specific data points** you want on the Overview tab beyond year/make/
   model/VIN/odometer/specs (e.g. plate, DOT #, insurance policy/expiry, fuel type)?

## Cross-cutting

- [x] **DB schema separation (DECIDED + done):** shared instance with
      Smitty/Student-Onboarding → all Fleet app tables in the **`fleet`** schema
      (owned by `fleet`, ALS reflects it); PostGIS in shared **`gis`**
      (Fleet's `fleet_*` views owned by `fleet_gis`). See `MIDDLEWARE_SETUP.md`
      + `DEPLOYMENT.md`. Verified: 40 tables in `fleet`, 0 in `public`.
- [~] **AuthN/AuthZ** (roles: dispatcher, driver, updater). DONE: ALS built-in
      JWT auth against `app_user` (password_hash + profile fields + avatar FK);
      mobile login/profile/logout, bearer header on all calls, 401 auto-logout,
      `currentUser.ts` placeholder replaced by `useAuth()`; **desktop (Wt) console
      login + profile** (LoginView/ProfileView, token on all ApiClient calls,
      sign out). See [`AUTHENTICATION.md`](AUTHENTICATION.md). REMAINING:
      role-based grants (LogicBank/ALS security), desktop **avatar upload** + HUD
      service token, native secure token storage
- [ ] LogicBank rules in generated middleware (weekly cap, settlement math)
- [ ] Seed data growth for demos

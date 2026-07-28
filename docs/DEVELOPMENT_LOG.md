# Fleet Dispatcher — Development Log

Newest first. One entry per meaningful change set; pair with the checklist in
[`TODO.md`](TODO.md).

## 2026-07-13

### Integration spec: Fleet ⇄ Smitty Services (vehicle sync)
- New **`docs/INTEGRATION_SMITTY.md`** — a hand-off spec for syncing vehicle +
  service/repair data with Smitty Services. Canonical **VIN** key with per-app id
  correlation; **system-of-record per field** (Fleet owns identity/specs/odometer/
  operational status, Smitty owns work orders/maintenance schedule/in-shop status);
  recommended **Kafka event contract** (`vehicle.v1` + `vehicle-service.v1`,
  VIN-keyed, idempotent envelope) mirroring Fleet's existing topic strategy, plus
  JSON:API backfill/reconcile; neutral canonical schema sketch; six open decisions
  for the Smitty side; phased P1–P4 rollout. Linked from TODO Feature 6.

### Fleet: click a vehicle → info toast (+ detail-page spec)
- Clicking a rig on the Fleet page pops the same **bottom-right toast** as the
  board loads: unit · trailer type, power unit, deck length + weight capacity,
  ramps/duals; stacks when several are selected. `FleetView` owns a bottom-right
  `Toaster`; `fetchEquipment`/`EquipmentInfo` enriched with the spec fields. No
  schema change.
- Captured **Feature 6 — Vehicle detail & lifecycle** in TODO (full detail page:
  Valuation incl. for-sale price · scheduled Maintenance · Mileage · Overview),
  with a proposed schema (equipment asset fields + `maintenance_record`/
  `maintenance_schedule` + optional valuation history) and four open questions
  gating the schema build.

### Avatar palette → natural skin tones
- Reworked the person palette from arbitrary UI colours to a **natural skin-tone
  spectrum** (ivory → espresso, 10 steps), so avatars represent real people.
  Initials now switch dark/white by tone luminance (`icons.h contrastText()`) to
  stay legible; light-tone avatars get a neutral hairline. Seed + `icons.h` +
  DESIGN_SYSTEM/domain-model updated; verified on PG16.

### Board: click a load → stacking load-info toasts
- Clicking a load on the board pops a **bottom-right toast** with that load's
  detail (driver · status, rate, run type, pickup→delivery, loaded/deadhead
  miles). Selecting several **stacks** them (sticky until the × closes each),
  matching the dispatcher "collect a few loads" flow. Works from both the Week
  grid chips and the Today list rows.
- `Toaster` gains a `Position` (TopRight default = message notifications;
  **BottomRight** = the board stack); the board owns its own bottom-right
  instance (fixed-positioned, survives body re-renders). Toast accent follows
  status (delivered = success, cancelled = danger). New CSS `.fd-toaster-bottom`
  / `.fd-toast-sub`; load chips get a click affordance. Wt builds on Linux.

### Colour-coded people + vehicles; Fleet view redesign
- **Schema**: `avatar_color` palette lookup (10 curated colours) + `app_user`
  .avatar_color_id (self-picked) + `driver.avatar_color_id` (admin-assignable;
  render precedence explicit → linked user → derived-from-name) +
  `trailer_type.color_hex` (rigs colour-coded by type). Seed assigns colours to
  the 10 drivers/3 users + the 5 trailer types. Verified on PG16 (43 fleet tables).
- **Desktop**: shared `icons.h` (palette/trailer hex by id — stable seed lookups,
  hardcoded like `statusName()`; person-avatar HTML builder, deterministic name
  fallback, role badge). **Fleet view redesigned** off the Bootstrap zebra table
  (white-on-dark stripes) into a clean roster: colour avatar + name/home-base,
  type badge, active-status dot; **equipment colour-coded by trailer type with a
  legend** (`fetchEquipment` typed fetch). **Board**: colour avatar before each
  driver name, vertically centred in the row. **Profile**: 10-swatch colour
  picker → `avatar_color_id` (PATCH). Wt builds on Linux; palette maps are
  version-neutral pure C++.
- Mobile (profile picker + directory avatars) is the follow-up.

### Docs: role-based manual test plan
- New **`docs/TEST_PLAN.md`** — hand-executable acceptance tests per role.
  Hierarchical numbering (section → case `x.y` → step `x.y.z`), hard page breaks
  before each section for printing/hand-out, and a Pass/Fail/Blocked line per
  case. Sections: Auth (all roles) · Dispatcher · Driver · Updater · Comms
  member-role & standing (orthogonal axis) · cross-client parity/regression.
  Negative cases exercise the server-side governance (broadcast lock, mute/ban,
  topic gating) via API, not just the UI. Appendices: traceability + SQL setup
  helpers.

### Deploy: Feature-4 shipped to the shared host + update runbook captured
- **Desktop build fix**: `WFileUpload::contentDescription()` returns `Wt::WString`
  in the host's Wt version — appended `.toUTF8()` (matches `clientFileName()`),
  clearing the last attachments compile blocker. Desktop console rebuilt clean.
- **Production redeploy verified** on `mycloud-server-01` (shared host: three ALS
  apps + nginx): ALS regenerated from the Feature-4 schema, `als-extensions`
  re-installed (`Logic Bank … 14 rules loaded`, `discovered logic:
  [comms_governance.py, fleet_events.py, …]`), login 200, `ChannelTopic` reflected
  (200). Server-side comms governance (broadcast lock · mute/ban · topic gating)
  is now live and enforcing.
- **`docs/DEPLOYMENT.md`**: new major section **"Redeploying after a change (update
  runbook)"** — the repeat path (pull → regen ALS + re-install extensions → rebuild
  Wt → mobile build → restart services → verify), a change→steps matrix, the Wt
  `.toUTF8()` gotcha, the quiet-governance verification, and a redeploy
  troubleshooting table distilled from this deploy.

## 2026-07-07

### Desktop parity run — step 4: attachments (COMPLETE)
- `CommPanel` composer gains a `WFileUpload`: on select it auto-uploads; we read
  the spooled file, base64-encode (`Wt::Utils::base64Encode`), then message →
  `createDocument` → `linkMessageDocument` (mirrors mobile). Attachment **chips**
  render per message (metadata via sparse fieldset, `data` omitted); clicking a
  chip fetches the full document and opens it in a new tab via a data URL.
- ApiClient: `Document`/`MessageDocument` models + `attachmentsForMessage`,
  `fetchDocumentMeta` (sparse), `fetchDocument` (full), `createDocument`,
  `linkMessageDocument`.
- **Feature-4 desktop message-board parity run is now complete** (steps 1–4:
  badges/unread · reply/emoji · pins/Saved · attachments). Wt builds on Linux.

### Desktop parity run — step 3: pins (scoped) + Saved archive
- `CommPanel`: per-message hover actions (reply · pin · save). Pin opens a scope
  picker (Only me / Channel / Everyone → `MessagePin.pin_scope_id`); a
  visible-pins strip sits atop the conversation (own pins removable); inline
  📌/🔖 markers. Pins visible to me = channel/everyone pins + my self-pins.
- New **`SavedView`** (left-menu "Saved") — the cross-channel personal archive:
  lists saved messages (resolves each body), remove inline. Registered in CMake +
  wired into `Shell`.
- `ApiClient`: `MessagePin`/`SavedMessage` models + CRUD (fetch/pin/repin/unpin,
  fetch/save/unsave, fetchMessage) + a new `deleteReq` (Http::Method::Delete).
- Closes the Feature-4 desktop parity run except step 4 (attachments).

### Desktop board: fix week-grid collapse on tablets
- On narrower viewports (iPad Pro) the fixed 260px + 320px side panels squeezed
  the center work panel until the 7-column week board collapsed — day headers
  word-wrapped into vertical `M-o-n` letters. Fixes: `.fd-week-board` gets a
  `min-width` and the `.fd-tablecard` scrolls horizontally instead of collapsing;
  day headers `white-space: nowrap` (no mid-word breaks; only load cells wrap);
  and the responsive stack breakpoint raised 900px → 1200px so tablet widths give
  the center full width (panels flow full-width).
- **Viewport meta** added (`main.cpp applyChrome`): without it Safari laid the
  console out at ~980px in every orientation, so the responsive breakpoints never
  saw the real width and the iPad **stacked even in landscape**. Now
  `width=device-width, initial-scale=1` — the media queries get true widths.
- `BoardView`: the week runs **Sunday→Saturday** (`currentWeekDays` back-offsets
  to Sunday). Weekday labels are **responsive** — full names on desktop, **single
  letters on tablet/phone** (both rendered; `.fd-day-full`/`.fd-day-short` toggle
  at ≤1200px). Date under each day (`.fd-day-date`) restyled to the normal text
  colour at 0.8 opacity so it's **readable on the darker header band** (was
  washed-out `text-muted`).

## 2026-06-30

### ALS auth: fix als-extensions for a fresh from-scratch regenerate
Full ALS regenerate (project was deleted) surfaced four latent, version-sensitive
bugs in our `als-extensions` — all fixed in the tracked files so future regens
just work. Verified end to end: `dispatch1`/`fleet123` logs in (200 + JWT) and
authenticated requests resolve the user.
- `Rule` import → `logic_bank.logic_bank` (not `rule_bank.rule_bank`).
- `declare_security.py` must define `declare_security_message` (ALS reads it).
- `get_user` validates the werkzeug password inline (newer ALS validates via
  `get_user`, treating `None` as failure) + a distinct 403 for inactive accounts.
- `get_user` is dual-purpose (login `password` str vs JWT-verify `jwt_data` dict)
  → guard the password check with `isinstance(password, str)` (else `dict.encode`
  500 on the first authed call).
- Documented the gotchas (+ SECURITY_ENABLED, add-auth order, PG-restart pooling,
  JSON content-type, hash-via-shell) in `docs/AUTHENTICATION.md`.

## 2026-06-20

### Desktop parity run — step 2: reply/quote + emoji
- Per-message hover **Reply** opens a composer reply banner; the sent reply
  carries `reply_to_id` and renders a quoted snippet above the message
  (resolved from the loaded set). `createMessage` gains a `replyToId`; `Message`
  gains `reply_to_id`. Composer **emoji** picker — a toggle panel of a compact
  trucking-relevant set, appended to the draft (UTF-8 end to end). Wt builds on
  Linux.
- Region decision recorded for the HUD: "region" = US state from position lat/lng.

### Desktop parity run — step 1: directory badges + unread
- Desktop channel directory now shows per-channel **unread** counts +
  **role/standing** badges (Owner/Admin, Muted/Banned); rail chips carry the
  unread count in their label. `ApiClient::fetchMyMemberships` (role/standing +
  last_read_at) + per-channel unread count; selecting a channel clears it and
  `markChannelRead` stamps last_read_at; messages to other channels bump the
  badge live. `ChannelMember` gains `last_read_at`. Wt builds on Linux.

### Desktop parity: P3 topics in CommPanel
- Desktop comms reaches topic parity: a topic-chip bar (General + each topic)
  filters the conversation and sets the compose target; a gated "+ Topic" dialog
  creates topics (owner/admin or dispatcher app-role, mirroring the server rule).
  CommPanel now retains the channel's full message set (`allMessages_`) and
  renders a topic-filtered timeline (send/push/refresh all route through it).
- ApiClient: `fetchTopics` / `createTopic` (+ `parseTopic`); `createMessage`
  takes a `topicId`. Built carefully — Wt isn't compiled in-sandbox; build on the
  Linux box.

### Mobile parity: per-board export + role/standing badges
- **Per-board export** (mobile): an Export action in the channel header (admins/
  dispatchers only) bundles channel + topics + members + messages into a
  `board-<name>-<date>.json` downloaded via a Blob URL — matches the desktop
  console. New `membersForChannel` client fn.
- **Role/standing badges**: the channel list shows per-row badges — channel type
  (Direct/Group/Broadcast), the user's role (Owner/Admin), and standing
  (Muted/Banned, danger) — surfaced at the row edge. Memberships captured in the
  existing per-channel load.
- Mobile build clean.

### Comms: topic creation restricted to admins/dispatchers
- Topic creation is now governed: only a channel owner/admin or a dispatcher
  (app-role) may add topics; drivers/updaters just browse them. Enforced by a
  LogicBank constraint on `ChannelTopic` (`als-extensions/comms_governance.py`)
  and gated in the mobile UI — a "+ New" appears in the channel's Topics header
  only for those users (`canManageTopics` helper). Mobile build clean.

### Mobile parity: P3 topics (forum threads, drill-in)
- Mobile-native flow (one focus per screen): the Channel page shows a **Topics**
  list + the **General** stream (messages with no topic); tapping a topic pushes
  a focused topic page (its own timeline + composer, back to the channel).
  `ChannelPage` is topic-aware via an optional `:topicId` route param — General
  filters to `!topic_id`, topic mode to `topic_id === topicId`; compose/attach
  carry the topic.
- API: `topicsForChannel` / `getTopic` / `createTopic`; `createMessage` takes an
  optional `topicId`. Realtime message events now carry `topic_id` (mobile
  handler + `als-extensions` producer). Mobile build clean.

### Mobile parity: P1/P2 composer gating
- Mobile `ChannelPage` now fetches the user's `ChannelMember` and, when they
  can't post (broadcast non-admin, or muted/banned with an active restriction),
  replaces the composer with a single read-only notice — mobile-native, no
  clutter. Shared `postBlockReason()` helper + `CHANNEL_TYPE`/`MEMBER_ROLE`/
  `MEMBER_STATUS` constants in `api/client`. Types: `ChannelMember` gains
  `member_status_id`/`restricted_until`; `Message` gains `topic_id`; new
  `ChannelTopic` (sets up P3). Mobile build clean.

### Comms P1/P2 desktop: role/status-aware composer
- `CommPanel` fetches the selected channel's `ChannelMember`s
  (`ApiClient::fetchChannelMembers`) and gates the composer via
  `updatePostPermission`: hidden with a reason for read-only members of a
  broadcast channel, or while muted/banned (respects `restricted_until` expiry).
  Optimistic (composer on) until members load; server LogicBank rule remains the
  authority. Models: `ChannelMember`, `Topic`; `Message` gains `topic_id`.

### Comms: in-app per-board export + shared header toolbar
- **Per-board export** (desktop console): an **Export** action bundles a board's
  channel meta + topics + members + messages (raw JSON:API documents, high page
  limit) into `board-<name>-<date>.json`, downloaded client-side via a Blob URL.
  New `ApiClient::fetchRaw` (returns the JSON:API body verbatim); export logic in
  `CommPanel` (chained fetches → assemble → JS download).
- **Shared comm-panel header**: factored into `buildConvoHead(parent, full)`, now
  present in **both** layouts — a reduced (icon) action set in the right rail and
  the full (labeled) set in the take-over view. Future actions (stats, status,
  archive) slot in here, gated on `full`.

### Comms Feature 4 — P1–P3 foundation (admin role · status · topics)
Schema-first (golden rules); verified on a throwaway PG16 (**42 tables** in
`fleet`, `public` = 0). ALS regen + `make als-extensions` needed on the Linux
box to activate the rule; client UI is the next slice.
- **P1 admin role + broadcast lock** — `channel_member_role` gains `admin`
  (owner/admin/member). New LogicBank rule
  `als-extensions/logic_discovery/comms_governance.py`: in `broadcast` channels
  only owner/admins may post. Seed adds a "Fleet Announcements" broadcast channel.
- **P2 member status/restriction** — `channel_member_status` lookup
  (active/muted/banned) + `channel_member.member_status_id` (default active) +
  `restricted_until`; the same rule blocks posting while a mute/ban is active
  (NULL = indefinite, past = expired).
- **P3 topics** — `channel_topic` (channel_id, name, created_by, is_closed) +
  `message.topic_id` (NULL = General stream) + indexes; seed adds a
  "Lubbock → Denver" topic.
- Docs: domain-model (roles/status/topics + broadcast policy), TODO (P1–P3 →
  `[~]`; captured clarified board-robustness items: mentions, stats/unread, user
  online status, comms backup/restore, archive-hidden-with-admin-revive),
  als-extensions README.

### Desktop: full Communications view (Channels/Groups) + HUD map fix
- **HUD/Map white screen** — Wt ≥ 4.7 makes `WLeafletMap` read its Leaflet
  sources from the `leafletJSURL`/`leafletCSSURL` **config properties** and
  throws fatally if unset (blanked the HUD). Added the properties to
  `wt_config.xml`, made `run.sh` actually load it (`-c`), and dropped the now
  double-loading in-code `useStyleSheet`/`require` in `HudView`/`MapView`.
- **Communications take-over view** — `CommPanel` gained a `Layout` (Rail/Full).
  The Communications menu item now opens **Full**: a left **Channels (Groups)
  directory** (sections by type: Groups / Direct / Broadcast) + the conversation
  on the right. Reuses all the existing push/poll/send logic (DRY).
- **Auto-hide right rail** — entering comms collapses the redundant rail (CSS
  width transition); leaving restores it unless the user had collapsed it.
- **Animation** — the full view animates in (`fd-comms-in`).
- **Panel toggles** moved out of the header into an anchored bar directly above
  the body (left/right toggles float to their panel edges, fixed-width, so the
  ▼/▶/◀ glyph swap never shifts neighbours).
- **Roadmap** — researched Telegram's channels/groups/members model and captured
  a priority-ordered **Feature 4 — Telegram-style team communications** in
  `TODO.md` (P1 admin role + broadcast lock → P8 polish).

## 2026-06-11

### Styling pass: pro compressed font + bright-white rounded panels
- Font: **Barlow Semi Condensed** (professional, slightly compressed) across
  desktop (@import + --fd-font) and mobile (index.html link + --ion-font-family);
  base 15px for laptop density.
- Panels: **light-grey page** (#eceef2) with **bright-white rounded** cards
  (radius 12px). Per request: **no shadows** (removed work/card/toast shadows +
  the nav inset underline), **no borders** (removed 1px borders on cards, side
  panels, comm chips, message bubbles, footer; separation via gap), **more
  padding**. Body now lays out three white panels (left menu / work / comms) on
  grey with a 1rem gap. Left menu item styles unchanged (kept as-is).
- Mobile mirrors it the Ionic way (white ion-card/ion-item, shadow off, rounded)
  while keeping list-separator hairlines (mobile convention). mobile build clean.
- Docs: DESIGN_SYSTEM (typography + panel rules + grey palette).

### Fix: week board overlap (grid forced onto a <table>)
- `.fd-week-board` set `display:grid` (7 cols) on the Wt WTable (8 logical cols:
  Driver + 7 days) → driver-name cells overlapped the day columns. Switched to
  proper table styling (`table-layout: fixed`, top-aligned cells, fixed Driver
  column). Day headers now render weekday + short MM-DD (no full-date wrapping).

### systemd units for all services + architecture SVG
- Added systemd units (run as `fleet`, EnvironmentFile=/etc/fleet-dispatcher/*.env,
  Restart=on-failure) for the components that lacked them: **ALS**
  (`deploy/fleet-dispatcher-api.service`, :5659), **geospatial**
  (`geospatial/deploy/`, :5701), **mobile** (`portals/mobile/deploy/`, :5173 via
  vite preview; nginx documented as the prod option). Desktop/realtime/assistant
  units already existed. New `deploy/README.md` ties them together (install order,
  shared SECRET_KEY=FLEET_JWT_SECRET, Postgres/Kafka as external units).
- Replaced `docs/architecture.svg` with a full current diagram (clients, ALS,
  Kafka + topics, realtime bridge, geospatial, assistant, PostgreSQL/PostGIS),
  showing the two planes (req/resp vs realtime stream). Embedded in architecture.md.

### ALS Kafka producer snippet + post-generate install automation
- New `als-extensions/`: customizations to layer onto the generated ALS project.
  `logic_discovery/fleet_events.py` emits a Kafka event per Message/PositionReport
  insert (`Rule.after_flush_row_event` + `kafka_producer.send_kafka_message`, key =
  channel_id/equipment_id) — the producer side of the realtime bridge. Verified
  the ALS API against GenAI-Logic source (`send_kafka_message` signature).
- Automation: it installs into `logic/logic_discovery/`, which ALS **auto-discovers**
  and a `rebuild` **preserves** — so it survives rebuilds; a fresh `create` just
  needs `make als-extensions` (idempotent `install.sh`, target ALS_PROJECT).
  Documented chaining it onto the generate step. Tested the installer against a
  fake ALS project (copy + idempotent + rejects non-ALS dirs).
- Docs: als-extensions/README, MIDDLEWARE_SETUP (post-generate step), REALTIME
  (producer points here), CLAUDE golden rule + component table.

### Security customizations versioned in als-extensions (reproducible builds)
- Captured the working ALS auth into `als-extensions/security/`: `auth_provider.py`
  (authenticates against `app_user` + werkzeug hashes) and `declare_security.py`
  (role grants). `install.sh`/`make als-extensions` now copies them into the ALS
  project's `security/` after `add-auth`, so a from-scratch build is reproducible.
  Root causes captured in docs: `SECURITY_ENABLED` defaults False; default sql
  provider uses a separate sqlite/plaintext auth DB; "No Grants Yet" blocks reads.
  Tested installer (with/without security/); files syntax-checked.

### Docs: ALS run port (5659) clarified; db-setup heredoc fix
- `scripts/db-setup.sh`: removed backticks in a SQL comment that an unquoted
  heredoc treated as a command substitution (`fleet: command not found` during
  role/db creation). Verified end-to-end (40 fleet tables).
- MIDDLEWARE_SETUP + DEPLOYMENT: clarified the API port — ALS defaults to **5656**,
  Fleet Dispatcher serves **5659**; it's a *run* setting (config.py /
  `APILOGICSERVER_PORT` / positional `host port`), **not** a reason to regenerate
  (a fresh `create` wipes `logic/` + `add-auth`). Noted that our `.env` `API_PORT`
  is a convention for our scripts/clients, not read by ALS.

### Interim nearest-neighbor optimizer (no API key)
- `geospatial/optimize.py`: nearest-neighbor stop ordering (origin first / dest
  last, distance-greedy) — a no-key default so "auto-optimize" / unlock actually
  reorders without HERE. `recompute.py` runs it when a trip is NOT `route_locked`,
  persists the new `seq` (two-phase, atomic, via psycopg → no Kafka echo), then
  computes geometry; locked trips keep the manual order (geometry only).
- Race-safety: the mobile editor now **locks first (awaited)** before any
  add/remove/reorder; recompute reads `route_locked` fresh from the DB, so a
  manual edit is never clobbered by the optimizer regardless of Kafka event order.
- Limits (documented): NN minimizes distance only — multi-load pickup-before-
  dropoff precedence + trailer capacity (deck feet/weight) remain the deferred
  full solver. mobile build clean; geospatial py_compile clean.

### Unlock & re-optimize (revert manual route, with confirmation)
- Mobile trip overview: when `route_locked`, an **Unlock & re-optimize** button
  with an **IonAlert confirmation** ("Are you sure you want to revert your manual
  updates to this route?"). Confirm → `updateTrip(route_locked:false)`.
- Clearing the lock is a trip change → flows through the event plane (`trip`
  Kafka event → geospatial recompute consumer) so the route recomputes
  **immediately**; it also re-enables the (deferred) optimizer. Added an explicit
  optimizer hook comment in `recompute.py` (reorder when not locked).

### Drag-reorder waypoints + auto route recompute + manual-order lock
- Mobile: **drag-and-drop reorder** in the waypoints editor (IonReorderGroup);
  persists new `seq` with a two-phase (offset-then-final) update to respect
  UNIQUE(trip_id, seq). Delete moved to a trailing button (reorder handle on end).
- **Auto route recompute** (geospatial/): new `routing.py` (haversine dev provider;
  HERE pluggable), `recompute.py` (reads ordered waypoints → upserts fleet.route
  via FLEET_DATABASE_URL), and `recompute_consumer.py` (Kafka consumer on
  `waypoint`/`trip` → recompute) started on FastAPI startup + a `POST
  /route/recompute/{trip_id}` endpoint. Recompute **preserves order** (never
  reorders), so it's safe for manually-ordered trips. Syntax-checked.
- **Manual-order lock** (per request): new `trip.route_locked` (default FALSE).
  Any manual waypoint edit (add/remove/**reorder**) sets it via `updateTrip`; the
  (deferred) route OPTIMIZER must skip locked trips — geometry recompute still
  runs. Trip overview shows a "manual order — optimization off" note. Verified
  schema+seed (route_locked=f default); mobile build clean.

### Route-optimization capacity foundation (engine deferred)
- Per user: the AI route-optimization **engine choice is deferred** (researching
  OR-Tools vs HERE Tour Planning vs LLM-as-caller) — recorded in TODO.
- Laid the data foundation: capacity is **two dimensions, per tractor/trailer
  config** — added `equipment.weight_capacity_lbs` (deck_length_ft already there)
  and `load.deck_feet` + `load.weight_lbs`. Seeded realistic values; **verified**
  schema+seed on PG16. Mobile `Load` type updated. (ALS regen needed to expose.)

### Mutable per-trip GPS route — driver waypoints
- Stop types added: `lunch` (7) + `load_stop` (8) for multi-load stops (schema
  comment + seed; verified — 8 stop types). The `waypoint`/`route` tables already
  modeled per-trip ordered, mutable stops.
- Mobile: trip detail split for low clutter — **TripDetailPage** is a compact
  overview (route summary: distance/ETA + read-only stops) that drills into a new
  **TripWaypointsPage** (`/trips/:tripId/waypoints`) to add a stop at the current
  GPS location by type (fuel / lunch / load stop / truck stop / waypoint) or
  remove one (swipe); origin/destination fixed; new stops insert before the
  destination (back-to-front seq bump to respect UNIQUE(trip_id, seq)).
- API/types: `Route` type; `updateWaypoint`, `deleteWaypoint`, `routeForTrip`.
- Realtime: seeded `waypoint` route + ALS `Waypoint` producer (insert/update,
  key = trip_id) so route edits stream live (`waypoints`/`trip:<id>`) — map
  updates as realtime data. mobile build clean; schema+seed verified.

### Realtime data plane: live data from the stream, not ALS reads
- Defined two planes (docs/REALTIME.md + CLAUDE): **realtime** (messages, fleet
  locations / map updates, live status) flows from the **Kafka stream via the
  bridge**; **everything else** (CRUD, lookups, snapshots, detail/forms, writes)
  stays **ALS JSON:API**. Kafka stream is now public infra — but clients integrate
  **through the bridge** (URL + JWT); brokers stay internal.
- Mobile: ChannelPage/ChannelsPage now **apply event payloads directly** from the
  stream (append message / bump unread) instead of re-reading via ALS; initial
  snapshot still via ALS. De-duped by id (absorbs the optimistic own-send).
- Desktop: `RealtimeClient` now also relays **position** events into a new
  `PositionBus`; `MapView` + `HudView` apply live fleet locations (upsert + render)
  with a 60 s ALS reconcile fallback. ALS producer payloads enriched
  (`reply_to_id` on message, `speed_mph` on position) so clients render live with
  no read-back. Bridge + snippet syntax-checked; mobile build clean.

### Consumer model: true pub/sub (unique group per bridge instance)
- Fixed the Kafka consumer-group semantics for fan-out: each bridge instance now
  uses a **unique** `group.id` by default (`KAFKA_GROUP_UNIQUE=true` → base +
  host/pid/uuid, computed once per process), so EVERY instance receives EVERY
  event and fans out to its own WebSocket clients. A shared group would be
  competing-consumer/queue semantics and would drop events per client across
  replicas.
- `KAFKA_GROUP_UNIQUE=false` opts into shared/queue mode (work distribution).
  Unique mode also disables offset commit (relay only wants live events). Bridge
  logs the group + mode. Independent services each use their own group (pub/sub):
  the bridge is one consumer; notifications/analytics/etc. can add their own.
- Docs: REALTIME "Consumer model — publish/subscribe"; .env documents
  KAFKA_GROUP_ID/KAFKA_GROUP_UNIQUE. Verified both modes.

### Seed realtime routes: load, trip, alert
- Bridge `DEFAULT_ROUTES` seeded with `load` (broadcast `loads`; scopes
  `driver:<id>`, `week:<id>`), `trip` (`trips`; `driver:<id>`), and `alert`
  (`alerts`; `driver:<id>`).
- ALS producers added for **Load** and **Trip** (`_PRODUCERS`), firing on insert
  OR update via a new `_is_change()` helper — so status/assignment changes stream
  live, keyed by the row's own id. `alert` route is ready; producer awaits an
  alert source. Bridge + snippet syntax-checked (routes load: message, position,
  load, trip, alert).

### Multi-topic ready: config-driven routing for new purposes
- Bridge routing is now **config-driven** (`realtime/app/config.py` →
  `DEFAULT_ROUTES`, overridable via `KAFKA_TOPIC_ROUTES` JSON env). Each topic maps
  to a `broadcast` key + optional `scopes` ({prefix, field}); `routing_keys()` is
  generic. **Unknown topics still work** (broadcast on their own name) so a
  producer can ship ahead of a route entry — no fan-out code change to add a
  purpose. Subscribed topics derive from the route map (or `KAFKA_TOPICS`).
- ALS producer made extensible: a `_PRODUCERS` list + `_emit()` helper; adding a
  type is one handler + one list entry (loads/trips/alerts… as the app evolves).
- Docs: REALTIME "Multiple topics, and adding one" guide; .env.example shows
  `KAFKA_TOPIC_ROUTES`. Bridge + snippet syntax-checked.

### Hide Kafka config — internal env files only
- Hardened `.gitignore`: ignore `.env` + `*.env`, keep only `*.env.example`
  placeholder templates (verified with `git check-ignore`). No real broker/secret
  is committed (audited: only `localhost:9092` / `change-me` placeholders).
- New `docs/REALTIME.md` → "Configuration & secrets": Kafka brokers/creds, the
  consumer group, and the JWT secret live ONLY in server-side env files
  (bridge `.env`, ALS project `.env`). **Clients never see Kafka** — they get only
  the bridge WebSocket URL + a JWT (the bridge is the encapsulation boundary).
- ALS producer config is env-driven (`KAFKA_CONNECT = os.getenv(...)`), not a
  hardcoded/committed broker; ALS project `.env` should be gitignored
  (als-extensions/README, MIDDLEWARE_SETUP updated).

### Desktop/HUD: realtime bridge WebSocket client → CommBus
- New `RealtimeClient` (`portals/dispatcher-desktop/src/`): one **server-wide**
  Boost.Beast WebSocket client that consumes the realtime bridge and publishes
  message events into `CommBus` (which already fans out to every session via Wt
  server push). Sync read loop on a worker thread with exponential-backoff
  reconnect; `stop()` closes the socket to unwind the blocking read.
- **Opt-in** via CMake `-DFD_REALTIME_CLIENT=ON` (needs Boost.Beast). Default OFF
  → Boost-less builds still work and the desktop falls back to intra-server
  `CommBus` + the 30 s reconcile poll. Configured by `FLEET_REALTIME_URL` +
  `FLEET_REALTIME_TOKEN` (a bridge service JWT); `main.cpp` uses start/wait/stop.
- `CommPanel` now **de-dups by message id** (a `std::set`), so a desktop user's
  own send (intra-`CommBus` + the bridge echo of the same row) renders once;
  toasts only for others.
- Not compiled in the sandbox; Beast client + cross-thread socket close are
  flagged for Linux build/verify. Positions→HUD/map is the next hook.

### Realtime: WebSocket bridge over ALS → Kafka
- New `realtime/` service (`docs/REALTIME.md`): a **confluent-kafka consumer →
  WebSocket relay**. ALS already produces domain events to Kafka
  (`Rule.after_flush_row_event` + mapper, per GenAI-Logic Sample-Integration);
  the bridge fans them out to clients. Stateless, JWT-authed (shared ALS secret),
  auto-reconnecting Kafka loop + WS heartbeats. Mirrors the `assistant/` layout
  (app/, requirements, Dockerfile, deploy/ systemd).
- **Topic strategy (decision):** one Kafka topic **per row type** (`message`,
  `position`) with **`channel_id` as a correlation id in the payload** and the
  Kafka **key = channel_id** for per-channel ordering — NOT a topic per channel
  (channels are dynamic UUID objects → topic explosion). Per-channel fan-out is
  done at the WebSocket layer (`channel:<id>` subscriptions).
- Mobile: `RealtimeClient` (exponential-backoff reconnect, re-subscribe on
  connect, status) + `RealtimeProvider`/`useRealtime`; wired into ChannelPage
  (live messages) and ChannelsPage (live unread). Accelerator only — initial
  load + pull-to-refresh remain the fallback. `npm run build` clean.
- Considered Postgres LISTEN/NOTIFY as the source but **reverted** it: ALS→Kafka
  is the canonical producer the team already uses; two sources = fragile.
- Robustness: DB/JSON:API stays canonical; Kafka durable; clients degrade to a
  reconcile poll if the bridge/Kafka is down.
- Desktop: still uses intra-server push (`CommBus` + Wt WebSockets); joining the
  external bridge via a WS client is the documented next step.

### Desktop: Fleet/Map/Settings views + comms WebSocket push
- Menu now: Board · Fleet · Map · New Load · Communications · Settings. The
  center work panel has no hide/show toggle — it swaps the visible view per menu
  item (left/right panels remain collapsible).
- `FleetView` — list of drivers (fetchDrivers) + equipment (unit numbers).
- `MapView` — geo-positioning of fleet locations: a Leaflet map (latest position
  per rig) + detail table, 15 s telemetry refresh (mirrors the HUD map).
- `SettingsView` — appearance (System/Light/Dark → data-fd-theme + localStorage,
  shared with the header toggle) + account/connection info.
- **Comms push/WebSockets**: new `CommBus` (in-process singleton like
  `HudControlBus`) broadcasts sent messages to every session via Wt server push;
  `CommPanel` now subscribes and renders pushed messages instantly (and toasts),
  publishes on send, and keeps only a 30 s reconcile poll for off-server (mobile)
  messages. New `wt_config.xml` enables `<web-sockets>true` for the push transport.
- Note: full cross-client realtime (mobile→desktop) needs the middleware to emit
  change events (SSE/WebSocket/broker) — backend follow-up (ALS is generated).
- **Not compiled in the sandbox**; new code reuses known-good Wt patterns
  (WLeafletMap from HudView, WServer::post bus from HudControlBus). CMake updated.

### Desktop dynamic shell: menu, communications rail, toasts
- Header: **logo top-left**; **profile (name) + logged-in role + Sign out
  top-right**, plus theme toggle and panel toggles. Panel toggles use the
  disclosure aesthetic — **▼ open**, **▶/◀ closed** (left/right).
- Left panel = **menuing system** (Board · New Load · Drivers · Communications)
  + **work-panel toggles** (Today | Week, Compact rows) that drive the center.
- Right panel = **Communications** (`CommPanel`): channel list + live conversation
  (10s `WTimer` poll) + composer. Selecting **Communications** in the menu makes
  comms take over the **full work area**.
- **Toaster** (`Toaster`): top-right transient alerts (new messages, save/create
  confirmations, errors). Single sweeper timer auto-dismisses; close marks for
  removal (no delete-during-callback). New-message toasts come from the rail only.
- ApiClient: `fetchChannels` / `fetchMessages` / `createMessage` (+ Channel,
  Message models). Server push (`enableUpdates`) already on → live feel.
- **Desktop not compiled in the sandbox**; new code uses plain Wt widgets +
  `WTimer` + `doJavaScript` (no version-sensitive APIs beyond those already
  flagged). Build on a Linux box to confirm.

### Shared design system across mobile, desktop, and HUD
- New `docs/DESIGN_SYSTEM.md` — one palette (subtle blues, sparse orange accent,
  white/light standard theme + complementary dark) as the source of truth,
  implemented as design tokens in each client. Default follows the OS
  (`prefers-color-scheme`); a manual override wins (persisted).
- Desktop (`docroot/css/fleet-dispatcher.css`): rewrote with `--fd-*` light/dark
  tokens, aligned Bootstrap primary to the brand blue, and added the **app frame**
  — full-width header (nav), three-column body (collapsible left panel · center
  work panel · collapsible right panel), full-width footer.
- Desktop `Shell` restructured into that frame: header with nav + theme toggle +
  panel show/hide toggles (`◧`/`◨`) + user·role + Sign out; left "Filters" panel;
  center command bar + content outlet; right "Details" inspector; footer
  (copyright + links). Active-nav gets the orange underline. Theme toggle flips
  `data-fd-theme` on `<html>` and persists to localStorage.
- HUD forces the dark theme (wall display) via `data-fd-theme="dark"`.
- Mobile: new `src/theme/variables.css` (Ionic vars from the same palette, light
  + dark) and `theme.ts` (System/Light/Dark, persisted, applied at startup);
  imported in `main.tsx`; **Appearance** picker added to Profile. Blue toolbars
  match the desktop nav. `npm run build` clean.
- **Desktop not compiled in the sandbox**; CSS is static, the Wt layout uses
  plain containers + `doJavaScript` for the theme toggle (no version-sensitive
  APIs added). Build on a Linux box to confirm.

### Desktop (Wt) console login + profile (same ALS JWT credentials)
- ApiClient: holds the bearer token (`setAuthToken`/`clearAuthToken`/`hasAuthToken`)
  and adds `Authorization: Bearer` to every GET/POST/PATCH; new `login()`
  (POST /api/auth/login via a plain-JSON helper), `fetchUserByUsername()`
  (GET /AppUser?filter[username]=), and `updateUser()` (PATCH /AppUser/{id} via a
  new patch helper). Added `parseUser`, `urlEncode`, `AppUser` model.
- New `LoginView` gates the console (DispatcherApp shows login → Shell on success;
  Shell's Sign out returns to login). New `ProfileView` (view/edit profile,
  PATCH). Shell now takes `(ApiClient*, AppUser, onLogout)`, shows name·role +
  Sign out in the app bar, and a Profile nav item. CMake gains the two sources.
- Token lives **server-side** in the Wt session (not in the browser). HUD entry
  point stays unauthenticated for now (wall display) — noted for a service token.
- **Not compiled in the sandbox** (no Wt). Version-sensitive spots flagged:
  `Http::Method::Patch` (older Wt may lack PATCH → PUT fallback) alongside the
  existing `done()` error_code / `Json::Type::Null` notes. Avatar upload deferred.

### Authentication + user profile (ALS built-in JWT, mobile-first)
- Schema: `app_user` gains `password_hash` (werkzeug pbkdf2:sha256) + profile
  fields (`phone`, `title`, `timezone`, `avatar_document_id` → CMS `document`,
  `last_login_at`). Avatar FK added after `document` (ordering). Seeded the three
  demo users with password `fleet123` (hashes verified) + titles/phones.
  **Verified** schema+seed on PostgreSQL 16; avatar FK present.
- Mobile: ALS JWT auth. New `auth/AuthContext` (`useAuth()` → user, driverId,
  equipmentId, token, login/logout), `api/auth.ts` login (POST /api/auth/login),
  bearer header on every client call + 401/403 auto-logout, `LoginPage` gate
  (shown until authed), `ProfilePage` (view/edit + avatar upload via CMS + sign
  out), profile button in the Loads header. Replaced the `currentUser.ts`
  identity placeholder with `useAuth()` across Channels/Channel/Saved/Locate/
  Trips/Assistant (only the `PHONE_PUSH_SOURCE_ID` lookup constant remains).
- Docs: new `AUTHENTICATION.md` (ALS add-auth + auth_provider sketch verifying
  the werkzeug hash, login endpoint, JWT secret, roles, token storage); TODO,
  domain-model. Token in localStorage for now (Capacitor Secure Storage = TODO).
- **Needs ALS regeneration** so `AppUser` exposes the new columns + the
  AppUser↔Document avatar relationship. Mobile `npm run build` clean.

### Feature 1 — reply-to-message (threaded quotes)
- Mobile: swipe a message → **Reply**; the composer shows a "Replying to …"
  banner with a one-line snippet of the original + cancel, and the sent message
  renders a quoted snippet (left-bar) above its body. Uses the existing
  `message.reply_to_id` self-reference (no schema change); `createMessage` gained
  an optional `replyToId`. Quotes resolve client-side from the loaded timeline
  (falls back to "quoted message" if the original isn't loaded).
- Verified: mobile `npm run build` clean.

### Feature 1 — emoji support in the messaging composer
- Mobile: a dependency-free `EmojiPicker` (curated set incl. trucking symbols:
  🚛 ⛽ 📍 🛣️ …) opened from a composer button (IonPopover); tapping appends to
  the draft. No emoji library — keeps the bundle lean; emojis are plain UTF-8
  text so display/storage need nothing special.
- Verified the storage/transport path round-trips a 4-byte emoji through
  `message.body` (TEXT) on a UTF8 PostgreSQL 16 cluster (24 chars / 32 bytes
  preserved); JSON:API passes UTF-8 through unchanged — no schema change.
- Desktop messaging view (not yet built) will get emoji for free: Wt's
  WText/WLineEdit are UTF-8 native.

### Feature 1 (Phase 2) — pinned messages + personal saved archive
- Schema: `pin_scope` lookup (`self`/`channel`/`everyone`) + `message_pin`
  (user-selectable visibility per pin; `channel_id` carried for direct filtering;
  one pin per (message, user)) + `saved_message` (per-user cross-channel archive
  with an optional note). +3 tables → **40** in `fleet`. Seeded a channel-scope
  pin and a saved message; **verified** schema+seed on PostgreSQL 16.
- Mobile: pin a message via a scope picker (Only me / Channel / Everyone), a
  "Pinned" strip atop the channel showing visible pins + scope, swipe actions to
  pin-unpin / save-unsave, and inline pin/saved markers. New personal **Saved**
  view (cross-channel archive) reached from the Message Board header.
- API client: `MessagePin`/`SavedMessage` types, `PIN_SCOPE` ids, pin/unpin/
  repin, visible-pins filter (self pins only for the pinner), save/unsave,
  single-message getter; added a JSON:API DELETE helper.
- Note: pin-visibility is filtered **client-side** for now — a server-side rule
  (auth/LogicBank) is the production approach. Mobile `npm run build` clean.

### Feature 3 — "Hey Dispatch" driver voice assistant (pluggable AI provider)
- New `assistant/` FastAPI service (port `5710`): `POST /assist` takes an
  on-device STT transcript + the driver's trusted context and returns a spoken
  reply + the actions it took; `GET /health` reports provider/model/config.
- Tool use (5 intents): `send_message_to_dispatcher` (→ ALS JSON:API `Message`),
  `get_eta`, `validate_route`, `alternate_routes` (→ HERE truck routing, with
  straight-line / "not configured" fallbacks), `tech_help` (built-in KB). Trusted
  context (driver, position, destination, truck profile) comes from the request,
  never the model.
- **Pluggable AI provider** (`app/providers/`): admin picks via
  `ASSISTANT_PROVIDER` — `anthropic` (default; adaptive thinking, effort,
  prompt-cached system+tools prefix), `openai`, or `ollama` (OpenAI-compatible
  adapter, local base URL). SDK imports are lazy, so only the chosen provider's
  package is loaded. The brain (`app/assistant.py`) owns the prompt + tool
  dispatch/action-recording and delegates the model loop to the provider.
- Mobile: new **Dispatch** tab (`AssistantPage`) — push-to-talk via Web Speech
  API (STT) → `/assist` → `speechSynthesis` (TTS); pulls the dispatcher channel +
  active-trip destination for context. New `VITE_ASSISTANT_BASE_URL`, assistant
  API client, and minimal `SpeechRecognition` typings.
- Verified: service `py_compile`s and imports without the AI SDKs (lazy);
  mobile `npm run build` clean. Docs: DEPLOYMENT (port table + step 8), TODO,
  new `AI_ASSISTANT.md`.

## 2026-06-02

### Shared-instance schema move + geospatial endpoint + one-shot (verified)
- Shared PostgreSQL instance (with Smitty / Student-Onboarding): moved all Fleet
  app objects out of `public` into a dedicated **`fleet`** schema (owned by
  `fleet`; ALS reflects it via `search_path = fleet, public`). `schema.sql` /
  `seed_data.sql` set the search_path; resource names unchanged (table-derived).
- PostGIS in a **shared `gis`** schema: Fleet owns only its `fleet_*` geography
  views via the new **`fleet_gis`** role (granted CREATE on gis, SELECT on the
  fleet lat/lng tables); extension objects stay superuser-owned (by design).
- `scripts/db-setup.sh` — one-shot for steps 1–4 (role+db, schema, seed, gis via
  admin since CREATE EXTENSION needs superuser).
- `geospatial/` — FastAPI endpoint (/health, /truck-stops/nearest, /trucks/near,
  /positions/latest) connecting as `fleet_gis`; Dockerfile + README.
- **Verified on PG16 + PostGIS 3.4:** one-shot creates `fleet` (37 tables) with
  `public` empty; `fleet_*` views owned by `fleet_gis`; the FastAPI endpoint
  served all four routes live (nearest truck stop, latest positions, trucks-near).
- Synced docs: DEPLOYMENT, MIDDLEWARE_SETUP (schema layout = DECIDED),
  SPATIAL_GIS_DATA_CONSIDERATIONS, TODO.

### Deployment runbook + PostGIS bootstrap (verified)
- `docs/DEPLOYMENT.md`: end-to-end standup — PostgreSQL, schema/seed, PostGIS
  (without breaking ALS), ALS create/run, geospatial-endpoint role, portals,
  verification + troubleshooting.
- `database/gis_bootstrap.sql`: installs PostGIS into a dedicated `gis` schema
  (+ `position_geog`/`poi_geog` geography views).
- Verified on PostgreSQL 16 + PostGIS 3.4: `public` ends with **0** PostGIS
  objects (so ALS reflection stays clean), `spatial_ref_sys` lives in `gis`, and
  spatial queries (nearest-POI KNN, geography distance) work with
  `search_path = gis, public`. docker-compose notes the `postgis/postgis` image.

### HUD: Leaflet map of truck positions
- HUD now renders a `Wt::WLeafletMap` (OSM tiles, no key) with a marker per
  rig's latest position, rebuilt on the 15s poll (markers tracked + removed/
  re-added); the detail table remains below. Leaflet JS/CSS via CDN (note for
  offline/self-contained deploy). Needs Wt ≥ 4.5.

### Feature 2: navigation schema + mobile Trips
- Schema: `trip`, `waypoint` (ordered, unique seq), `point_of_interest`
  (incl. truck stops via category), `route` (HERE polyline as text) + lookups
  `trip_status`/`stop_type`/`poi_category` — ALS-facing lat/lng, no PostGIS.
  Seed: a Lubbock→Denver trip with 3 waypoints, a truck-stop POI, a route.
  Verified on PostgreSQL 16 (ordered waypoints + unique-seq guard).
- Mobile: **Trips** tab — list driver trips, start a (planned) trip, trip
  detail with ordered waypoints and "add current location" (geolocation →
  addWaypoint). Client: tripsForDriver/getTrip/createTrip/waypointsForTrip/
  addWaypoint. `npm run build` passes.
- Pending: trip start/stop lifecycle, HERE routing/navigation, POIs browsing,
  HUD map tiles.

### Feature 2: truck-location backbone (telemetry)
- Schema: `location_source` lookup + `position_report` (lat/lng numeric,
  heading/speed/accuracy, recorded_at; equipment/driver FKs) per the geospatial
  decision — ALS-facing, no PostGIS. Seed: 3 sources + a short track. Verified on
  PostgreSQL 16 (latest-per-rig query + lat range CHECK).
- Mobile: **Locate** tab — `navigator.geolocation` → `api.postPosition` (POST
  `/PositionReport`, phone_push). Placeholder driver/equipment ids in
  `currentUser.ts` pending auth. `npm run build` passes.
- Desktop HUD: `ApiClient::fetchPositions` + a **truck-locations panel** (latest
  position per rig, refreshed every 15s via `WTimer`).
- PostGIS `gis` schema, the geospatial endpoint, HERE routing, and HUD map tiles
  remain queued.

### Dispatcher desktop: /hud surface + HudControlBus
- Schema: `hud_command` append-only command channel (free-text `command_type` +
  `arg`) for distributed/remote HUDs. Verified on PostgreSQL 16.
- C++/Wt: `HudControlBus` (server-push singleton: subscribe/unsubscribe/publish
  via `WServer::post`), `HudView` (read-only board at `/hud` that reacts to
  `SetMode`), and a second WServer entry point (`/hud`) alongside the console.
- The console's Today/Week toggle now publishes `SetMode` to the bus (instant,
  in-process) and records it via `ApiClient::postHudCommand` (POST /HudCommand)
  for remote HUDs. `FocusDriver`/`HighlightLoad` enum + handlers stubbed.
- README: HUD served at `/hud`.

### Dispatcher desktop: load intake form + assignment
- `LoadForm` (Wt): combos for dispatch week, shipper, receiver, commodity,
  pickup/drop-off locations, run type, status, plus optional driver + equipment
  (assignment); rate/miles spin boxes; pickup/delivery date edits. Validates
  required fields and distinct pickup/drop-off, then POSTs.
- `ApiClient`: `Option`/`LoadDraft`, `fetchOptions` (generic reference combos),
  and `createLoad` (POST /Load, hand-built JSON:API body via `postJson`).
- Shell: "New Load" nav → form; on success returns to the board (which reloads).
  CMake builds `LoadForm.cpp`.

### Dispatcher desktop: shell + Today/Week board (build start)
- Schema: added `load.pickup_date` / `delivery_date` (+ `delivery_after_pickup`
  CHECK, pickup-date index) to drive the board; seed updated. Verified on
  PostgreSQL 16. Synced the mobile `Load` type.
- C++/Wt: `Shell` (app bar, nav, Today|Week command bar, content outlet),
  `BoardView` (Today list vs Mon→Mon grid), and an async `ApiClient`
  (Wt::Http::Client + Wt::Json) reading `Driver`/`Load`. `main.cpp` enables
  server push; CMake builds the new sources.
- Decision: HUD will be **distributed-capable** — commands also persisted as a
  `hud_command` JSON:API resource (built with the bus, later).
- NOT compiled here (no Wt in the sandbox); version-sensitive lines flagged in
  `ApiClient.cpp` and the Bootstrap-5 theme.

### Dispatcher Desktop & HUD design
- Added `docs/DISPATCHER_DESKTOP.md`: the app **shell**, the **Today | Week**
  board modes, and the **control → command → HUD** mechanism — a Wt server-push
  `HudControlBus` where the control console publishes commands and HUD sessions
  auto-react (generalizable to `FocusDriver`, `HighlightLoad`, …). Notes the
  same-server (in-process bus) vs distributed (`hud_command` JSON:API) options.
- TODO desktop section refined accordingly.

### Desktop build/run procedure + systemd
- Expanded the dispatcher-desktop README with Linux dependency install
  (Debian/Ubuntu `libwt-dev`/`libwthttp-dev`, Fedora `wt-devel`), a Wt ≥ 4.5
  note for the Bootstrap 5 theme, configure/build/run, install-prefix run, and a
  systemd service procedure.
- Added `deploy/fleet-dispatcher-desktop.service` (hardened unit, runs with the
  install-prefix docroot so Wt `resources/` resolve) and `deploy/desktop.env.example`.

### Messaging Phase 1 CLOSED
- Finished the unblocked Phase-1 items: **unread counts + mark-as-read**
  (`channel_member.last_read_at`, `IonBadge`), **pull-to-refresh**
  (`IonRefresher`) on channel list + detail, and **attachment-load optimization**
  (sparse fieldsets via `getDocumentMeta`; full `data` fetched lazily on tap).
- Extracted the placeholder identity to `src/currentUser.ts`.
- Carried over to their owners: channel/DM creation + membership and current-user
  (→ auth), clickable toasts + realtime delivery (→ realtime), desktop messaging
  view (→ desktop), server-side `unread_count` (→ LogicBank).
- TODO: added the **Dispatcher Desktop Portal** section as the next pivot.
- Verified: mobile `npm run build` (tsc + Vite) passes.

### Messaging: attachment upload + list→detail navigation
- Restructured mobile nav to **list → detail** with native transitions and
  `IonBackButton` (KISS): `ChannelsPage` + `ChannelPage` (replacing the combined
  `MessagesPage`), and `LoadsPage` + `LoadDetailPage`; routes nested per tab.
- Attachment **upload**: file picker → base64 → create message + CMS `document`
  + `message_document` link. Attachments render as chips; tapping opens a
  base64 data URL (preview/download).
- Client: `getOne`, `getLoad`, `getChannel`, `getDocument`, `createDocument`,
  `linkMessageDocument`, `attachmentsForMessage`.
- Added `docs/MOBILE_UI_WIDGETS.md` — Ionic 8 widget vocabulary for UX language.
- Captured the planned **clickable message toast** pattern (deep-links to a
  channel; needs realtime) in TODO + the widget doc.
- Verified: mobile `npm run build` (tsc + Vite) passes.

### Merged the geospatial docs into one
- Combined the former `GEOSPATIAL.md` (decision + strategy) into
  `SPATIAL_GIS_DATA_CONSIDERATIONS.md` as the single source; removed
  `GEOSPATIAL.md`; repointed references in `domain-model.md` and `TODO.md`.

### GIS considerations doc
- Added `docs/SPATIAL_GIS_DATA_CONSIDERATIONS.md` — detailed considerations
  (ALS/PostGIS conflict internals, data model, indexing, geospatial endpoint,
  HERE, ingestion, ops/security, bootstrap), originally a companion to the
  geospatial strategy doc (later merged in).

### Decision ACCEPTED: separate PostGIS spatial data from ALS APIs
- Locked in the two-path approach: relational map data via ALS, spatial (PostGIS)
  via a dedicated endpoint over a `gis` schema. Accepted the added standup cost.
- Flipped `SPATIAL_GIS_DATA_CONSIDERATIONS.md` status to DECIDED; noted standup cost; marked the
  Feature 2 TODO section accordingly.

### Decision captured: geospatial / PostGIS strategy
- Added `docs/SPATIAL_GIS_DATA_CONSIDERATIONS.md`: to avoid the ALS↔PostGIS conflicts (PostGIS
  metadata reflected as junk resources; geometry types unmappable), use two
  access paths — relational map data (trips, waypoints, POIs, truck-stops,
  routes, position_report) via ALS with **plain lat/lng numeric** columns, and a
  **separate geospatial endpoint** for PostGIS `ST_*` over a dedicated `gis`
  schema (PostGIS installed there; excluded from ALS reflection). HERE handles
  truck-legal routing. Linked from `domain-model.md`; TODO items refined.

### Decision captured: multi-schema database layout
- Noted ALS multi-schema support. Documented a candidate split (peel `cms`
  out from `app`/ops; `telemetry` later) in `MIDDLEWARE_SETUP.md` and added a
  decision item to `TODO.md`. No DDL change yet — to apply when the DB is
  configured. Cross-schema FKs are supported, so relationships still hold.

### Messaging: send-message composer
- Mobile `MessagesPage` gains a composer (input + send) backed by a new
  `createMessage` JSON:API client helper (POST `/Message`).
- Current author is a hardcoded placeholder pending auth (tracked in TODO).
- Verified: mobile `npm run build` (tsc + Vite) passes.

### Project tracking
- Added `docs/TODO.md` (sequenced plan) and `docs/DEVELOPMENT_LOG.md` (this
  file), mirroring the workflow used in the sibling projects.

### Messaging & Content (CMS) — feature 1, slice 1
- Schema + seed: `channel`, `channel_member`, `message`; lookups `channel_type`,
  `channel_member_role`, `document_type`.
- Attachments modeled as a reusable **CMS `document`** (bytea + metadata) linked
  via FK join tables (`message_document`, `load_document`); the seed reuses one
  BOL document from both a message and a load.
- Docs: Messaging + CMS contexts; "Planned: Truck Location & Dispatcher HUD"
  (source-agnostic position ingestion — AirTag / Google / phone-push, PostGIS).
- Mobile: Channel/Message/Document types + client methods, `MessagesPage`,
  tabbed nav (Loads · Messages).
- Verified on PostgreSQL 16: schema + seed apply; document round-trips as text,
  linked to 1 message + 1 load.

### Middleware offloaded
- Moved the middleware README into `docs/MIDDLEWARE_SETUP.md`; removed the
  `middleware/` folder; trimmed `docker-compose.yml` to the `db` service.

### Mobile module publishing
- `scripts/publish-mobile.sh` + `Makefile` (`publish-mobile`, `pull-mobile`,
  `mobile-build`) for git-subtree publishing to `Fleet-Dispatcher-Mobile`.
- Exposed `publish-mobile` as a CMake target (+ opt-in `-DFLEET_PUBLISH_MOBILE`);
  default remote URL set to HTTPS.

## 2026-06-01

### SQL restructure for ALS
- Replaced Postgres ENUM types with lookup tables (sequential int keys); domain
  objects use GUID keys; all relationships are FK constraints. `driver_type`
  carries `default_percent` / `owner_bears_costs`. Renamed seed → `seed_data.sql`.
- Validated end-to-end on PostgreSQL 16.

### Mobile portal made buildable
- Added the app shell (index.html, main.tsx, App.tsx, LoadsPage), env typing,
  and a self-contained config; aligned `api/types.ts` to the lookup schema.

### Desktop (C++/Wt) theming + resources
- Applied `WBootstrap5Theme`; CMake auto-detects and deploys the entire Wt
  `resources/` tree beside the binary (+ install rules).

### Initial scaffold
- Three-tier DDD project: docs (architecture, domain model), schema + seed,
  middleware setup notes, and both portal scaffolds. (The standalone `domain/`
  Python package was later dropped in favor of schema + docs as the model.)

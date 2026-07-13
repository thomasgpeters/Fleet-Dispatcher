<!--
  Fleet Dispatcher — Manual Test Plan.
  Hard page breaks (the <div style="page-break-before:always"> blocks) render as
  real page breaks when this file is printed or converted to PDF/DOCX:
    • Browser: open the rendered HTML and File > Print (Chrome/Edge honor the CSS).
    • pandoc:  pandoc docs/TEST_PLAN.md -o TEST_PLAN.pdf   (via wkhtmltopdf/weasyprint)
    • VS Code: "Markdown PDF" extension respects page-break-before.
-->

# Fleet Dispatcher — Manual Test Plan

**Version:** 1.0 &nbsp;·&nbsp; **Applies to:** Feature-4 comms build (broadcast lock ·
mute/ban · topics · pins/Saved · attachments) on the ALS JSON:API + desktop
console/HUD + mobile app.

## Purpose

Role-based, hand-executable acceptance tests. Each **section** covers one role
(or role axis); each **case** (numbered `x.y`) is one scenario with expanded
numbered **steps** (`x.y.z`), an expected result, and a line to record the
outcome. Print one section per tester.

## Roles under test

Fleet Dispatcher has two independent role axes. Most cases exercise the
**application role**; §5 exercises the **comms membership role**, which gates the
message board independently.

| Axis | Values | Where defined | Governs |
| --- | --- | --- | --- |
| **Application role** | Dispatcher (1) · Driver (2) · Updater (3) | `fleet.app_role` → `app_user.app_role_id` | What client/features a login sees |
| **Comms member role** | owner (1) · member (2) · admin (3) | `fleet.channel_member.member_role_id` | Posting/topic/moderation rights per channel |
| **Comms standing** | active (1) · muted (2) · banned (3) | `fleet.channel_member.member_status_id` (+ `restricted_until`) | Whether a member may post |

## Test users (demo seed — password `fleet123`)

| Username | Name | App role | Notes |
| --- | --- | --- | --- |
| `dispatch1` | Dana Dispatcher | Dispatcher | Console power user; owner/admin of demo channels |
| `driver1` | Pat Diesel | Driver | Linked to a `Driver` record (`driver.user_id`) → driver tabs |
| `updater1` | Uma Updater | Updater | No driver record; status maintenance only |

> These are demo credentials. Change them for any real deployment.

## Environment / endpoints

| Surface | URL (default) |
| --- | --- |
| ApiLogicServer JSON:API | `http://localhost:5659/api` |
| Login endpoint | `POST http://localhost:5659/api/auth/login` → `{"access_token": "…"}` |
| Dispatcher desktop console | `http://<host>:8089/` |
| Dispatcher HUD (map) | `http://<host>:8089/hud` |
| Mobile app | dev: `npm run dev` in `portals/mobile`; prod: served by VCP |

**Server-side check helper** — several cases verify a rule server-side (not just
the UI). Grab a token first, then reuse `$TOKEN`:

```bash
TOKEN=$(curl -sS -X POST http://localhost:5659/api/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"dispatch1","password":"fleet123"}' \
  | sed -n 's/.*"access_token":"\([^"]*\)".*/\1/p')
```

## How to record a result

Each case ends with a result line. Mark one box and add notes:

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

**Legend:** *Pass* = expected result observed. *Fail* = wrong/missing behavior
(note actual). *Blocked* = couldn't run (note why, e.g. dependency down).

<div style="page-break-before: always;"></div>

## 1. Authentication & session *(all roles)*

Run each case for **all three** logins (`dispatch1`, `driver1`, `updater1`)
unless a case names a specific user.

### 1.1 — Successful login

**Client:** Any (console / mobile). &nbsp; **Precondition:** Services up; user is `active`.

1.1.1 Open the client's login screen.
1.1.2 Enter the username and password `fleet123`.
1.1.3 Submit.
1.1.4 Observe the landing view and that a session persists on refresh.

**Expected:** HTTP 200; a bearer token is stored; the user lands on their role's
home surface; a page refresh keeps them signed in (no re-prompt).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 1.2 — Wrong password is rejected generically

**Client:** Any. &nbsp; **Precondition:** Valid username.

1.2.1 Open the login screen.
1.2.2 Enter a valid username with an **incorrect** password.
1.2.3 Submit.
1.2.4 Read the error message.

**Expected:** Login fails with a **generic** "incorrect username or password"
(does not reveal whether the username exists); no token issued; user stays on the
login screen.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 1.3 — Inactive account gets a clear message

**Client:** Any. &nbsp; **Precondition:** A user with `active = false` (temporarily
disable a demo user in `app_user`, or use a known inactive account).

1.3.1 Open the login screen.
1.3.2 Enter the inactive user's **correct** credentials.
1.3.3 Submit.
1.3.4 Read the error message.
1.3.5 Re-enable the account afterward (`UPDATE app_user SET active=true …`).

**Expected:** Login is refused with a message indicating the **account is no
longer active** (distinct from the generic bad-password error).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 1.4 — Expired / invalid token triggers auto-logout

**Client:** Any. &nbsp; **Precondition:** Logged in.

1.4.1 Sign in and confirm an authed view loads.
1.4.2 Invalidate the session (wait for token expiry, or clear/tamper the stored token).
1.4.3 Trigger any data-loading action.
1.4.4 Observe the response.

**Expected:** The API returns 401/403; the client trips its unauthorized handler
and **auto-logs-out** to the login screen (no silent broken state).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 1.5 — Logout clears the session

**Client:** Any. &nbsp; **Precondition:** Logged in.

1.5.1 Choose Log out / Sign out.
1.5.2 Confirm the login screen appears.
1.5.3 Press the browser Back button (or re-open the app).

**Expected:** Token is cleared; Back does **not** restore an authenticated view.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 1.6 — Unauthenticated API access is blocked

**Client:** curl. &nbsp; **Precondition:** Security enabled on ALS.

1.6.1 Call a protected resource with **no** bearer:
```bash
curl -sS -o /dev/null -w '%{http_code}\n' http://localhost:5659/api/Driver
```
1.6.2 Note the status code.

**Expected:** `401` (request rejected; security is enforced, not merely UI-gated).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

<div style="page-break-before: always;"></div>

## 2. Dispatcher role *(`dispatch1`)*

The console power user: fleet ops, scheduling, HUD, and full comms authority.
Run on the **desktop console** (`:8089`) unless noted.

### 2.1 — Console home & week board render

**Client:** Desktop console. &nbsp; **Precondition:** Logged in as `dispatch1`; loads seeded.

2.1.1 Log in and open the **Board** view.
2.1.2 Inspect the week grid header.
2.1.3 Confirm the week starts on **Sunday**.
2.1.4 On a touch device (or narrow width), confirm day labels collapse to **single letters** (S M T W T F S) and dates stay legible on the dark panel.
2.1.5 Confirm scheduled loads appear in the correct day columns.

**Expected:** Week grid renders Sunday-first without column collapse; touch/narrow
widths show single-letter days and readable dates; loads populate the grid.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.2 — Create a load

**Client:** Desktop console. &nbsp; **Precondition:** Logged in as `dispatch1`.

2.2.1 Open the load create form.
2.2.2 Enter customer, commodity/category, pickup and drop details.
2.2.3 Save.
2.2.4 Return to the Board and locate the new load.
2.2.5 (Optional) Confirm via API: `curl -H "Authorization: Bearer $TOKEN" http://localhost:5659/api/Load`.

**Expected:** The load persists, appears on the Board, and is returned by the API.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.3 — Assign a load to a driver + equipment

**Client:** Desktop console. &nbsp; **Precondition:** A load and at least one driver (`Pat Diesel`) + equipment exist.

2.3.1 Open an unassigned load.
2.3.2 Assign it to driver **Pat Diesel**.
2.3.3 Select an eligible equipment/rig.
2.3.4 Save the assignment.
2.3.5 Note the load id for cross-check **3.2**.

**Expected:** Assignment saves; the load shows as assigned to Pat + the chosen rig.
(Driver visibility is verified in **3.2**.)

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.4 — Build a trip with ordered waypoints

**Client:** Desktop console. &nbsp; **Precondition:** An assigned load (**2.3**).

2.4.1 From the assigned load, create a trip.
2.4.2 Add at least two waypoints (e.g. pickup, drop) in sequence.
2.4.3 Save.
2.4.4 Reopen the trip and confirm waypoint order.

**Expected:** Trip and its waypoints persist in the entered sequence.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.5 — HUD map renders (no white screen)

**Client:** HUD (`:8089/hud`). &nbsp; **Precondition:** Logged in; `wt_config.xml` Leaflet URLs configured; fleet positions seeded.

2.5.1 Navigate to `/hud`.
2.5.2 Wait for the map to initialize.
2.5.3 Confirm base tiles load (not a blank/white page).
2.5.4 Confirm fleet position markers plot on the map.

**Expected:** Leaflet map renders with tiles and fleet markers; no white screen /
fatal `WLeafletMap` error.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.6 — Communications: full view toggle & animation

**Client:** Desktop console. &nbsp; **Precondition:** Logged in.

2.6.1 With the right comms rail visible, select the **Communications** menu item.
2.6.2 Observe the right rail hide and the full Channels (Groups) view animate in.
2.6.3 Confirm the comm-panel **header with the full action set** is shown.
2.6.4 Toggle back and confirm the reverse animation and the rail's reduced action set.

**Expected:** Rail hides on entering full view; transition animates smoothly (or
respects reduced-motion); header/action sets match the layout (full vs rail).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.7 — Post, reply, emoji, pin, save

**Client:** Desktop console (comms). &nbsp; **Precondition:** `dispatch1` is a member of a standard channel.

2.7.1 Select a channel; type and send a message.
2.7.2 Hover a message and start a **reply**; send it and confirm the "Replying to…" banner cleared.
2.7.3 Add an **emoji** from the picker to a new message.
2.7.4 **Pin** a message; choose scope **Channel** (or Everyone).
2.7.5 Confirm the pinned message appears in the visible-pins strip atop the conversation.
2.7.6 **Save** a message.
2.7.7 Open the left-menu **Saved** view and confirm the saved message is listed.

**Expected:** All actions succeed and render; pin shows in the strip; Saved shows
the message in the cross-channel archive.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.8 — Create a topic (dispatcher is allowed)

**Client:** Desktop console (comms). &nbsp; **Precondition:** A channel selected.

2.8.1 With a channel open, use **"+ New"** in the topic bar.
2.8.2 Name and create the topic.
2.8.3 Confirm the new topic chip appears and can be selected to filter the stream.

**Expected:** Topic creation is **allowed** (dispatcher app-role qualifies); the
topic chip appears and filters messages.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.9 — Post to a broadcast channel (dispatcher/admin allowed)

**Client:** Desktop console (comms). &nbsp; **Precondition:** "Fleet Announcements" broadcast channel; `dispatch1` is owner/admin.

2.9.1 Open **Fleet Announcements**.
2.9.2 Confirm the composer is visible (not read-only).
2.9.3 Post a message.

**Expected:** Posting **succeeds** for the owner/admin (contrast with **3.6**,
where a plain member is blocked).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.10 — Moderate a member (mute / ban)

**Client:** Desktop console (comms). &nbsp; **Precondition:** A channel where `dispatch1` is owner/admin and another member (e.g. `driver1`) exists.

2.10.1 Open the channel's member directory.
2.10.2 Set another member's standing to **muted** (or banned); optionally set `restricted_until` in the future.
2.10.3 Save.
2.10.4 Cross-check the effect in **5.3** (that member can no longer post).

**Expected:** Standing change persists; the affected member's posting is blocked
(verified in §5).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.11 — Per-board export

**Client:** Desktop console (comms, full view). &nbsp; **Precondition:** A channel with messages, members, and topics.

2.11.1 Open the channel in full view.
2.11.2 Trigger **Export**.
2.11.3 Confirm the export status feedback and that a bundle downloads.
2.11.4 Open the bundle and confirm it contains messages, members, and topics.

**Expected:** A per-board bundle downloads containing the channel's messages,
members, and topics.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.12 — Archive & revive a channel

**Client:** Desktop console / API. &nbsp; **Precondition:** `dispatch1` is admin; an archivable channel exists.

2.12.1 Archive a channel.
2.12.2 Confirm it is **hidden from regular users'** channel lists (verify as `driver1` if possible).
2.12.3 As `dispatch1` (admin), **revive** the archived channel.
2.12.4 Confirm it becomes visible again.

**Expected:** Archived channels disappear for regular users; an admin can revive
them and they reappear.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 2.13 — Attachments (upload & open)

**Client:** Desktop console (comms). &nbsp; **Precondition:** A channel the dispatcher can post to.

2.13.1 In the composer, choose a file to attach (e.g. a small image and a PDF).
2.13.2 Confirm the file auto-uploads and the message posts with an attachment chip.
2.13.3 Click the attachment chip.
2.13.4 Confirm the document opens in a new tab.

**Expected:** File uploads (base64 → `Document` + `MessageDocument` link); an
attachment chip renders on the message; clicking it opens the full document.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

<div style="page-break-before: always;"></div>

## 3. Driver role *(`driver1`)*

Mobile-first. Sees **only their own** assigned work and updates status in the
field. Run on the **mobile app** unless noted.

### 3.1 — Driver login resolves driver context

**Client:** Mobile. &nbsp; **Precondition:** `driver1` linked via `driver.user_id`.

3.1.1 Log in as `driver1` / `fleet123`.
3.1.2 Confirm the signed-in **Driver** record (Pat Diesel) resolves.
3.1.3 Confirm assigned equipment resolves.
3.1.4 Confirm the driver-only tabs/surfaces are present.

**Expected:** Login resolves the linked Driver + equipment; driver tabs appear.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 3.2 — Driver sees only own assigned loads

**Client:** Mobile. &nbsp; **Precondition:** Dispatcher assigned a load to Pat in **2.3**; other drivers' loads exist.

3.2.1 Open **My Loads** (or equivalent).
3.2.2 Confirm the load assigned in **2.3** is listed.
3.2.3 Confirm loads assigned to **other** drivers are **not** shown.

**Expected:** Only the signed-in driver's assigned loads/trips are visible.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 3.3 — Advance trip status

**Client:** Mobile. &nbsp; **Precondition:** An active trip assigned to Pat.

3.3.1 Open the active trip.
3.3.2 Advance its status (e.g. en-route → arrived).
3.3.3 Confirm the update saves.
3.3.4 (Cross-check) As `dispatch1`, confirm the new status is reflected.

**Expected:** Status write succeeds and is reflected back to dispatch.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 3.4 — Complete a waypoint

**Client:** Mobile. &nbsp; **Precondition:** A trip with ordered waypoints (**2.4**).

3.4.1 Open the trip's waypoint list.
3.4.2 Mark the next waypoint complete.
3.4.3 Confirm the status updates and the sequence advances correctly.

**Expected:** Waypoint status updates in order.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 3.5 — Post in a standard channel

**Client:** Mobile (comms). &nbsp; **Precondition:** `driver1` is a member (role = member) of a standard channel.

3.5.1 Open a standard channel.
3.5.2 Confirm the composer is visible.
3.5.3 Post a message and confirm it appears.

**Expected:** A regular member can read and post in a standard channel.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 3.6 — Broadcast channel is read-only for a plain member

**Client:** Mobile (comms) **+ API**. &nbsp; **Precondition:** `driver1` is a plain member of "Fleet Announcements" (broadcast).

3.6.1 Open **Fleet Announcements** in the app.
3.6.2 Confirm the composer is **hidden** (read-only UI) with a notice explaining why.
3.6.3 Attempt a post **directly via the API** to prove server-side enforcement:
```bash
# as driver1's token; expect a rejection, not a 201
curl -sS -o /dev/null -w '%{http_code}\n' -X POST \
  -H "Authorization: Bearer $DRIVER_TOKEN" -H 'Content-Type: application/vnd.api+json' \
  http://localhost:5659/api/Message -d '{"data":{"type":"Message","attributes":{"channel_id":"<broadcast-id>","body":"test"}}}'
```

**Expected:** UI hides the composer; the API **rejects** the post server-side
(P1 broadcast lock in `comms_governance.py`) — not merely a hidden button.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 3.7 — Topic creation is not offered to a plain member

**Client:** Mobile (comms) **+ API**. &nbsp; **Precondition:** `driver1` is a plain member (not owner/admin) of a channel.

3.7.1 Open the channel's topic bar.
3.7.2 Confirm **no "+ New topic"** control is shown.
3.7.3 Attempt to create a topic via the API and confirm it is rejected server-side.

**Expected:** Topic-create control is hidden and the server rejects a forced
`ChannelTopic` create (governance: owner/admin or dispatcher only).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 3.8 — "Hey Dispatch" voice assistant

**Client:** Mobile. &nbsp; **Precondition:** Assistant service configured (`ASSISTANT_PROVIDER` + key); `VITE_ASSISTANT_BASE_URL` set.

3.8.1 Open the assistant / push-to-talk surface.
3.8.2 Speak a request (e.g. "message dispatch that I'm running late").
3.8.3 Confirm the assistant responds.
3.8.4 Confirm any resulting dispatcher message is posted on the driver's behalf.

**Expected:** Assistant responds and can perform its tool actions (e.g. post a
message) for the driver.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 3.9 — Channel badges reflect membership

**Client:** Mobile (comms). &nbsp; **Precondition:** `driver1` is a member of several channels with differing type/standing.

3.9.1 Open the channels list.
3.9.2 Inspect the role / standing / channel-type badges per channel.
3.9.3 Confirm badges match the driver's actual membership (e.g. member vs admin, broadcast vs group).

**Expected:** Badges accurately reflect the signed-in user's role, standing, and
the channel type.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

<div style="page-break-before: always;"></div>

## 4. Updater role *(`updater1`)*

Status maintenance on dispatch's behalf. **No** driver record → no driver-only
surfaces.

### 4.1 — Updater login has no driver context

**Client:** Mobile / console. &nbsp; **Precondition:** `updater1` has no linked `Driver`.

4.1.1 Log in as `updater1` / `fleet123`.
4.1.2 Confirm general access loads.
4.1.3 Confirm **no** driver-only tabs (My Loads / my equipment) appear.

**Expected:** Login succeeds; driver-specific surfaces are absent (no driver
context to resolve).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 4.2 — Update a load / trip status on dispatch's behalf

**Client:** Mobile / console. &nbsp; **Precondition:** Loads/trips exist.

4.2.1 Open a load or trip.
4.2.2 Change its status (the updater's core job).
4.2.3 Confirm the write succeeds.
4.2.4 (Cross-check) As `dispatch1`, confirm the status change is visible.

**Expected:** Status update succeeds and is reflected for dispatch.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 4.3 — Driver-only actions are unavailable (graceful)

**Client:** Mobile / console. &nbsp; **Precondition:** Logged in as `updater1`.

4.3.1 Attempt to reach a driver-only surface (e.g. "my equipment" / "my trips").
4.3.2 Observe the result.

**Expected:** The surface is unavailable or empty because there's no driver
context — a **graceful** empty state, **not** an error/crash.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 4.4 — Comms access follows membership

**Client:** Mobile / console (comms). &nbsp; **Precondition:** `updater1` is a member of at least one channel.

4.4.1 Open a channel the updater belongs to.
4.4.2 Read the conversation (dev-permissive read grants apply).
4.4.3 Post per the channel's member rules.

**Expected:** Updater can read channels they're a member of; posting follows the
same member-role/standing rules as everyone else (§5).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

<div style="page-break-before: always;"></div>

## 5. Comms member-role & standing *(orthogonal to app-role)*

These gate the message board **independently** of app-role. Run each case as a
user **holding that membership** in the target channel — regardless of their
application role.

### 5.1 — Owner/admin capabilities

**Client:** Any (comms). &nbsp; **Precondition:** Test user is **owner or admin** of a channel.

5.1.1 Create a topic in the channel.
5.1.2 Mute another member.
5.1.3 Pin a message with scope **Everyone**.
5.1.4 Confirm all three actions are permitted.

**Expected:** Owner/admin may create topics, moderate members, and pin for everyone.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 5.2 — Plain member restrictions

**Client:** Any (comms) **+ API**. &nbsp; **Precondition:** Test user is a **plain member** (not owner/admin, not dispatcher app-role) of a **broadcast** channel.

5.2.1 Confirm the topic-create control is hidden.
5.2.2 Confirm the composer is hidden/read-only in the broadcast channel.
5.2.3 Force both actions via the API and confirm each is rejected server-side.

**Expected:** Topic-create and broadcast-post are blocked in **both** the UI and
the API (client gating + LogicBank rule).

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 5.3 — Muted/banned member cannot post (restriction active)

**Client:** Any (comms) **+ API**. &nbsp; **Precondition:** Test user muted or banned in a channel with `restricted_until` **NULL or in the future** (set up in **2.10**).

5.3.1 Open the channel as the restricted user.
5.3.2 Attempt to post via the UI.
5.3.3 Attempt to post via the API.
5.3.4 Read the block reason.

**Expected:** Posting is **rejected server-side**; the message indicates the
active mute/ban restriction.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 5.4 — Expired restriction allows posting again

**Client:** Any (comms). &nbsp; **Precondition:** Test user muted with `restricted_until` set to a **past** timestamp.

5.4.1 Confirm the member's standing still reads muted but `restricted_until` is in the past.
5.4.2 Attempt to post.

**Expected:** Posting is **allowed** — the rule treats a past `restricted_until`
as an expired restriction.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 5.5 — @mention a member

**Client:** Any (comms). &nbsp; **Precondition:** Two members in a channel.

5.5.1 Post a message containing `@<member>`.
5.5.2 Confirm the mention is registered/highlighted.
5.5.3 Confirm the mentioned member is notified (per the app's notification surface).

**Expected:** The @mention resolves to the member and notifies them.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

<div style="page-break-before: always;"></div>

## 6. Cross-client parity & regression

Run the same actions across clients to confirm the ALS JSON:API + realtime bridge
keep everything consistent.

### 6.1 — Comms surfaces match across desktop and mobile

**Client:** Desktop console **and** mobile. &nbsp; **Precondition:** Same user is a member of the same channel on both clients.

6.1.1 In one client, post a message, pin it, save it, attach a file, and add a topic.
6.1.2 Open the same channel on the other client.
6.1.3 Confirm the message, pin, Saved entry, attachment, and topic all appear.

**Expected:** All Feature-4 surfaces (message, pin, Saved, attachment, topic)
render consistently on both clients.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 6.2 — Message written on one client appears on the other

**Client:** Desktop console **and** mobile. &nbsp; **Precondition:** Both clients open to the same channel.

6.2.1 Post a message from the mobile app.
6.2.2 Watch the desktop console (server push / reconcile poll).
6.2.3 Confirm the message appears without a manual refresh (within the poll window).
6.2.4 Repeat in the reverse direction (desktop → mobile).

**Expected:** New messages propagate both directions via the push bridge /
reconcile poll.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

### 6.3 — Server-side governance is active after deploy

**Client:** curl / journal. &nbsp; **Precondition:** ALS running the current build.

6.3.1 Confirm the governance module loaded:
```bash
journalctl -u fleet-dispatcher-api -n 200 --no-pager \
  | grep -Ei 'discovered logic|rules loaded'
#   expect comms_governance.py in the discovered-logic list + "N rules loaded"
```
6.3.2 Confirm a new comms resource is reflected: `curl -o /dev/null -w '%{http_code}\n' -H "Authorization: Bearer $TOKEN" http://localhost:5659/api/ChannelTopic` → `200`.

**Expected:** Governance rules are loaded and the Feature-4 tables are reflected —
the prerequisite for §3.6 / §5 enforcement.

> **Result:** ☐ Pass ☐ Fail ☐ Blocked — **Tester:** ________ **Date:** ______ — **Notes:** ______________________

<div style="page-break-before: always;"></div>

## Appendix A — Traceability

| Area | Cases | Server-side rule / source |
| --- | --- | --- |
| Authentication | 1.1–1.6 | ALS JWT auth · `auth_provider.py` |
| Dispatcher ops | 2.1–2.5 | Board/HUD · Loads/Trips/Waypoints |
| Comms (dispatcher) | 2.6–2.13 | `CommPanel` (desktop) |
| Driver field ops | 3.1–3.4 | Mobile driver tabs · `driver.user_id` |
| Broadcast lock | 3.6, 5.2 | `comms_governance.py` (P1) |
| Mute/ban | 5.3–5.4, 2.10 | `comms_governance.py` (P2) |
| Topic gating | 2.8, 3.7, 5.2 | `comms_governance.py` (ChannelTopic) |
| Parity/regression | 6.1–6.3 | ALS JSON:API + realtime bridge |

## Appendix B — Setup helpers

- **Toggle a user active/inactive** (for 1.3):
  `UPDATE app_user SET active=false WHERE username='updater1';` (revert to `true` after).
- **Find the broadcast channel id** (for 3.6/5.2):
  `SELECT id, name FROM channel WHERE channel_type_id = 3;`
- **Set a mute with an expiry** (for 5.3/5.4):
  `UPDATE channel_member SET member_status_id=2, restricted_until = now() + interval '1 hour' WHERE …;`
  (past expiry: `now() - interval '1 hour'`).
- **Reset the demo DB** to a known state: re-apply `database/schema.sql` +
  `seed_data.sql` on a throwaway (see `/verify-db`), never on a persistent DB you
  care about.

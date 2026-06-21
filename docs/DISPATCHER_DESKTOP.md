# Dispatcher Desktop & HUD — design

> Status: **proposed.** Design for the C++/Wt dispatcher portal: the application
> shell, the Today/Week board, and the **control → command → HUD** mechanism.
> Talks to the shared JSON:API for data (no direct DB).

## Two faces, one Wt server

The desktop portal serves two surfaces from the same Wt application:

| Surface             | Who          | Nature                  | Entry        |
| ------------------- | ------------ | ----------------------- | ------------ |
| **Control console** | Dispatcher   | Interactive             | `/`          |
| **HUD display**     | Wall / big screen | Read-only, auto-updating | `/hud`  |

The control console issues **commands**; HUD sessions **react** to them in real
time via Wt server push.

## Sign-in

The console is gated by a Wt login (`LoginView`) using the shared **ALS JWT
auth** — the same credentials as the mobile app. `DispatcherApp` shows the login
first, then the `Shell` once a token + user are loaded. The bearer token lives
**server-side** in the Wt session (never sent to the browser) and rides on every
`ApiClient` call. **Sign out** clears it and returns to the login. See
[`AUTHENTICATION.md`](AUTHENTICATION.md). (The `/hud` display stays
unauthenticated for now.)

## The shell

The shell is an **app frame** implementing the shared design system
([`DESIGN_SYSTEM.md`](DESIGN_SYSTEM.md)) — subtle blues, sparse orange accents,
light/dark themes:

- **Full-width header** — **logo branding (top-left)**; **profile (name) + the
  logged-in role + Sign out (top-right)**, plus a **theme toggle** (light/dark,
  persisted).
- **Panel toggle bar** — a slim bar **directly above the body**; the left/right
  panel show/hide toggles are anchored to opposite edges (above their panels)
  and fixed-width, so they never shift when clicked. They use the disclosure
  aesthetic: **▼ when open**, a pointing arrow when closed (**▶** left panel,
  **◀** right panel).
- **Three-column body**:
  - **Left panel** — the **menuing system** (Board · Fleet · Map · New Load ·
    Communications · Settings) plus **work-panel toggles** that operate the
    center: mode (**Today | Week**) and **Compact rows**.
  - **Center work panel** — the active view outlet. It has **no hide/show toggle**;
    it simply **swaps** the visible view based on the selected menu item:
    - **Board** — Today/Week dispatch board.
    - **Fleet** — `FleetView`: a list of drivers + equipment.
    - **Map** — `MapView`: **geo-positioning** of fleet locations on a Leaflet
      map (latest position per rig) + a detail table, refreshed on a timer.
    - **New Load** — the load intake form.
    - **Communications** — comms take over the **full work area**
      (`CommPanel` in `Layout::Full`): a **Channels (Groups) directory** on the
      left (channels listed in sections by type — Groups / Direct messages /
      Broadcast) + the conversation on the right. The view **animates in**
      (`fd-rise-in`) and the redundant right rail is **auto-hidden** while it's
      active (and restored when you leave, unless you'd collapsed it yourself).
    - **Settings** — `SettingsView`: appearance (theme) + account/connection info.
  - **Right panel** — **Communications** rail (`CommPanel` in `Layout::Rail` —
    compact horizontal channel chips), collapsible.
  - **Comm-panel header** (`buildConvoHead`) shows in **both** layouts: the
    conversation title + a board-action toolbar — a **reduced** (icon) set in the
    rail and the **full** (labeled) set in the take-over view. The first action is
    **Export**: bundles the board's channel meta + topics + members + messages
    (raw JSON:API docs) into `board-<name>-<date>.json`, downloaded via a Blob URL
    (`ApiClient::fetchRaw`). Future actions (stats, status, archive) slot in here.
  - Panels hide/show via `.fd-collapsed` and stack under the center on narrow
    viewports.
- **Full-width footer** — copyright + links (HUD, Docs, Support).
- **Toaster** — top-right transient alerts (`Toaster`): new messages, save/create
  confirmations, errors. One sweeper timer auto-dismisses (no delete-in-callback).
- **Profile** — `ProfileView`: view/edit profile fields (`PATCH /AppUser/{id}`);
  avatar upload is a follow-up.
- **Owned state** — the signed-in `AppUser`; the JSON:API client (with the bearer
  token) is owned by the application and shared with the shell.

### Real-time comms (push / WebSockets)

`CommPanel` is push-driven, not polling-driven, for on-server traffic:

- **`CommBus`** (in-process singleton, modeled on `HudControlBus`) broadcasts a
  newly-sent message to every subscribed session via **Wt server push**
  (`WServer::post` → handler → `triggerUpdate`).
- Push is delivered over **WebSockets** — enable them in `wt_config.xml`
  (`<web-sockets>true</web-sockets>`) and run with `-c wt_config.xml`. `run.sh`
  passes the config automatically.

> **Leaflet map config (version-sensitive).** Wt ≥ 4.7 loads Leaflet itself from
> the `leafletJSURL` / `leafletCSSURL` **config properties** (set in
> `wt_config.xml`) and throws a *fatal* error at `WLeafletMap` construction if
> they're unset — which blanks the page (this is what made the HUD a white
> screen). Don't also `useStyleSheet`/`require` Leaflet in code (double-load).
> The config must be loaded (`-c wt_config.xml`, which `run.sh` does). For an
> offline deploy, host `leaflet.js`/`leaflet.css` under the docroot and point the
> two properties at the local paths.
- **Cross-app** (e.g. a mobile message): `RealtimeClient` — a server-wide
  Boost.Beast WebSocket client (opt-in `cmake -DFD_REALTIME_CLIENT=ON`) — connects
  to the realtime bridge (`FLEET_REALTIME_URL` + `FLEET_REALTIME_TOKEN`), consumes
  ALS→Kafka events, and publishes them into `CommBus`, which pushes to every
  session. `CommPanel` de-dups by message id so an own send (intra-`CommBus` +
  bridge echo) renders once. See [`REALTIME.md`](REALTIME.md).
- A **slow reconcile poll** (30 s) remains the fallback when the client is off or
  the bridge/Kafka is unavailable — so comms are never fragile.

The **Map** view reads telemetry (`PositionReport`) on a 15 s timer — positions
flow device → DB, and the console reads them via the API; realtime telemetry push
is a similar backend follow-up.

## Board modes

| Mode      | Shows                                  | Query                                  | Layout                     |
| --------- | -------------------------------------- | -------------------------------------- | -------------------------- |
| **Today** | Today's runs (active loads today)      | loads intersecting today               | per-driver list / timeline |
| **Week**  | The Mon→Mon board                      | loads for the current `dispatch_week`  | drivers × 7 days grid      |

The mode toggle swaps both the query and the layout. The HUD honors the same
mode, switched remotely by a command.

## Control → command → HUD

```
   Control console ──publish(cmd)──▶ HudControlBus (server singleton)
                                        │ WServer::post(sessionId, …)
   HUD session(s) ◀─── server push ─────┘   apply(cmd) + triggerUpdate()
```

- HUD sessions call `enableUpdates(true)` and **subscribe** to the bus.
- A control emits a `HudCommand`; the bus posts it to every HUD session, which
  applies it and triggers a UI update.
- **Generalizable**: one bus carries every HUD command — `SetMode`,
  `FocusDriver`, `HighlightLoad`, `ZoomMap`, …

### Command model (sketch)

```cpp
struct HudCommand {
    enum Type { SetMode, FocusDriver, HighlightLoad /* … */ } type;
    std::string arg;            // e.g. "today" | "week" | a driver/load id
};

enum class BoardMode { Today, Week };

// One instance per Wt server. Thread-safe; HUD sessions register a handler.
class HudControlBus {
public:
    static HudControlBus& instance();
    using Handler = std::function<void(const HudCommand&)>;

    std::string subscribe(const std::string& sessionId, Handler h);  // HUD on open
    void unsubscribe(const std::string& token);                      // HUD on close

    void publish(const HudCommand& cmd) {                            // control surface
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& [token, sub] : subs_)
            Wt::WServer::instance()->post(sub.sessionId,
                [h = sub.handler, cmd] { h(cmd); });   // runs under the app's update lock
    }
private:
    struct Sub { std::string sessionId; Handler handler; };
    std::mutex mutex_;
    std::map<std::string, Sub> subs_;
};
```

HUD handler applies the command and calls `Wt::WApplication::instance()->triggerUpdate()`.
Control button: `HudControlBus::instance().publish({HudCommand::SetMode, "week"});`

This mirrors Wt's `simplechat` broadcast example (a shared server-side service
posting to subscribed sessions).

## Same-server vs. distributed HUD

- **Same Wt server** (dispatcher workstation + wall display on one host/LAN):
  the in-process bus above — instant, no extra infra. **Recommended start.**
- **Separate process/machine:** also persist the command as a **`hud_command`
  resource via the JSON:API** (auditable, replayable); the remote HUD
  subscribes/polls. Optional later; the bus interface stays the same.

## Data

Both faces read board data from the JSON:API (`Load`, `Driver`, `Equipment`,
`DispatchWeek`). The **map** layer (truck locations) is Feature 2 (telemetry +
the geospatial endpoint); until then the HUD renders board/load/status data and
the command mechanism is exercised by the Today/Week toggle.

## Build order

1. Shell: app bar, nav, content outlet, command bar; wire the JSON:API client.
2. Board view with the **Today | Week** toggle (control console).
3. `HudControlBus` + `/hud` entry; HUD reacts to `SetMode` over server push.
4. Add further commands (`FocusDriver`, `HighlightLoad`) on the same bus.
5. (Feature 2) map layer + truck locations.

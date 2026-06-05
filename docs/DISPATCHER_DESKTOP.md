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

## The shell

The shell is the frame + app-wide state every view plugs into:

- **Top app bar** — brand, current **dispatch week** selector, API/health
  indicator, user/role.
- **Navigation** — Board · Loads · Drivers · Equipment · Messages · HUD.
- **Content outlet** — hosts the active view.
- **Command bar** — mode toggle (**Today | Week**) + action buttons that emit
  HUD commands.
- **Owned state** — JSON:API client, current week, current board mode, handle to
  the command bus.

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

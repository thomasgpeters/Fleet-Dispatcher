# Dispatcher portal — desktop (C++ / Wt)

The desktop client for the **Dispatcher** role: receive and dispatch new loads,
manage the Monday→Monday weekly board, assign drivers and equipment.

**Stack:** C++17 with the [Wt](https://www.webtoolkit.eu/wt) framework, themed
with **Bootstrap** (`Wt::WBootstrap5Theme`).

Like the mobile portal, it talks to the shared middleware over **JSON:API**
(ApiLogicServer / SAFRS) at the configured API base (default
`http://localhost:5656/api`) and never touches PostgreSQL directly.

## What this portal does

- **Dispatcher:** view this week's assigned loads, take/decline loads, mark
  in-transit / delivered, record post-trip inspection, log fuel/DEF/oil, manage
  own schedule (ELD-adjacent).
- Build out the weekly board, load intake form, and driver/equipment assignment
  views against the JSON:API resources described in
  [`../../docs/domain-model.md`](../../docs/domain-model.md).

## Styling — Bootstrap + the Wt `resources/` folder

The app uses `Wt::WBootstrap5Theme`. Wt themes load their CSS/JS from the Wt
**`resources/`** folder, which must sit under the docroot at runtime
(`<docroot>/resources`). Experience shows that if the *entire* `resources/` tree
isn't deployed next to the app, Bootstrap styles render incorrectly — so the
CMake build copies the whole folder beside the executable on every build, and
`install` does the same.

App-specific overrides live in [`docroot/css/fleet-dispatcher.css`](docroot/css/fleet-dispatcher.css)
and are also deployed (the whole `docroot/` is copied next to the binary).

Resulting runtime layout (in the build directory):

```
build/
  fleet_dispatcher_desktop      ← executable
  resources/                    ← full Wt resources tree (themes/bootstrap, jquery, …)
  css/fleet-dispatcher.css      ← from docroot/
```

## Build

Requires a C++17 compiler, CMake ≥ 3.16, and Wt (built with the HTTP connector
and Bootstrap theme support).

### Dependencies (Linux)

**Debian / Ubuntu:**

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libwt-dev libwthttp-dev
# Boost is pulled in as a Wt dependency; if not, add: libboost-all-dev
```

**Fedora / RHEL:**

```bash
sudo dnf install -y gcc-c++ cmake wt-devel
```

Wt installs its `resources/` tree under `/usr/share/Wt/resources` (Debian) or
`/usr/share/Wt/resources` / `/usr/local/share/Wt/resources` — CMake auto-detects
it (override with `-DWT_RESOURCES_DIR=...`).

> **Bootstrap 5 theme note:** `Wt::WBootstrap5Theme` needs **Wt ≥ 4.5**. Check
> with `pkg-config --modversion wt` (or `dpkg -l libwt-dev`). If your distro ships
> an older Wt, either build Wt from source, or switch the theme to
> `Wt::WBootstrapTheme` with `setVersion(Wt::BootstrapVersion::v3)` in
> `src/main.cpp` (a comment there marks the spot).

### Configure, build, run

```bash
cd portals/dispatcher-desktop
cmake -S . -B build
cmake --build build

# Start it (sensible defaults; override via env or pass-through flags):
./run.sh                       # console at :8080/ , HUD at :8080/hud
HTTP_PORT=9000 FLEET_API_BASE_URL=http://api.lan:5656/api ./run.sh
# (or run the binary directly:)
./build/fleet_dispatcher_desktop --docroot build --http-address 0.0.0.0 --http-port 8080
```

[`run.sh`](run.sh) points `--docroot` at the build dir (where CMake deployed the
Wt `resources/` tree) and forwards any extra args to the Wt binary. Env knobs:
`HTTP_ADDRESS`, `HTTP_PORT`, `DOCROOT`, `BIN`, `FLEET_API_BASE_URL`.

Then open http://localhost:8080 (control console). The **HUD display** is served
at **`/hud`** (e.g. http://localhost:8080/hud) on the same server; the console's
Today/Week toggle drives it live via the in-process command bus.

The HUD shows a **Leaflet map** (`Wt::WLeafletMap`, needs **Wt ≥ 4.5**) of the
latest truck position per rig, refreshed every 15s. Leaflet's JS/CSS are loaded
from a CDN in `HudView.cpp`; for an offline/self-contained deploy, host them
locally (or set the leaflet URLs in `wt_config.xml`) and remove those two lines.
Map tiles use OpenStreetMap (no API key).

CMake auto-detects the Wt `resources/` folder in the common install locations
(`/usr/share/Wt/resources`, `/usr/local/share/Wt/resources`, …). If Wt lives in
a non-standard prefix, point at it explicitly:

```bash
cmake -S . -B build -DWT_RESOURCES_DIR=/path/to/Wt/resources
```

If the folder can't be found, CMake warns at configure time and Bootstrap will
not render until you set `WT_RESOURCES_DIR`.

### Install

```bash
sudo cmake --install build --prefix /opt/fleet-dispatcher
# -> /opt/fleet-dispatcher/bin/fleet_dispatcher_desktop
#    /opt/fleet-dispatcher/bin/resources/...   (Wt resources)
#    /opt/fleet-dispatcher/bin/css/...          (app docroot)
```

Run the installed build (docroot is the install `bin/` so `resources/` resolves):

```bash
FLEET_API_BASE_URL=http://localhost:5656/api \
  /opt/fleet-dispatcher/bin/fleet_dispatcher_desktop \
  --docroot /opt/fleet-dispatcher/bin --http-address 0.0.0.0 --http-port 8080
```

### Run as a systemd service

A ready-to-edit unit and env template live in [`deploy/`](deploy):

```bash
# 1. Install the app to a prefix (above), then create a service user:
sudo useradd --system --no-create-home --shell /usr/sbin/nologin fleet
sudo chown -R fleet:fleet /opt/fleet-dispatcher

# 2. Environment (endpoint, etc.):
sudo mkdir -p /etc/fleet-dispatcher
sudo cp deploy/desktop.env.example /etc/fleet-dispatcher/desktop.env   # edit as needed

# 3. Install + start the unit:
sudo cp deploy/fleet-dispatcher-desktop.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now fleet-dispatcher-desktop

# 4. Check it:
systemctl status fleet-dispatcher-desktop
journalctl -u fleet-dispatcher-desktop -f
```

The unit runs the binary with `--docroot /opt/fleet-dispatcher/bin` so the Wt
`resources/` (Bootstrap CSS/JS) resolve. Adjust the prefix, port
(`--http-port`), user, and `EnvironmentFile` to taste — see
[`deploy/fleet-dispatcher-desktop.service`](deploy/fleet-dispatcher-desktop.service).

### Publishing the mobile module from CMake

CMake exposes the monorepo's `make publish-mobile` (git-subtree publish of
`portals/mobile` to the `Fleet-Dispatcher-Mobile` repo):

```bash
# On demand:
cmake --build build --target publish-mobile

# Or run it automatically after the desktop binary links:
cmake -S . -B build -DFLEET_PUBLISH_MOBILE=ON
cmake --build build
```

Publishing pushes to a remote, so the POST_BUILD step is **off by default**;
enable it with `-DFLEET_PUBLISH_MOBILE=ON`. Override the target with
`-DMOBILE_REMOTE_URL=...` / `-DMOBILE_BRANCH=...` (defaults to the HTTPS URL on
`main`). When the desktop app is built standalone (no top-level `Makefile`), the
target is silently skipped.

## Configuration

`FLEET_API_BASE_URL` (default `http://localhost:5656/api`) selects the
middleware endpoint. See [`../../.env.example`](../../.env.example).

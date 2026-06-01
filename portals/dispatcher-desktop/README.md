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

```bash
cd portals/dispatcher-desktop
cmake -S . -B build
cmake --build build
./build/fleet_dispatcher_desktop --docroot build --http-address 0.0.0.0 --http-port 8080
```

Then open http://localhost:8080.

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
cmake --install build --prefix /opt/fleet-dispatcher
# -> /opt/fleet-dispatcher/bin/fleet_dispatcher_desktop
#    /opt/fleet-dispatcher/bin/resources/...   (Wt resources)
#    /opt/fleet-dispatcher/bin/css/...          (app docroot)
```

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

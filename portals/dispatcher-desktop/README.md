# Dispatcher portal — desktop (C++ / Wt)

The desktop client for the **Dispatcher** role: receive and dispatch new loads,
manage the Monday→Monday weekly board, assign drivers and equipment.

**Stack:** C++17 with the [Wt](https://www.webtoolkit.eu/wt) framework.

Like the mobile portal, it talks to the shared middleware over **JSON:API**
(ApiLogicServer / SAFRS) at the configured API base (default
`http://localhost:5656/api`) and never touches PostgreSQL directly.

## Build

Requires a C++17 compiler, CMake ≥ 3.16, and Wt (with libcurl for the JSON:API
client).

```bash
cd portals/dispatcher-desktop
cmake -S . -B build
cmake --build build
./build/fleet_dispatcher_desktop --docroot . --http-address 0.0.0.0 --http-port 8080
```

Then open http://localhost:8080.

## Configuration

`FLEET_API_BASE_URL` (default `http://localhost:5656/api`) selects the
middleware endpoint. See [`../../.env.example`](../../.env.example).

## Status

Scaffold: `CMakeLists.txt` plus a minimal Wt application (`src/main.cpp`) that
renders the dispatcher shell. Build out the weekly board, load intake form, and
driver/equipment assignment views against the JSON:API resources described in
[`../../docs/domain-model.md`](../../docs/domain-model.md).

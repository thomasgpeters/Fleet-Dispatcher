#!/usr/bin/env bash
#
# Run the Fleet Dispatcher — Dispatcher desktop (Wt HTTP server).
#
# Serves the control console at  /     and the HUD at  /hud
# on the same server. The Wt resources/ tree and the app docroot are deployed
# into the build directory by CMake, so we point --docroot at it.
#
# Override any of these via environment, e.g.:
#   HTTP_PORT=9000 FLEET_API_BASE_URL=http://api.lan:5659/api ./run.sh
#
#   BUILD_DIR           build output dir          (default: ./build)
#   BIN                 executable path           (default: $BUILD_DIR/fleet_dispatcher_desktop)
#   DOCROOT             Wt docroot (has resources/) (default: $BUILD_DIR)
#   HTTP_ADDRESS        listen address            (default: 0.0.0.0)
#   HTTP_PORT           listen port               (default: 8089)
#   FLEET_API_BASE_URL  JSON:API base             (default: http://localhost:5659/api)
#
# Any extra arguments are passed straight through to the Wt binary, e.g.:
#   ./run.sh --http-port 9090 --accesslog access.log

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$HERE/build}"
BIN="${BIN:-$BUILD_DIR/fleet_dispatcher_desktop}"
DOCROOT="${DOCROOT:-$BUILD_DIR}"
HTTP_ADDRESS="${HTTP_ADDRESS:-0.0.0.0}"
HTTP_PORT="${HTTP_PORT:-8089}"
export FLEET_API_BASE_URL="${FLEET_API_BASE_URL:-http://localhost:5659/api}"

if [ ! -x "$BIN" ]; then
    echo "error: binary not found: $BIN" >&2
    echo "       build first:  cmake -S \"$HERE\" -B \"$BUILD_DIR\" && cmake --build \"$BUILD_DIR\"" >&2
    exit 1
fi
if [ ! -d "$DOCROOT/resources" ]; then
    echo "warning: $DOCROOT/resources not found — Bootstrap/Leaflet may not render." >&2
    echo "         (CMake copies the Wt resources/ tree into the build dir on build.)" >&2
fi

echo "Dispatcher console : http://${HTTP_ADDRESS}:${HTTP_PORT}/"
echo "Fleet HUD          : http://${HTTP_ADDRESS}:${HTTP_PORT}/hud"
echo "JSON:API           : ${FLEET_API_BASE_URL}"
echo "docroot            : ${DOCROOT}"
echo

exec "$BIN" \
    --docroot "$DOCROOT" \
    --http-address "$HTTP_ADDRESS" \
    --http-port "$HTTP_PORT" \
    "$@"

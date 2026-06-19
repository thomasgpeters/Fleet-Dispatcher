#!/usr/bin/env bash
#
# Install Fleet Dispatcher's ALS customizations into a generated ApiLogicServer
# project. Run this AFTER `ApiLogicServer create` (a fresh generate) — ALS
# `rebuild` preserves logic/, so a one-time install survives rebuilds.
#
# Idempotent: safe to re-run. Copies our auto-discovered logic into
# <ALS_PROJECT>/logic/logic_discovery/ where ALS picks it up automatically.
#
# Usage:
#   als-extensions/install.sh [ALS_PROJECT_DIR]
#   ALS_PROJECT=/path/to/fleet-dispatcher-api als-extensions/install.sh
#
# Default target: ../fleet-dispatcher-api (sibling of this repo).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ALS_PROJECT="${1:-${ALS_PROJECT:-$HERE/../../fleet-dispatcher-api}}"

if [[ ! -d "$ALS_PROJECT" ]]; then
    echo "error: ALS project not found: $ALS_PROJECT" >&2
    echo "       pass the path:  als-extensions/install.sh /path/to/fleet-dispatcher-api" >&2
    exit 1
fi
if [[ ! -d "$ALS_PROJECT/logic" ]]; then
    echo "error: $ALS_PROJECT doesn't look like an ALS project (no logic/)." >&2
    exit 1
fi

DEST="$ALS_PROJECT/logic/logic_discovery"
mkdir -p "$DEST"
cp -v "$HERE/logic_discovery/"*.py "$DEST/"

# Security customizations — only if `ApiLogicServer add-auth` has been run (it
# creates security/). Overwrites the generated defaults so auth uses app_user
# (werkzeug hashes) and grants are in place. See docs/AUTHENTICATION.md.
if [[ -d "$ALS_PROJECT/security" ]]; then
    mkdir -p "$ALS_PROJECT/security/authentication_provider/sql"
    cp -v "$HERE/security/authentication_provider/sql/auth_provider.py" \
          "$ALS_PROJECT/security/authentication_provider/sql/auth_provider.py"
    cp -v "$HERE/security/declare_security.py" \
          "$ALS_PROJECT/security/declare_security.py"
    security_installed=1
else
    echo "note: no $ALS_PROJECT/security — run 'ApiLogicServer add-auth --provider-type=sql'"
    echo "      first, then re-run this script to install the auth provider + grants."
    security_installed=0
fi

echo
echo "Installed ALS extensions into: $ALS_PROJECT"
echo "  - logic/logic_discovery/  (Kafka event producers)"
if [[ "${security_installed}" -eq 1 ]]; then
    echo "  - security/  (app_user auth provider + role grants)"
fi
echo
echo "Then run ALS with security on (and a real secret):"
echo "  export SECURITY_ENABLED=true"
echo "  export SECRET_KEY=<your-jwt-secret>          # also used by realtime/ bridge"
echo "  python api_logic_server_run.py 0.0.0.0 5659"
echo
echo "For realtime, also enable the Kafka producer in the ALS config:"
echo "  KAFKA_CONNECT = '{\"bootstrap.servers\": \"localhost:9092\"}'   (older ALS: KAFKA_PRODUCER)"
echo "Events flow: ALS -> Kafka -> realtime/ bridge -> clients."


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

echo
echo "Installed ALS extensions into: $DEST"
echo "Reminder — enable the Kafka producer in the ALS project config, e.g.:"
echo "  KAFKA_CONNECT = '{\"bootstrap.servers\": \"localhost:9092\"}'   (older ALS: KAFKA_PRODUCER)"
echo "Then (re)start ALS. Events flow: ALS -> Kafka -> realtime/ bridge -> clients."

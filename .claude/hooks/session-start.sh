#!/bin/bash
# Fleet Dispatcher — SessionStart hook for Claude Code on the web.
#
# Installs the mobile app's npm dependencies so `npm run build`, `npm run lint`,
# and the /verify-db ritual work out of the box. The container state is cached
# after this completes, so subsequent sessions start warm.
#
# Notes on the other components (intentionally NOT set up here):
#   * Database verification uses a *throwaway* PostgreSQL cluster (PG16 is
#     preinstalled in the web sandbox) — see .claude/commands/verify-db.md — so
#     no persistent DB needs standing up at session start.
#   * The ApiLogicServer middleware is generated outside this repo, and the Wt
#     desktop is not compiled in the sandbox; neither is a session dependency.
#   * assistant/ and geospatial/ are Python; add a `pip install -r requirements
#     .txt` (in a venv) here if/when their tests are run from the web.
set -euo pipefail

# Web sessions only; locally you manage your own toolchain.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

# `npm install` (not `ci`) so the cached node_modules is reused across sessions.
cd "$CLAUDE_PROJECT_DIR/portals/mobile"
npm install --no-audit --no-fund

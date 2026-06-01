#!/usr/bin/env bash
#
# Publish portals/mobile to the standalone Fleet-Dispatcher-Mobile repo using
# git subtree. The prefix is stripped, so the mobile app's package.json lands at
# the ROOT of the target repo — which is what VCP builds.
#
# Develop in this monorepo (the source of truth); run this to publish.
#
# Config (env vars, all optional):
#   MOBILE_REMOTE_URL   git URL of Fleet-Dispatcher-Mobile
#                       (default: git@github.com:thomasgpeters/Fleet-Dispatcher-Mobile.git)
#   MOBILE_REMOTE_NAME  local remote name (default: mobile)
#   MOBILE_BRANCH       target branch (default: main)
#
# Usage:
#   scripts/publish-mobile.sh
#   MOBILE_REMOTE_URL=https://github.com/thomasgpeters/Fleet-Dispatcher-Mobile.git scripts/publish-mobile.sh

set -euo pipefail

PREFIX="portals/mobile"
REMOTE_NAME="${MOBILE_REMOTE_NAME:-mobile}"
REMOTE_URL="${MOBILE_REMOTE_URL:-git@github.com:thomasgpeters/Fleet-Dispatcher-Mobile.git}"
BRANCH="${MOBILE_BRANCH:-main}"

# Run from the repo root regardless of where the script is invoked from.
cd "$(git rev-parse --show-toplevel)"

if [ ! -d "$PREFIX" ]; then
  echo "error: $PREFIX not found (run from the Fleet-Dispatcher monorepo)" >&2
  exit 1
fi

# Ensure the remote exists and points at the configured URL.
if git remote get-url "$REMOTE_NAME" >/dev/null 2>&1; then
  git remote set-url "$REMOTE_NAME" "$REMOTE_URL"
else
  git remote add "$REMOTE_NAME" "$REMOTE_URL"
fi

echo "Publishing '$PREFIX' -> remote '$REMOTE_NAME' ($REMOTE_URL), branch '$BRANCH'"
echo

# Standard subtree push. Recomputes the prefix history and pushes it.
# If the target has diverged and a fast-forward is impossible, fall back to a
# split + force push (see scripts/README or the monorepo README).
git subtree push --prefix="$PREFIX" "$REMOTE_NAME" "$BRANCH"

echo
echo "Done. Fleet-Dispatcher-Mobile @ $BRANCH now mirrors $PREFIX."

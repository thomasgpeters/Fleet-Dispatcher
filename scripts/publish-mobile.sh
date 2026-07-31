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
#                       (default: https://github.com/thomasgpeters/Fleet-Dispatcher-Mobile.git)
#   MOBILE_REMOTE_NAME  local remote name (default: mobile)
#   MOBILE_BRANCH       target branch (default: main)
#
# Usage:
#   scripts/publish-mobile.sh
#   MOBILE_REMOTE_URL=git@github.com:thomasgpeters/Fleet-Dispatcher-Mobile.git scripts/publish-mobile.sh   # SSH override

set -euo pipefail

PREFIX="portals/mobile"
REMOTE_NAME="${MOBILE_REMOTE_NAME:-mobile}"
REMOTE_URL="${MOBILE_REMOTE_URL:-https://github.com/thomasgpeters/Fleet-Dispatcher-Mobile.git}"
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
if git subtree push --prefix="$PREFIX" "$REMOTE_NAME" "$BRANCH"; then
  echo
  echo "Done. Fleet-Dispatcher-Mobile @ $BRANCH now mirrors $PREFIX."
  exit 0
fi

# Fast-forward push was rejected — the mobile repo diverged from the monorepo's
# recomputed subtree (a stale/auto commit on that repo, or a prior push with
# different hashes). The monorepo is the source of truth and the mobile repo is a
# publish MIRROR (no direct development there), so recompute the subtree and
# force-push it, overwriting the remote branch. If instead the push failed for a
# non-diverge reason (auth), this force-push surfaces the same error.
echo
echo ">> Fast-forward rejected; falling back to split + force-push"
echo "   (monorepo is source of truth; mobile repo is a publish mirror)."
TMP_BRANCH="mobile-publish-tmp-$$"
git branch -D "$TMP_BRANCH" 2>/dev/null || true
git subtree split --prefix="$PREFIX" -b "$TMP_BRANCH"
git push "$REMOTE_NAME" "$TMP_BRANCH:$BRANCH" --force
git branch -D "$TMP_BRANCH"

echo
echo "Done (force). Fleet-Dispatcher-Mobile @ $BRANCH now mirrors $PREFIX."

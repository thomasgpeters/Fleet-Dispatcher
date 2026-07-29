#!/usr/bin/env bash
# Fleet Dispatcher — runtime smoke check for MESSAGING + GEOLOCATION.
#
# Exercises the live services end-to-end so you can tell, in one command, whether
# comms and fleet-location data are actually flowing. Read-only by default; pass
# --post to also send (and read back) a test message.
#
# Usage:
#   scripts/smoke-check.sh                     # read-only checks
#   scripts/smoke-check.sh --post              # also post a test message
#   API_BASE=http://localhost:5659/api GIS_BASE=http://localhost:5701 \
#     SMOKE_USER=dispatch1 SMOKE_PASS=fleet123 scripts/smoke-check.sh
#
# Config (env, with sensible defaults; also reads ./.env if present):
#   API_BASE   ALS JSON:API base            (default http://localhost:5659/api)
#   GIS_BASE   geospatial endpoint base     (default http://localhost:5701)
#   SMOKE_USER / SMOKE_PASS  login creds     (default dispatch1 / fleet123)
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -f "$ROOT/.env" ] && { set -a; . "$ROOT/.env"; set +a; }

API_BASE="${API_BASE:-http://localhost:5659/api}"
GIS_BASE="${GIS_BASE:-http://localhost:${GIS_PORT:-5701}}"
SMOKE_USER="${SMOKE_USER:-dispatch1}"
SMOKE_PASS="${SMOKE_PASS:-fleet123}"
DO_POST=0; [ "${1:-}" = "--post" ] && DO_POST=1

pass=0; fail=0
ok()   { printf '  \033[32m✓\033[0m %s\n' "$1"; pass=$((pass+1)); }
bad()  { printf '  \033[31m✗\033[0m %s\n' "$1"; fail=$((fail+1)); }
note() { printf '    %s\n' "$1"; }

# Extract a top-level JSON string value without requiring jq.
jval() { sed -n "s/.*\"$1\":\"\([^\"]*\)\".*/\1/p" | head -1; }
# Count "type" occurrences in a JSON:API collection (rough resource count).
jcount() { grep -o '"type"' | wc -l | tr -d ' '; }

echo "Fleet Dispatcher smoke check"
echo "  API_BASE=$API_BASE   GIS_BASE=$GIS_BASE   user=$SMOKE_USER"
echo

# --- Auth -------------------------------------------------------------------
echo "AUTH"
LOGIN=$(curl -sS -m 10 -X POST "$API_BASE/auth/login" \
  -H 'Content-Type: application/json' \
  -d "{\"username\":\"$SMOKE_USER\",\"password\":\"$SMOKE_PASS\"}" 2>/dev/null)
TOKEN=$(printf '%s' "$LOGIN" | jval access_token)
if [ -n "$TOKEN" ]; then ok "login ($SMOKE_USER) → token"; else
  bad "login failed — cannot continue"; note "response: ${LOGIN:0:160}"; exit 1
fi
AUTH=(-H "Authorization: Bearer $TOKEN")
echo

# --- Messaging --------------------------------------------------------------
echo "MESSAGING (ALS JSON:API)"
CH=$(curl -sS -m 10 "${AUTH[@]}" "$API_BASE/Channel" 2>/dev/null)
NCH=$(printf '%s' "$CH" | jcount)
[ "${NCH:-0}" -gt 0 ] && ok "channels reachable ($NCH)" || bad "no channels / Channel unreadable"

CHID=$(printf '%s' "$CH" | sed -n 's/.*"id":"\([0-9a-f-]\{36\}\)".*/\1/p' | head -1)
MSG=$(curl -sS -m 10 "${AUTH[@]}" "$API_BASE/Message?page%5Blimit%5D=200" 2>/dev/null)
NMSG=$(printf '%s' "$MSG" | jcount)
[ "${NMSG:-0}" -gt 0 ] && ok "messages reachable ($NMSG)" || note "no messages yet (not necessarily broken)"

if [ "$DO_POST" = 1 ] && [ -n "$CHID" ]; then
  # NB: use a custom name — UID is a read-only bash builtin (the numeric user id).
  AUTHOR_ID=$(curl -sS -m 10 "${AUTH[@]}" "$API_BASE/AppUser?filter%5Busername%5D=$SMOKE_USER" 2>/dev/null \
        | sed -n 's/.*"id":"\([0-9a-f-]\{36\}\)".*/\1/p' | head -1)
  if [ -z "$AUTHOR_ID" ]; then
    bad "could not resolve author id for $SMOKE_USER (write test skipped)"
  else
    BODY="smoke-check $(cat /proc/sys/kernel/random/uuid 2>/dev/null || echo test)"
    POST=$(curl -sS -m 10 -o /dev/null -w '%{http_code}' "${AUTH[@]}" -X POST "$API_BASE/Message" \
      -H 'Content-Type: application/vnd.api+json' \
      -d "{\"data\":{\"type\":\"Message\",\"attributes\":{\"channel_id\":\"$CHID\",\"author_id\":\"$AUTHOR_ID\",\"body\":\"$BODY\"}}}" 2>/dev/null)
    case "$POST" in
      20*) ok "posted a test message (HTTP $POST) to channel $CHID"
           note "(a 'smoke-check …' message now exists in that channel)";;
      *)   bad "message POST returned HTTP $POST";;
    esac
  fi
else
  note "write test skipped (run with --post to send a test message)"
fi
echo

# --- Geolocation / fleet locations -----------------------------------------
echo "GEOLOCATION (fleet positions)"
POS=$(curl -sS -m 10 "${AUTH[@]}" "$API_BASE/PositionReport?page%5Blimit%5D=500" 2>/dev/null)
NPOS=$(printf '%s' "$POS" | jcount)
if [ "${NPOS:-0}" -gt 0 ]; then ok "position_report data present ($NPOS via API)"
else bad "no position_report rows — HUD/map will be empty (seed or driver check-ins needed)"; fi

# Geospatial endpoint (may be down / not deployed — treated as a warning).
GH=$(curl -sS -m 8 -o /dev/null -w '%{http_code}' "$GIS_BASE/health" 2>/dev/null)
if [ "$GH" = "200" ]; then
  ok "geospatial endpoint /health (HTTP 200)"
  LP=$(curl -sS -m 8 "$GIS_BASE/positions/latest" 2>/dev/null)
  NLP=$(printf '%s' "$LP" | grep -o '"lat"' | wc -l | tr -d ' ')
  [ "${NLP:-0}" -gt 0 ] && ok "/positions/latest returns $NLP fix(es)" \
                        || note "/positions/latest empty (no recent positions)"
else
  note "geospatial endpoint not answering on $GIS_BASE (HTTP ${GH:-000}) — HUD map"
  note "reads position_report directly, so fleet locations can still work without it."
fi
echo

# --- Summary ----------------------------------------------------------------
echo "SUMMARY: $pass passed, $fail failed"
[ "$fail" -eq 0 ] && echo "All core checks passed." || echo "See the ✗ lines above."
exit "$fail"

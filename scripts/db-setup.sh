#!/usr/bin/env bash
#
# One-shot database standup — DEPLOYMENT.md steps 1–4:
#   1. (optional) create the role + database
#   2. apply database/schema.sql
#   3. apply database/seed_data.sql
#   4. (optional) apply database/gis_bootstrap.sql  (PostGIS into the `gis` schema)
#
# Config via environment or a .env at the repo root:
#   DB_HOST DB_PORT DB_NAME DB_USER DB_PASSWORD
#     (defaults: localhost 5432 fleet_dispatcher fleet fleet)
#   DATABASE_URL   derived from the above if unset
#
# Role/database creation needs admin access; provide a superuser psql command:
#   FLEET_ADMIN_PSQL   default: "sudo -u postgres psql"
#     e.g. FLEET_ADMIN_PSQL='psql postgresql://postgres@localhost:5432/postgres'
#
# Flags:
#   --no-create   skip role/database creation (DB already exists)
#   --no-seed     skip seed_data.sql
#   --no-gis      skip gis_bootstrap.sql (no PostGIS)
#
# Usage:
#   scripts/db-setup.sh
#   scripts/db-setup.sh --no-create --no-gis

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Load .env (if present) without clobbering already-exported vars.
if [ -f .env ]; then
    set -a
    # shellcheck disable=SC1091
    . ./.env
    set +a
fi

DB_HOST="${DB_HOST:-localhost}"
DB_PORT="${DB_PORT:-5432}"
DB_NAME="${DB_NAME:-fleet_dispatcher}"
DB_USER="${DB_USER:-fleet}"
DB_PASSWORD="${DB_PASSWORD:-fleet}"
DATABASE_URL="${DATABASE_URL:-postgresql://${DB_USER}:${DB_PASSWORD}@${DB_HOST}:${DB_PORT}/${DB_NAME}}"
FLEET_ADMIN_PSQL="${FLEET_ADMIN_PSQL:-sudo -u postgres psql}"

do_create=1 do_seed=1 do_gis=1
for arg in "$@"; do
    case "$arg" in
        --no-create) do_create=0 ;;
        --no-seed)   do_seed=0 ;;
        --no-gis)    do_gis=0 ;;
        *) echo "unknown flag: $arg" >&2; exit 2 ;;
    esac
done

echo "==> Target: ${DB_NAME} as ${DB_USER}@${DB_HOST}:${DB_PORT}"

if [ "$do_create" -eq 1 ]; then
    echo "==> [1/4] Ensuring role + database (admin: ${FLEET_ADMIN_PSQL})"
    $FLEET_ADMIN_PSQL -v ON_ERROR_STOP=1 <<SQL
DO \$do\$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = '${DB_USER}') THEN
        CREATE ROLE "${DB_USER}" LOGIN PASSWORD '${DB_PASSWORD}';
    END IF;
END
\$do\$;
SELECT 'CREATE DATABASE "${DB_NAME}" OWNER "${DB_USER}"'
 WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = '${DB_NAME}')\gexec
-- Shared instance: let the app role create its own `fleet` schema, and reflect
-- it by default so ALS sees the fleet schema (not public).
GRANT CREATE ON DATABASE "${DB_NAME}" TO "${DB_USER}";
ALTER ROLE "${DB_USER}" SET search_path = fleet, public;
SQL
else
    echo "==> [1/4] Skipping role/database creation (--no-create)"
fi

echo "==> [2/4] Applying schema.sql"
psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -q -f database/schema.sql

if [ "$do_seed" -eq 1 ]; then
    echo "==> [3/4] Applying seed_data.sql"
    psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -q -f database/seed_data.sql
else
    echo "==> [3/4] Skipping seed (--no-seed)"
fi

if [ "$do_gis" -eq 1 ]; then
    # CREATE EXTENSION postgis needs superuser, so run as admin against the
    # target DB. Fed via stdin so the admin OS user needn't read the repo file.
    echo "==> [4/4] Applying gis_bootstrap.sql as admin (PostGIS into gis schema)"
    $FLEET_ADMIN_PSQL -d "$DB_NAME" -v ON_ERROR_STOP=1 < database/gis_bootstrap.sql
else
    echo "==> [4/4] Skipping PostGIS (--no-gis)"
fi

echo "==> Done. Next: generate/run ApiLogicServer (see docs/DEPLOYMENT.md step 5)."

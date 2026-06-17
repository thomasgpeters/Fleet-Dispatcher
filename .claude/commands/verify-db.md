---
description: Apply schema.sql + seed_data.sql to a throwaway PostgreSQL and report results
allowed-tools: Bash, Read
---

Verify that `database/schema.sql` and `database/seed_data.sql` apply cleanly to a
**fresh, throwaway** PostgreSQL 16 cluster — the standing rule before committing
any schema or seed change in this repo. Never touch a real/persistent database.

Run this script and report the outcome (schema OK / seed OK, the `fleet` table
count, and any errors). If `$ARGUMENTS` names a focus (e.g. a table or FK),
also print a quick check for it.

```bash
set -e
PGBIN=/usr/lib/postgresql/16/bin
TMP=$(mktemp -d); chmod 777 "$TMP"
cleanup() { runuser -u postgres -- "$PGBIN/pg_ctl" -D "$TMP/data" -w stop >/dev/null 2>&1 || true; rm -rf "$TMP"; }
trap cleanup EXIT

runuser -u postgres -- "$PGBIN/initdb" -D "$TMP/data" -A trust -E UTF8 >/dev/null 2>&1
runuser -u postgres -- "$PGBIN/pg_ctl" -D "$TMP/data" -o "-p 5439 -k $TMP" -l "$TMP/log" -w start >/dev/null 2>&1
runuser -u postgres -- psql -h "$TMP" -p 5439 -d postgres -c "CREATE DATABASE fd;" >/dev/null

echo "server_encoding: $(runuser -u postgres -- psql -h "$TMP" -p 5439 -d fd -tAc 'SHOW server_encoding;')"
echo "== schema =="
runuser -u postgres -- psql -h "$TMP" -p 5439 -d fd -v ON_ERROR_STOP=1 -q -f database/schema.sql && echo "schema OK"
echo "== seed =="
runuser -u postgres -- psql -h "$TMP" -p 5439 -d fd -v ON_ERROR_STOP=1 -q -f database/seed_data.sql >/dev/null && echo "seed OK"
echo "== fleet tables =="
runuser -u postgres -- psql -h "$TMP" -p 5439 -d fd -tAc \
  "SELECT count(*) FROM information_schema.tables WHERE table_schema='fleet';"
echo "== public tables (expect 0) =="
runuser -u postgres -- psql -h "$TMP" -p 5439 -d fd -tAc \
  "SELECT count(*) FROM information_schema.tables WHERE table_schema='public';"
```

If `gis_bootstrap.sql` is part of the change, note that it needs PostGIS (the
sandbox may lack it) and apply it only when available.

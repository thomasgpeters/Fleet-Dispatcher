# ApiLogicServer — authenticate against your real domain user table

A small, reproducible pattern for giving an [ApiLogicServer](https://apilogicserver.github.io/Docs/)
project **production-shaped JWT auth**: validate logins against your **own
domain user table** (with werkzeug-hashed passwords) and role grants — instead of
the default separate auth store.

Built for a trucking-dispatch platform; shared back for the ALS community.

## Files

| File | Drop in at | What it does |
| --- | --- | --- |
| `auth_provider.py` | `security/authentication_provider/sql/auth_provider.py` | Authenticates against `app_user` (werkzeug `pbkdf2` hashes). |
| `declare_security.py` | `security/declare_security.py` | Read grants for your roles (dev-permissive baseline). |
| `install.sh` | (your repo) | Re-applies both over a freshly generated project. |

## Use

```bash
# 1) generate, then enable JWT auth (creates security/)
ApiLogicServer create --project_name=my-api --db_url="$DATABASE_URL"
cd my-api && ApiLogicServer add-auth --provider-type=sql && cd -

# 2) install these customizations (re-run after every regenerate)
./install.sh ./my-api

# 3) run with security ON (defaults OFF) + a real secret
cd my-api
export SECURITY_ENABLED=true
export SECRET_KEY=$(openssl rand -hex 32)
python api_logic_server_run.py 0.0.0.0 5656
```

Adapt `auth_provider.py` / `declare_security.py` to your schema: the example
assumes an `AppUser` model (`username`, `full_name`, `email`, `active`,
`password_hash`, `app_role_id`) and an `AppRole` lookup with a `code`.

## Two gotchas worth the README

1. **`get_user` is dual-purpose.** ALS calls it for **login**
   (`get_user(username, password_str)`) *and* for **JWT identity resolution on
   every authenticated request** (`get_user(identity, jwt_claims_dict)`). The 2nd
   arg is a **dict** in the JWT path — guard the password check with
   `isinstance(password, str)`, or `check_password_hash(hash, <dict>)` →
   `dict.encode()` → **500** on the first authed call after login.
2. **Validate inline.** Newer ALS validates the login *via* `get_user` (treating a
   `None` return as failure), so the provider verifies the werkzeug hash itself
   and returns `None` on mismatch (generic message; a distinct one for disabled
   accounts).

Operational reminders: `SECURITY_ENABLED=true` must be exported (defaults off);
run `add-auth` before installing; ALS login needs a **JSON body with
`Content-Type: application/json`**; restarting Postgres invalidates ALS's pooled
connections (restart ALS too, or set
`SQLALCHEMY_ENGINE_OPTIONS = {"pool_pre_ping": True}`).

## Credit

ApiLogicServer / LogicBank by **Val Huber** and the GenAI-Logic team — the
declarative-rules lineage that goes back to Versata. This pattern only exists
because the generator is so clean to extend.

MIT License — see `LICENSE`.

# Authentication & user profiles

Fleet Dispatcher authenticates with **ApiLogicServer's built-in JWT auth**,
verifying against the `app_user` table (no separate auth service). Clients log in,
get a token, and send it as a bearer header on every request. This doc covers the
ALS setup (the middleware, generated outside this repo) and how the schema/clients
support it.

> **Why ALS built-in:** ALS already generates and serves the JSON:API from the
> schema; its JWT auth reflects the same database. A standalone auth service would
> duplicate that. (External IdP / Keycloak is an option ALS supports too, but it's
> out of scope here.)

## Data model

`app_user` (in `database/schema.sql`) is the identity table ALS reflects:

| Column              | Role                                                            |
| ------------------- | -------------------------------------------------------------- |
| `username`          | Login identity (unique).                                       |
| `password_hash`     | **Werkzeug `pbkdf2:sha256`** hash — ALS verifies against this. |
| `app_role_id`       | → `app_role` (`dispatcher` / `driver` / `updater`) for grants. |
| `active`            | Disabled users can't sign in.                                  |
| `full_name`, `email`, `phone`, `title`, `timezone` | Editable profile fields.       |
| `avatar_document_id`| → `document` (profile photo stored via the CMS).               |
| `last_login_at`     | Stamp on successful login (optional, app/LogicBank).           |

`driver.user_id` links a driver record to a login, so the mobile app can resolve
the signed-in user's `Driver` (and assigned equipment) for the driver-only tabs.

## ALS setup (middleware, outside this repo)

After creating the project (see [`MIDDLEWARE_SETUP.md`](MIDDLEWARE_SETUP.md)):

**The auth customization is versioned + reinstallable** (so a from-scratch build
is reproducible). The working `auth_provider.py` (authenticates against `app_user`
+ werkzeug hashes) and `declare_security.py` (role grants) live in
[`../als-extensions/security/`](../als-extensions/security/) and are installed by
`make als-extensions` **after** `add-auth`:

```bash
# 1. enable JWT auth (registers POST /api/auth/login; creates security/)
cd fleet-dispatcher-api && ApiLogicServer add-auth --provider-type=sql && cd -

# 2. install our app_user provider + grants (+ Kafka producers)
make als-extensions ALS_PROJECT=./fleet-dispatcher-api

# 3. run with security ON (defaults OFF) + a real secret
cd fleet-dispatcher-api
export SECURITY_ENABLED=true          # config.py defaults this to False!
export SECRET_KEY=<your-jwt-secret>   # share with the realtime/ bridge; keep out of git
python api_logic_server_run.py 0.0.0.0 5659
```

What the installed `auth_provider.py` does (lookup + werkzeug check):

```python
from werkzeug.security import check_password_hash
from database import models
user = session.query(models.AppUser).filter(models.AppUser.username == id).one_or_none()
# get_user returns a DotMapX (id, name, email, password_hash, UserRoleList[role_name])
# check_password: check_password_hash(user.password_hash, password)
```

Gotchas (each cost us a round during first standup):
- **`SECURITY_ENABLED` defaults to `False`** — you must `export SECURITY_ENABLED=true`,
  else the login endpoint 405s/500s.
- The **default** sql provider uses a separate sqlite auth DB + **plaintext**
  passwords — that's why we replace it with the `app_user`/werkzeug version.
- **"No Grants Yet"** → authenticated reads return empty/denied until
  `declare_security.py` has grants (our installed file grants all roles read).

- **Login endpoint:** `POST /api/auth/login` with `{"username", "password"}`
  returns `{"access_token": "<jwt>"}`. Authenticated requests send
  `Authorization: Bearer <jwt>`.

> **Regenerate the schema after these changes.** The new `app_user` columns
> (`password_hash`, profile fields, `avatar_document_id` FK) mean ALS must be
> re-reflected so `AppUser` exposes them and the `AppUser ↔ Document` avatar
> relationship.

## Passwords

Hashes are **werkzeug `pbkdf2:sha256`**, which `check_password_hash` verifies.
Generate one:

```python
from werkzeug.security import generate_password_hash
print(generate_password_hash("the-password"))   # pbkdf2:sha256:...$salt$hash
```

**Dev seed:** the three demo users (`dispatch1`, `driver1`, `updater1`) all have
password **`fleet123`** (hashes in `database/seed_data.sql`). **Change these for
any real deployment.** Never store plaintext; the column is `password_hash`.

## Clients

**Mobile (Ionic/React)** — implemented:

- `src/auth/AuthContext.tsx` — `useAuth()` provides `{ user, driverId,
  equipmentId, token, login, logout }`. Login calls `POST /api/auth/login`,
  persists the token, and loads the `AppUser` (+ linked `Driver`/equipment).
- `src/api/client.ts` — sends `Authorization: Bearer` on every request; a 401/403
  trips the unauthorized handler → auto-logout.
- `src/pages/LoginPage.tsx` — shown until authenticated; `ProfilePage.tsx` —
  view/edit profile, set a photo (via the CMS `document` table), sign out.
- Identity is no longer hardcoded: the old `currentUser.ts` placeholder is gone
  (only the `PHONE_PUSH_SOURCE_ID` lookup constant remains).

**Token storage:** the token is kept in `localStorage` (works in the browser and
the Capacitor WebView). For a hardened native build, switch to Capacitor Secure
Storage / Preferences — tracked in [`TODO.md`](TODO.md).

**Desktop (Wt)** — implemented for the dispatcher console:

- `src/LoginView.*` gates the console: it calls `ApiClient::login()`
  (`POST /api/auth/login`), then loads the profile via
  `fetchUserByUsername()`. `src/main.cpp` (`DispatcherApp`) shows the login
  first, then the `Shell` once a token + user are in hand.
- `ApiClient` holds the bearer token (`setAuthToken`/`clearAuthToken`) and adds
  `Authorization: Bearer` to every GET/POST/PATCH. The token lives **server-side**
  in the Wt session (never sent to the browser) — stronger than the mobile
  WebView's `localStorage`.
- `src/ProfileView.*` shows the signed-in user and edits profile fields
  (`PATCH /AppUser/{id}`); the app bar shows name · role with a **Sign out**
  button. (Avatar upload is a follow-up — the mobile app already has it.)
- **Not compiled in the dev sandbox** (no Wt). Two Wt-version-sensitive spots are
  flagged in `ApiClient.cpp`: the `done()` `error_code` type and
  `Http::Method::Patch` (older Wt may lack PATCH — fall back to PUT).
- **HUD** (`/hud`): stays unauthenticated for now (wall-screen display). When auth
  is enforced on reads, give it a service token via `ApiClient::setAuthToken()`.

## CORS

The mobile app and the API are different origins (and Capacitor uses
`capacitor://`/`ionic://`). Ensure the ALS server allows the app's origin and the
`Authorization` header (ALS ships permissive dev CORS; tighten for production).

## Regenerate gotchas (als-extensions ↔ ALS version)

Our auth + logic live in `als-extensions/` and are reinstalled over a freshly
generated project (`make als-extensions`). A 2026-06 fresh standup surfaced four
ALS-version-sensitive issues — all now fixed in the tracked files, so a clean
regenerate + `make als-extensions` just works. Documented here so they're not a
mystery if the behavior recurs on a newer ALS:

1. **`Rule` import path.** Import as `from logic_bank.logic_bank import Rule`
   (matches ALS's generated `declare_logic.py`), **not** from
   `logic_bank.rule_bank.rule_bank` (that module exposes `RuleBank`). Wrong path
   → `ImportError` at LogicBank activation.
2. **`declare_security_message`.** `config/server_setup.py` reads a module-level
   `declare_security.declare_security_message` string at startup; our replacement
   `declare_security.py` must define it or startup throws `AttributeError`.
3. **`get_user` validates the password.** Newer ALS `login()` validates via
   `get_user(username, password)` and treats a `None` return as failure (it does
   **not** rely on a separate `check_password`). So `get_user` verifies the
   werkzeug hash inline and returns `None` on mismatch.
4. **`get_user` is dual-purpose.** The SAME method is called for **login**
   (`get_user(username, password_str)`) and for **JWT identity resolution on every
   authenticated request** (`get_user(identity, jwt_claims_dict)`). The second arg
   is a **dict** in the JWT path — guard the password check with
   `isinstance(password, str)`, or `check_password_hash(hash, <dict>)` throws
   `AttributeError` (500) on the first authed call after login.

Operational reminders from the same standup:
- **`SECURITY_ENABLED=true`** must be exported (defaults off) — the #1 cause of
  "can't log in" after a regenerate.
- **`add-auth` before `make als-extensions`** (the installer needs `security/`).
- Restarting **PostgreSQL** invalidates ALS's pooled connections → restart ALS
  too (or set `SQLALCHEMY_ENGINE_OPTIONS = {"pool_pre_ping": True}`).
- ALS login requires a **JSON body with `Content-Type: application/json`**;
  form-encoded posts return 401 (both our clients already send JSON correctly).
- Never edit a werkzeug hash through the shell — the `$…$…` gets mangled. Write it
  via `psql -v h=… -c "… :'h' …"` or `generate_password_hash` in Python.

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

```bash
cd fleet-dispatcher-api
ApiLogicServer add-auth --provider-type=sql --db_url=auth   # JWT auth, SQL provider
```

Then point the auth provider at `app_user` and verify the werkzeug hash. In the
generated `security/authentication_provider/sql/auth_provider.py`, the lookup +
check look like:

```python
from werkzeug.security import check_password_hash
from database.models import AppUser            # reflected from the fleet schema

class Authentication_Provider(Abstract_Authentication_Provider):
    @staticmethod
    def get_user(username: str, password: str) -> AppUser:
        user = session.query(AppUser).filter(AppUser.username == username).one_or_none()
        if user and user.active and user.password_hash \
           and check_password_hash(user.password_hash, password):
            return user
        return None
```

- **JWT secret:** set `SECRET_KEY` (ALS reads it from config/env) — keep it out of
  the repo. Tokens are signed with it; rotating it invalidates issued tokens.
- **Roles:** map `app_role` to ALS grants if you want row/column-level security per
  role. Not required for login itself.
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

**Desktop (Wt):** not yet wired (mobile-first). When added, the dispatcher
console/HUD will log in the same way and send the bearer token on its
`ApiClient` calls.

## CORS

The mobile app and the API are different origins (and Capacitor uses
`capacitor://`/`ionic://`). Ensure the ALS server allows the app's origin and the
`Authorization` header (ALS ships permissive dev CORS; tighten for production).

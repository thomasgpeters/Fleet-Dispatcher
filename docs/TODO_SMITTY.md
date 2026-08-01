# TODO — Smitty Services

Working plan for the current session or feature. Checkable items that the
agent produces during planning and marks off as it works.

For the long-lived prioritized backlog, see `docs/BACKLOG.md`.
For captured lessons / don't-repeat-this patterns, see `tasks/LESSONS.md`.

---

## How to use

1. **Plan First** — write the plan here before writing code.
2. **Verify Plan** — have the user confirm before implementation.
3. **Track Progress** — mark items complete as you finish them.
4. **Explain Changes** — a one-line summary per item is enough.
5. **Document Results** — add a Review section at the bottom when done.
6. **Capture Lessons** — after any correction, update `LESSONS.md`.

Legend: `[ ]` not started · `[~]` in progress · `[x]` done · `[-]` dropped

---

## Active plan

Working top-down through the 11-item pending-features list
(2026-04-21). The inline-edit standardization for Vehicle /
Purchase and the ncurses test-harness plan were both moved to
`docs/BACKLOG.md` (§6.3, §7) to keep the active plan focused on
feature delivery.

### Persistent auth + bcrypt — desktop side (gating item for mobile)

Migrate `src/Auth.cpp` from the current in-memory + base64 scheme
to DB-backed authentication against `app_users`, with bcrypt
password hashing. This is the blocker for the mobile client
logging in (they'd need to agree on the stored password format).

**Current state (from audit, 2026-04-20)**
- `Auth` is an in-memory singleton. Five seeded users (`admin`,
  `office`, `parts`, `mechanic`, one more) all have password
  `"password"`, base64-encoded (reversible, not a hash). Seed
  runs on every process restart, so `addUser` / `resetPassword`
  / `removeUser` appear to work but evaporate.
- `role_menu_privileges`-style custom tweaks via
  `setPrivilegesForRole(roleId, map)` also in-memory only.
- Eight call sites go through the singleton: `LoginPage`,
  `NavBar`, `SideBar`, `Settings` (the primary user-mgmt
  surface), `SmittyApplication`, `JobDetail`, plus the class
  files themselves. **All read-only accessors stay unchanged.**
- `app_users`, `roles`, `menu_items`, `role_menu_privileges`
  tables exist in `schema.sql` and are covered by patch 007.
- `password_hash VARCHAR(255) NOT NULL` is wide enough for bcrypt
  ($2a$ / $2b$ / $2y$ prefix + 22-char salt + 31-char digest =
  60 chars).
- **No bcrypt / OpenSSL / libsodium** in `CMakeLists.txt` today —
  will be added fresh.
- `loginEndpoint_ = "/auth/login"` is dead code; remove.

**Target architecture**
- Password storage: **bcrypt** (match
  `Imagery-License-Server`'s library — exact identity in the
  open-questions block below).
- User records live in Postgres via ALS (`/api/AppUser`).
- `Auth` retains the singleton shape and the 8 current call
  sites' APIs — only the implementation changes. Login does
  `fetchAllByField("AppUser", "username", u)` + bcrypt verify;
  `addUser` / `resetPassword` POST / PATCH against `/api/AppUser`.
- Role privileges still computed from `defaultPrivilegesForRole`
  as a baseline; overrides persisted to `role_menu_privileges`
  via ALS and refreshed on login.
- On successful login, `Auth` stashes a 64-char random session
  token in the `UserSession` so the mobile app's Bearer-interceptor
  pattern has a token to propagate — the desktop app doesn't
  need it, but generating it here keeps both clients consistent
  and sets up a future JWT upgrade cleanly.

#### Phase 1 — Schema alignment (DDL patch)
- [x] `database/patches/008_app_users_for_mobile.sql` — idempotent
      patch: `ADD COLUMN IF NOT EXISTS email VARCHAR(100) UNIQUE`,
      `ADD COLUMN IF NOT EXISTS login_enabled SMALLINT NOT NULL
      DEFAULT 1`, and `ALTER COLUMN password_hash TYPE VARCHAR(100)`.
      Wrapped in a single BEGIN/COMMIT with commented rollback.
      Integer defaults only, no string DEFAULTs (per LESSONS).
- [x] `database/schema.sql` — matching `CREATE TABLE app_users`
      shape + `CREATE INDEX idx_app_users_email`. Default-user
      seed INSERTs now also populate `email` (`admin@local`, etc.)
      so fresh-install DBs already satisfy the mobile
      `filter[email]=` lookup pattern.
- [x] `database/seed_data.sql` — appended `idx_app_users_email`
      to the index list so ALS-seeded installations pick it up.
- [ ] **User runs:** apply patch 008 + rebuild ALS
      (`ApiLogicServer rebuild-from-database …`), then `curl -s
      http://localhost:5659/api/AppUser | head` to verify the new
      columns surface.

#### Phase 2 — Bcrypt plumbing

- [x] **BLOCKER cleared (2026-04-21):** user pasted
      `src/bcrypt/blowfish.cpp` from Student-Onboarding.
- [x] Added `src/BcryptUtil.cpp` + `src/bcrypt/blowfish.cpp`
      to `CMakeLists.txt` SOURCES. No `SMITTY_WITH_BCRYPT`
      gate — the files are always compiled (no system dep,
      nothing to toggle).
- [x] Ported for Smitty: dropped `StudentIntake::Utils`
      namespace (Smitty is global-namespace); replaced
      `Logger.h` dep with `Wt::log("error"/"warn")` to match
      `src/ApiClient.cpp`; fixed `#include "blowfish.h"` →
      `#include "bcrypt/blowfish.h"` now that header and
      source live in different directories.
- [x] Smoke test wired into `Auth::Auth()` —
      `runBcryptSelfTest()` hashes `"password"` twice, verifies
      each, confirms the two hashes differ (salt randomises),
      and confirms a wrong password is rejected. Result logs
      at `info` on pass / `error` on fail with a precise failure
      shape. Fires once per process at the first `Auth::instance()`
      call.

#### Phase 3 — DB-backed `Auth` rewrite
- [x] `Auth::login(identifier, p)` — fetch all AppUsers,
      resolve identifier as username OR email (case-insensitive
      on email), check `active && login_enabled`, verify via
      `BcryptUtil::checkPassword`, populate `session_`,
      generate 64-char session token, load role privileges from
      `role_menu_privileges`.
- [x] `Auth::addUser(u, p, displayName, role, email="")` —
      POST `/api/AppUser` with
      `password_hash = BcryptUtil::hashPassword(p)`; email
      defaults to `<username>@local` when not supplied.
- [x] `Auth::resetPassword(u, newP)` — PATCH
      `/api/AppUser/{id}` with bcrypt hash; reload cache.
- [x] `Auth::removeUser(u)` — DELETE `/api/AppUser/{id}`;
      prevents deleting the currently logged-in user.
- [x] `Auth::updateUser(u, displayName, role, active)` — PATCH
      `/api/AppUser/{id}` for non-password fields.
- [x] Seeding stays in SQL per project convention — see
      `database/patches/009_seed_rbac_data.sql`. Idempotent
      seeds for `roles`, `menu_items`, `role_menu_privileges`,
      and `app_users` (base64 `cGFzc3dvcmQ=`, rehashed on first
      login). The C++ `Auth::seedDefaultUsersIfEmpty()` variant
      was removed — app shouldn't own data seeding.
- [x] `loginEndpoint_` + `setLoginEndpoint()` removed.
- [x] `users_` is now a rehydrated cache; authoritative data
      lives in `/api/AppUser`. Every mutation (add/update/reset/
      remove) reloads the cache before returning.
- [x] `defaultPrivilegesForRole()` kept as the fresh-install
      fallback, used when a role has zero `role_menu_privileges`
      rows. `applyMenuPrivileges()` prefers the DB, falls back
      to code.
- [x] `UserRecord::passwordBase64` renamed to `passwordHash`;
      `email` and `loginEnabled` fields added.

#### Phase 4 — Migration for existing installs
- [x] **First-login rehash.** `Auth::verifyPassword()` tries
      bcrypt first; on failure, if the stored value looks like
      legacy base64 (short, no `$` prefix, alnum+ `+/=` only),
      falls back to the old `base64Encode()` compare and on
      match PATCHes the row to the new bcrypt hash before
      completing login.
- [x] `[Auth] rehashed legacy password for user '...'` is
      emitted at `info` level so operators can watch the grace
      window drain.
- [ ] **60-day grace.** After ~60 days post-deploy, delete the
      legacy path in `Auth::verifyPassword` (comment near the
      `looksLikeLegacyBase64` branch marks where). Users who
      haven't logged in by then need an admin password reset.
- [-] `--migrate-passwords` CLI — **dropped.** The first-login
      rehash plus monitoring the log line is enough; a separate
      CLI is ceremony for a one-shot event.

#### Phase 5 — Settings UI + smoke tests
- [x] `Settings.cpp` "Users" tab — Email column now shows
      between Username and Display Name; Add User dialog
      gained an optional Email input (placeholder notes
      "defaults to `<username>@local`" when blank).
      Client-side validation catches obviously-malformed
      addresses (must contain `@` or be blank). The existing
      `addUser(u, p, d, r)` callers are untouched because the
      email parameter defaults to `""` on the Auth side.
- [x] `Settings.cpp` "Privileges" tab — custom role tweaks
      persist to `role_menu_privileges` via
      `Auth::setPrivilegesForRole()` → delete-all-then-insert
      against `/api/RoleMenuPrivilege`. Effective privileges on
      next login come from the DB.
- [x] `LoginPage.cpp` — label/placeholder updated to "Username
      or email"; `Auth::login()` resolves either against
      `/api/AppUser`.
- [x] Test scenarios drafted in
      `tests/USER_TESTING_SCENARIO.md`:
      - TC-AUTH-001 fresh install seeds four defaults
      - TC-AUTH-002 legacy base64 user logs in and is rehashed
      - TC-AUTH-003 reset password persists across restart
      - TC-AUTH-004 add user via Settings persists
      - TC-AUTH-005 email login (username / email / case-
        insensitive email / case-sensitive username)
- [ ] User to run the five TC-AUTH scenarios against a live
      build and capture results per the Reporting template.

#### Open questions for user (answer before writing code)

1. **Bcrypt library — LOCKED and vendored.** Student-Onboarding's
   OpenBSD-style bcrypt ported in. No `find_package`, no system
   dep. Smitty uses the split `include/` + `src/` layout (not
   Student-Onboarding's flat `src/utils/` tree):
   - [x] Ported into Smitty at `include/BcryptUtil.h`,
         `src/BcryptUtil.cpp`, `include/bcrypt/blowfish.h`,
         `src/bcrypt/blowfish.cpp`. Namespace `StudentIntake::Utils`
         dropped — Smitty uses global-namespace classes throughout.
         `Logger.h` dep replaced with `Wt::log("error"/"warn")` to
         match `src/ApiClient.cpp`'s logging style. Plain
         `#include "blowfish.h"` in the .cpp changed to
         `#include "bcrypt/blowfish.h"` now that header and source
         are in different directories.
   - [x] Added `src/BcryptUtil.cpp` + `src/bcrypt/blowfish.cpp`
         to `CMakeLists.txt` `SOURCES`.
   - [-] `PasswordHash.h` / `.cpp` wrapper — **skipped.** The
         `BcryptUtil::hashPassword()` / `checkPassword()` API is
         already clean and namespace-free; an extra wrapper would
         be ceremony with no benefit. `Auth.cpp` will call
         `BcryptUtil` directly in Phase 3.
   - **Cross-client compatibility:** bcrypt output is the
     standard `$2a$<cost>$<salt><digest>` string; mobile's
     `bcryptjs` and the vendored C++ port both produce and
     verify the same format.
   - **Cost factor:** match whatever Student-Onboarding's
     `BcryptUtil` uses by default (likely 10 or 12; confirm
     when copying the file). Cross-verify doesn't care about
     cost — only login speed does.

2. **Email column — ANSWERED: both.** Desktop username-login
   stays primary; email is optional but first-class. Users can
   log in with either on either client (desktop Wt app *and*
   mobile React app). `LoginPage` and `Auth::login()` accept an
   identifier and resolve by username or email in one path.

3. **Role model — ANSWERED: single-role, four roles unchanged.**
   One `role_id` FK per user, keep today's four roles
   (Administrator, Office Manager, Parts Manager, Mechanic).
   Mobile exposes the one role as a single-element array.
   No migration, no join table.

4. **Custom privilege persistence — ANSWERED: persist.**
   Admin tweaks from Settings → Privileges write back to
   `role_menu_privileges` via ALS. On login, `Auth` pulls the
   role's rows from the DB; `defaultPrivilegesForRole()` is
   only the fallback when a role has no rows yet.

5. **Password migration grace period — ANSWERED: 60 days.**
   Base64-fallback + auto-rehash code stays for 60 days after
   deploy. After that, the fallback branch is deleted and any
   user who hasn't logged in needs an admin password reset.
   We log each rehash event so we can watch the tail drain.

6. **Session token shape — ANSWERED: 64-char random string.**
   Generated client-side on login, stored in `UserSession`,
   re-generated on every login. JWT deferred to a future v2
   once an identity server is on the roadmap.

---

### Mechanic Portal Revamp

Multi-phase overhaul of the Mechanic Dashboard and the adjacent
admin JobDetail surfaces. Drives `docs/BACKLOG.md` §1 and §2 to
completion, plus user-driven UX items (Product link on JobParts,
autocomplete picker, bigger Part / Labor description editors).

Order is optimised for: (a) shipping visible mechanic-facing
change early, (b) bundling related code touches so the parts
state model is updated in mechanic + admin together, (c) putting
decisions that block other phases first within their phase.

**Phase 1 — 6-state parts pass (BACKLOG §1.1 + §2.1) — already shipped**

Discovery during planning: 6-state parts UI was already in place
on both surfaces — `MechanicDashboard.cpp:60-80, 633-671` and
`JobDetail.cpp:1151-1223`. The BACKLOG entry was stale.

- [x] MechanicDashboard part cards have all 5 transition buttons
      (Receive / Stock / Pick / Install / Inspect) — verified
- [x] Admin JobDetail parts table has Stock / Pick / Install /
      Inspect — verified
- [-] Lift label/class helpers into a shared header — deferred;
      duplication is small and admin uses inline switch, not the
      same helper, so extracting is more disruption than value
      right now. Re-open if a third surface needs the labels.

**Phase 2 — Labor status progression (BACKLOG §1.2 + §2.3)**

- [x] MechanicDashboard labor cards already had the status badge
      and Start / Mark Complete buttons (`MechanicDashboard.cpp`
      `laborStatusLabel`/`laborStatusClass`/`laborActionLabel`).
- [x] Admin JobDetail labor advance control — Ready / Start /
      Complete buttons added in the action column (commit 981b3f2).
- [ ] Decide: auto-flip labor 0→1 when linked parts hit
      Received? Deferred; manual advance works for now.

**Phase 3 — Add Part / Add Labor dialog improvements — shipped**

Touches the WDialogs at `src/JobDetail.cpp:1370` (parts) and
`:898` (labor).

- [x] **3.1 Bigger Part description.** `WLineEdit` →
      `WTextArea` (3 rows, span-full) — Add and Edit (commit 7386c9a).
- [x] **3.2 Bigger Labor description.** Bumped 3 → 5 rows in
      Add and Edit (commit 7386c9a).
- [x] **3.3 Decisions locked**: snapshot pricing; ad-hoc allowed;
      decrement-on-Pick when product_id set; "+ Add new Product"
      present. Pricing-history source = PO line items
      (`purchase_items.unit_cost` ordered by
      `purchases.purchase_date DESC`); no new history table.
- [-] **3.4 Schema patch** — not needed. `fk_job_parts_products`
      already exists in `seed_data.sql:3642`.
- [x] **3.5 / 3.6 Product autocomplete picker** wired inline into
      Add Part and Edit Part dialogs (commit de39a11). Pick auto-
      fills description from product_name and cost from last
      PO `unit_cost` (fallback to `products.unit_price` if no PO
      history). No separate ProductSuggestionPopup helper class —
      logic lives in `attachProductPicker()` on JobDetail.
- [x] **3.7 "+ New" inline product create** (commit b58e2a8).
      One-click create from typed name + current Cost; new
      product_id is linked into the JobPart, description auto-
      fills, local cache updates. No intermediate dialog.
- [x] **3.8 DataBus refresh** on Product create/update so the
      picker sees newly added SKUs without a page reload
      (commit d0e63f5; `loadProductsLookup` extracted from
      `loadLookups`).

Decrement inventory on Pick (3.3 outcome) — see backlog
"Phase 3 follow-up: inventory decrement on Pick" entry below.

**Phase 4 — Mechanic claim flow (BACKLOG §1.3) — shipped (commit ac4e512)**

`jobs.mechanic_id` is now written from the UI.

- [x] "Perform this Work" claim button on unclaimed cards.
- [x] "Show All Jobs" / "Show My Jobs" filter toggle — default
      view is mine + unassigned.
- [x] Claimed cards show "Assigned to <username>" badge; current
      user's claimed cards show "Assigned to you" + Release
      button (self-service un-claim).
- [x] `mechanic_id` added to Job EntityRegistry definition.

**Phase 5 — Road Test gate + progress bar refactor — already shipped**

Discovered both were already implemented:
- [x] **5.1 Road Test gate.** `MechanicDashboard.cpp:556` —
      gated by all parts inspected AND all labor complete.
- [x] **5.2 Progress bar.** Aggregate-read stage logic at
      `MechanicDashboard.cpp:544-565`; click is navigation, no
      bulk write-back.

**Phase 6 — Docs**

- [x] `tasks/TODO.md` cleanup (this section) so plan reflects
      what landed.
- [x] `tasks/LESSONS.md` — add lessons captured during this
      session.
- [x] `docs/BACKLOG.md` — mark §1.x / §2.1 / §2.3 as shipped.
- [x] `docs/DEVELOPMENT_LOG.md` — append Mechanic Portal Revamp
      Feature section.
- [ ] `docs/DESIGN_DECISIONS.md` — JobPart↔Product policy
      decisions (snapshot, ad-hoc, last-PO cost). Deferred —
      separate session.
- [ ] `docs/SERVICE_CENTER_GUIDE.md` — 6-state parts, labor
      status, claim flow. Deferred.
- [ ] `docs/USERS_GUIDE.md` — Perform this Work flow.
      Deferred.

**Phase 3 follow-up: inventory decrement on Pick — LANDED 2026-04-25**

- [x] Extracted shared helper to
      `include/Inventory.h` / `src/Inventory.cpp` —
      `Inventory::applyDelta(productId, delta)` reads
      `units_in_stock`, writes `current + delta` clamped to ≥0.
- [x] Wired into `MechanicDashboard::advancePartStatus` and
      `JobDetail::advancePartReceived` on the 2→3 (Pick)
      transition, only when `product_id` is set and quantity > 0.
- [x] Deleted orphan `JobDetail::installPart()` — a stale
      4-state-era version of the same logic that was never called.
- [-] Reverse on Release (state ≥3 → <3) — deferred; rare. The
      bigger reform (PO Receive drives the increment, not JobPart
      1→2) is captured as `docs/BACKLOG.md` §4a Phase A.

**Stale draft sections lower in this file**

The "Job parts vs. Products — formalize the relationship" and
"Products autocomplete on JobParts (and elsewhere)" sections
below are superseded by Phase 3 above. Will prune in a separate
cleanup commit; left in place now to preserve historical context
for the decisions we made.

---

### Parts Sourcing — Contractor-Quotes engine integration

Pull the multi-supplier sourcing engine from
[`thomasgpeters/Contractor-Quotes`](https://github.com/thomasgpeters/Contractor-Quotes)
into Smitty so mechanics and the parts manager can compare supplier
offers, pick the best one, and create POs from the chosen offer.
First step toward the "parts as tracked assets + AI sourcing" vision
in `docs/BACKLOG.md` §4a.

**Why Contractor-Quotes?** Same stack (C++17 / Wt / ALS / JSON:API),
a sourcing engine that's already factored cleanly: `SourcingEngine`
takes a data provider, returns a ranked vector of offers using a
weighted composite (price 40 %, availability + lead time 25 %,
proximity via Haversine 20 %, supplier rating 15 %), with bulk
discount support. ~130 lines of pure C++, plus DTOs and a
`supplier_product` join table that's exactly the multi-supplier
shape Smitty needs.

**Reuse strategy:** copy the engine + DTOs in-tree, defer extracting
a shared submodule until a third consumer materialises (per the
"don't extract a submodule yet" reasoning under the Accounting
discussion above). When the rule of three triggers we lift it into
something like `cpp-sourcing-engine`.

**Phase A — Schema integration**

- [ ] DDL patch `database/patches/NNN_supplier_product.sql`:
      - `CREATE TABLE supplier_product (id, product_id, supplier_id,
        unit_cost, stock_qty, in_stock, can_backorder, min_order_qty,
        bulk_discount, bulk_threshold, last_updated)` mirrored from
        Contractor-Quotes' table, adapted to Postgres / SMALLINT IDs
        and Smitty naming (unit_cost not unit_price for consistency
        with `purchase_items`).
      - FK in `seed_data.sql` (per Smitty convention, FKs live
        there): `fk_supplier_product_product`,
        `fk_supplier_product_supplier`.
      - `ALTER TABLE suppliers ADD COLUMN IF NOT EXISTS latitude
        DOUBLE PRECISION DEFAULT 0.0`, plus `longitude`, `rating`,
        `lead_time_days` so the sourcing weights have data to score
        on. Numeric defaults only (per LESSONS.md).
- [ ] `database/schema.sql` updated to match.
- [ ] `database/seed_data.sql` adds the new index and FKs.
- [ ] User runs the patch + ALS rebuild.

**Phase A.5 — Seed data**

- [ ] New patch `database/patches/NNN_seed_supplier_product.sql`
      adding ~3 supplier offers per truck-parts product so the
      engine has something to rank. Use the existing 50 truck SKUs
      and 10 suppliers from patch 014; each Cummins/Mack/Volvo OEM
      part also gets offers from regional cross-source suppliers
      (e.g. NAPA Heavy-Duty, FleetPride) to be added as new
      supplier rows. Realistic prices, lead times and stock so the
      ranking reads sensibly during demos.
- [ ] Add a "Shop location" setting to `app_config` (lat / lng of
      the home shop) so proximity scoring has a fixed reference
      point. Pre-fill with Imagery Motor Services HQ. Surfaced on
      the existing Settings page.

**Phase B — Port the engine in-tree**

- [ ] Copy Contractor-Quotes' `engine/SourcingEngine.h` /
      `.cpp` into Smitty as `include/Sourcing.h` /
      `src/Sourcing.cpp`. Single namespace `Sourcing` containing
      `Offer`, `Weights`, `Engine` (renamed from `SourcingResult` /
      `SourcingWeights` / `SourcingEngine` for cleanness).
- [ ] Drop the `DataProvider` indirection. Smitty already has a
      single backend (ALS via `ApiClient`); the engine reads
      `SupplierProduct` and `Supplier` directly through ApiClient
      calls. Saves an abstraction layer that's only useful when the
      project supports multiple data backends (Contractor-Quotes
      supports both Wt::Dbo and ALS — Smitty doesn't).
- [ ] Add to `CMakeLists.txt` SOURCES.
- [ ] Port the Boost.Test cases as plain C++ assertions in a
      `tests/sourcing_smoke.cpp` if the testing harness lands
      first; otherwise defer the tests to Phase 7 of the testing
      plan and leave a comment in the engine pointing at the
      original Contractor-Quotes test file as the reference
      oracle.

**Phase C — Sourcing UI — LANDED 2026-04-26**

- [x] **ProductDetail page: "Source Now" button.** Header action
      opens a modal with quantity input + ranked-offers table
      (supplier, unit cost, lead time, distance, stock, composite
      score, "Create PO" per row).
- [x] **Reusable SourcingDialog component** (`include/SourcingDialog.h`
      / `src/SourcingDialog.cpp`) so JobPart and ProductDetail share
      one implementation. Free-function API:
      `SourcingDialog::open(host, productId, qty, pickLabel, onPick)`.
- [x] **JobPart Add / Edit dialog: "Source" button** next to
      "+ New" in the picker row. Opens the same modal against
      the picked product; selecting an offer drops the supplier's
      `unit_cost` into the dialog's Cost field.
- [x] **Real Create PO from offer** — confirm dialog with
      read-only summary + editable expected_date / notes, then
      POSTs Purchase + PurchaseItem in sequence. Toast confirms
      `PO #N created`.
- [x] **Patch 018 (products.product_id IDENTITY)** — fixes Add
      Product / "+ New" inline create that was failing with
      `'product_id': 'product_id'` ALS rejection.
- [x] CSS — reused `.child-grid-table` + `.picker-row` for layout
      consistency. New `.dialog-summary` + `.dialog-summary-row` /
      `.dialog-summary-value` for the Create PO confirm read-only
      block.
- [x] Bootstrap Icons in autocomplete — *attempted then reverted*.
      Wt's WSuggestionPopup escapes suggestion text, so HTML
      icons rendered as raw markup. Switched to Unicode emoji
      prefixes per category (gear ⚙, fuel pump ⛽, ice cube 🧊,
      etc.). The Bootstrap Icons local bundle stays in
      `resources/bootstrap-icons/` for future use elsewhere.

**Phase D — Low-stock prompt**

**Phase D — Low-stock prompt**

- [ ] When mechanic Picks a part and `units_in_stock < quantity`,
      surface a Toast "Insufficient stock — source now?" with a
      button that opens the Phase C sourcing UI for that
      product, pre-filled with the missing quantity.
- [ ] Dashboard "Low Stock" tile lists products where
      `units_in_stock <= reorder_level`. Each row has a "Source"
      action that opens the same UI. (This is the seam for §4a
      Phase B once asset valuation lands.)

**Phase E — AI sourcing agent (deferred — schema and seam only)**

- [ ] Define a `Sourcing::Finder` interface
      (`std::vector<Offer> find(productId, quantity)`); the
      Contractor-Quotes-derived engine becomes the
      `Sourcing::CatalogFinder` impl reading from
      `supplier_product`. Future `Sourcing::AiFinder` impl wraps
      a Claude API call to discover offers from suppliers not in
      the catalog. UI in Phase C calls `Finder::find` so the
      switchover is invisible.
- [ ] Audit log table `sourcing_queries` (later) records prompt /
      response / chosen-offer for traceability.
- [ ] Deferred until catalog sourcing (Phases A–C) is in
      production and we have feedback on what mechanics actually
      ask for.

**Phase F — Mobile (parking-lot)**

- [ ] Mirror the sourcing UI in the mobile React/Capacitor app
      so a mechanic can source from the bay floor. Coordinated
      via the mobile submodule pin-bump dance. Out of scope for
      Phase A–D; track here so we don't lose it.

**Open decisions — LOCKED 2026-04-26**

1. **Cost model.** Single `unit_cost` per `supplier_product` row. No
   MSRP layer, no `bulk_discount` / `bulk_threshold` columns —
   Smitty's existing `markup_pct` on JobPart drives retail. Simpler
   than Contractor-Quotes' richer pricing apparatus; if bulk
   pricing tiers come up later, model them as additional rows
   (one per quantity tier) rather than columns on a single row.
2. **PK + existing `products.supplier_id`.** PK is
   `supplier_product_id` per Smitty's `<table>_id` convention.
   Existing `products.supplier_id` stays as the "default / primary
   supplier" alongside the new `supplier_product` table — additive
   change, no migration of existing data.
3. **Cross-source supplier seed list.** No fixed seed. Users add
   suppliers via Settings as they sign up new vendors. Phase A.5's
   seed is a tiny demo set using the 10 OEMs already in patch 014
   so the engine has rankable data; everything beyond that is
   user-driven.
4. **Weights.** Re-tuned for a parts shop: price 50 % / availability
   + lead time 30 % / proximity 10 % / supplier rating 10 %.
5. **Attribution.** Ported `Sourcing.h` / `.cpp` carry a header
   comment naming Contractor-Quotes as the source plus the commit
   sha they were lifted from, so future syncs are easy.

---

### §4a Phase A — PO-driven inventory ledger

Move the inventory increment off the JobPart 1→2 transition and
onto the PO Receive flow, where it actually corresponds to stock
arriving at the shop. Add a row-level audit trail
(`inventory_movements`) so every increment / decrement is
explainable and reversible. Foundation for the asset-value /
carrying-cost work in §4a Phase B.

**Role gating (locked):** every flow that mutates inventory or
records a movement is restricted to **Administrator** and
**Parts Manager** roles. Office Manager and Mechanic see the
movements ledger read-only; the Receive button on a Purchase is
hidden for them. Privilege check via `Auth::canEdit("purchases")`
for Receive, `Auth::canEdit("products")` for manual adjustments
(if added later), and `Auth::canView("products")` for the
ledger view.

**Stage 1 — Schema**

- [ ] DDL patch `database/patches/019_inventory_movements.sql`
      — new table `inventory_movements`:
      ```sql
      CREATE TABLE inventory_movements (
          movement_id   SMALLINT PRIMARY KEY GENERATED BY DEFAULT AS IDENTITY,
          product_id    SMALLINT NOT NULL,
          delta         INTEGER  NOT NULL,           -- + receive, - pick / adjust
          source_type   VARCHAR(20) NOT NULL,        -- 'po_receive' / 'job_pick' / 'manual_adjust'
          source_id     INTEGER,                     -- purchase_id / job_part_id / null
          balance_after INTEGER  NOT NULL,           -- snapshot of products.units_in_stock right after this delta
          created_at    TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
          created_by    VARCHAR(60) NOT NULL
      );
      ```
      String column DEFAULTs avoided per `tasks/LESSONS.md`. Index
      on `(product_id, created_at DESC)` for the per-product
      ledger view query.
- [ ] FK on `inventory_movements.product_id` →
      `products(product_id)` in `database/seed_data.sql` (per Smitty
      convention).
- [ ] `database/schema.sql` updated to match.
- [ ] User runs the patch + ALS rebuild
      (`ApiLogicServer rebuild-from-database --db_url=… --project_name=.`)
      so `/api/InventoryMovement` is exposed.

**Stage 2 — `Inventory::applyDelta` writes a movement row**

- [ ] Extend `Inventory::applyDelta(productId, delta)` to also
      record a row in `inventory_movements`. Add overload /
      out-param so callers pass `source_type` and `source_id`
      explicitly:
      ```cpp
      bool Inventory::applyDelta(productId, delta, sourceType, sourceId);
      ```
      Backwards-compat stub `applyDelta(productId, delta)` that
      defaults `source_type = "manual_adjust"` and `source_id = -1`
      so existing call sites compile while we migrate them.
- [ ] Update existing call sites to pass real source info:
      - `MechanicDashboard::advancePartStatus` Pick:
        `source_type="job_pick"`, `source_id=jobPartId`.
      - `JobDetail::advancePartReceived` Pick: same.
- [ ] Capture `Auth::instance().currentUser().username` in
      `created_by`.

**Stage 3 — PO Receive triggers increment**

- [ ] New `Inventory::receivePurchase(purchaseId)` helper.
      Fetches all `purchase_items` for the PO, calls
      `applyDelta(product_id, +quantity, "po_receive", purchaseId)`
      for each. Sums total cost as a sanity log line. Returns
      success/failure summary so the caller can surface a Toast.
- [ ] PurchaseList / PurchaseDetail: when status changes
      `* → "Received"`, call `Inventory::receivePurchase(poId)`
      after the status PATCH succeeds. Skip if the previous
      status was already "Received" (idempotency — never
      double-receive). Show "Received N parts; inventory
      updated" Toast.
- [ ] Hide / disable the Receive button (or the "Received" status
      option in the dropdown) for users without
      `Auth::canEdit("purchases")`. Office Manager + Mechanic
      can view the PO; only Admin + Parts Manager flip the
      status.

**Stage 4 — Remove the JobPart 1→2 inventory side effect**

- [ ] `JobDetail::stockPart()` becomes a pure state transition —
      no more `units_in_stock += quantity`. The inventory was
      already incremented at PO Receive (Stage 3). Stock state
      on the JobPart now reflects "the part has been physically
      received and shelved for this job," but the dollar / count
      already moved at the PO step.
- [ ] Document the new semantics in CLAUDE.md §6 / DEVELOPMENT_LOG
      so future-me knows why `stockPart()` no longer increments.

**Stage 5 — Movements ledger view**

- [ ] Register `InventoryMovement` in `EntityRegistry.cpp`.
- [ ] ProductDetail: new "Movements" section / tab below the
      existing fields. Renders rows newest-first:
      ```
      Date         Δ      Balance   Source            By
      2026-04-30   +12    142       PO #523 received  parts
      2026-04-29   −1     130       Job #117 pick     mike
      ```
      Read-only for everyone with `canView("products")`. Cap to
      ~50 rows with a "Show more" if needed.
- [ ] CSS: reuse `.child-grid-table` for the rows.

**Stage 6 — Polish + docs**

- [x] Update `docs/USERS_GUIDE.md` Products section: how to read
      the Movements ledger, what `source_type` values mean. Adds an
      "Inventory Movements" subsection with column descriptions and
      an Action / icon legend (Bootstrap Icons).
- [ ] Update `docs/DEVELOPMENT_LOG.md` with a Feature section
      for the inventory ledger.
- [ ] Update `docs/BACKLOG.md` §4a Phase A to `[x]` and bump
      Phase B (asset value) into the priority list.

**Status note (post-Stage 5):** the panel is live with action-
oriented labels + icons (Picked from Inventory / Received into
Inventory / Stocked Manually / Manual Adjustment) and clickable
Ref links that open a mini Job or PO view. Stage 3 (PO Receive
writing `+qty` `po_receive` rows) is wired. Stage 4 (pulling the
inline increment from `JobDetail::stockPart()`) shipped as a
**partial** — instead of dropping the increment, it was migrated
to `applyDelta(+qty, "manual_stock", jobPartId)` so cash-buy /
walk-in stocking still works AND now writes a proper ledger row.

**Stage 4 follow-up — inline PO-creation from Stock workflow — LANDED**

Lockstep direction from session discussion: the *right* end-state
is "everyone goes through the PO process," but only if the user
doesn't have to leave the part-stocking screen, lose their place,
go create a PO, then come back. KISS for the user.

- [x] Stock button now opens a chooser dialog
      (`JobDetail::chooseStockMethod`) with two buttons:
      **Cash Buy** (today's path — `stockPart()` writes a
      `manual_stock` movement) and **Create PO** (new path
      below). Cancel dismisses.
- [x] "Create PO" path opens
      `JobDetail::showCreatePoFromJobPartDialog` pre-filled
      with the linked Product's primary supplier (with a
      combo override), description, qty, unit price. On
      save, POSTs Purchase status="Received" + PurchaseItem,
      calls `Inventory::receivePurchase()` directly (a
      fresh POST does NOT trip
      `PurchaseDetail::onRecordSaved`, so the receive must
      be driven explicitly), then PATCHes
      `JobPart.received = 2`. Net effect: one
      `po_receive` movement against a real PO.
- [-] "Existing PO" path — deferred. Partial-receive
      semantics on a Purchase (one line received while
      others are still outstanding) is an unresolved design
      question; defer until that decision lands.
- [-] Deprecating `manual_stock` from the source_type
      vocabulary — out of scope. It stays as the explicit
      "cash buy with no paper trail" path.

**Stage 4b — Inline PO creation from Add JobPart Source flow — LANDED** *(parallel to Stage 4)*

User direction: *"From Jobs, when adding a part to a Job,
when sourcing, admin and / or mechanic should be able to
create a PO right there and set the process into motion."*
Different entry point from Stage 4 (which gates the Stock
button). This stage gates the **Source** button inside the
Add JobPart dialog — the moment of *committing to acquire*
the part is the right moment to drive the PO.

**Stages:**

- [x] Extracted `showCreatePoConfirmDialog` from
      `src/ProductDetail.cpp` into
      `include/PoCreate.h` / `src/PoCreate.cpp`. Free
      function `PoCreate::open(host, productId, productName,
      offer, qty, onDone)`; `onDone` fires with the new PO
      id (or cancellation / error) so callers can do
      follow-up work. ProductDetail's "Source Now" →
      "Create PO" flow now goes through it; behaviour
      unchanged.
- [x] Extended `SourcingDialog::open()` to accept a
      `std::vector<RowAction>` instead of a single
      pickLabel + callback. Each row in the offers table
      renders one button per action. Both call sites
      updated:
      - ProductDetail uses one action ("Create PO" —
        primary, blue).
      - JobDetail's Source flow uses two actions: **Use**
        (cost-only update) and **Create PO** (calls
        `PoCreate::open` against the offer; on success
        copies cost AND flips `createdPoFlag` so the save
        handler writes received=1).
- [x] Save handler in `showAddPartDialog` and
      `showEditPartDialog` reads `*createdPoFlag` and sets
      `attrs["received"] = 1` when true. JobPart lands in
      "on order" state, awaiting Stock from the same
      mechanic later.
- [x] Auth gate: `UserRole::Mechanic` now has
      `grant("purchases", true, true, false)` —
      view + edit, no delete. Sidebar Purchases entry
      visible to mechanics so they can follow their drafts
      through admin review.
- [x] **Admin review step.** A PO created via inline
      sourcing lands in `status = "New"`. Mechanic cannot
      advance status; only an admin can flip "New" →
      "Ordered" via a header "Approve & Order" button.
      - [x] Admin-only "Approve & Order" button on
            PurchaseDetail header, visible only when
            current status is "New". Opens a confirm
            modal showing supplier + line count + total,
            then PATCHes status to "Ordered" + stamps
            `purchase_date = CURRENT_DATE` if unset.
      - [x] Non-admin users (Mechanic, OfficeManager,
            PartsManager) see the status as a read-only
            `WText` (still styled as a `dialog-input` so
            it visually slots in) inside the edit dialog
            instead of a combo, so they can't bypass the
            review gate. The hidden field round-trips the
            current value so PATCH doesn't drop it.
      - [-] Mechanic-only "Cancel PO" button — **deferred**.
            See Decision C below; current resolution is
            option 3 (mechanic asks admin to cancel
            mistakes via the regular flow).
      - [-] Admin's PurchaseList default filter for "New"
            POs — **deferred**. Optional polish; can come
            in a follow-up commit.
- [ ] USERS_GUIDE.md: extend the Add Part section in
      Jobs to document the new "Create PO" path and what
      it does to the JobPart's state. Also document the
      admin review step in the PO section. **(Pending — doc
      sweep.)**

**Open decisions — LOCKED:**

A. **Mechanic purchases grant.** ✅ Confirmed: view+edit,
   no delete. Side effect: Purchases entry appears in
   mechanic's sidebar (acceptable — they need it to see
   their drafts under review).

B. **JobPart auto-`received=1` on Create PO.** ✅
   Confirmed: yes. Implemented via `createdPoFlag` shared
   state.

C. **PO `created_by` for cancel-by-creator scoping.** ✅
   Locked option 3: skip mechanic Cancel entirely. Zero
   schema change. Mechanic asks admin to cancel mistakes
   via the regular flow. Revisit if this turns out to be a
   common friction point.

**Out of scope for this stage:**

- Linking the JobPart row directly to the new Purchase
  via FK (no `job_parts.purchase_id` column today). The
  link stays implicit through `product_id`. Adding a
  direct FK is a separate schema change and arguably
  belongs to whatever resolves Stage 4's "Existing PO"
  picker option.
- Auto-advancing JobPart received state when the PO is
  later marked Received. Currently the mechanic still
  clicks Stock manually. Auto-advance is a workflow
  decision that needs its own conversation.

**Open decisions (locked, from §4a discussion)**

1. Carrying cost = weighted average vs last-PO cost. **Deferred
   to Phase B** — Phase A only ledger-tracks counts and per-PO
   costs. Phase B's report computes valuation from that history.
2. Manual inventory adjustments. **Admin only** for the
   foreseeable future (no UI in Phase A; if/when added, gate
   to `canEdit("products")`).
3. Negative stock. **Hard-clamp at 0** (already done by
   `Inventory::applyDelta`).
4. AI sourcing. **Out of Phase A.** Phase D.

**Migration notes**

- No backfill of existing inventory state. Today's
  `products.units_in_stock` values are the snapshot; the
  movements ledger starts empty and grows from the first PO
  Receive / Pick post-deploy. Older PO history isn't
  reconstructed.
- One-shot upgrade path: after the patch lands, the very next
  PO Receive writes the first row. Existing un-received POs
  continue to work — the increment fires on whatever
  status-change triggers the Receive flow.

---

### §4b — RMAs & supplier returns

A natural extension of §4a Phase A: today the ledger only
records stock leaving forward (to jobs) and arriving forward
(from POs, once Stage 4 wires up). It can't represent stock
flowing **back to a supplier** — defective parts, wrong-part
shipments, over-shipped lines, customer returns that the shop
forwards on. RMAs (Return Merchandise Authorizations) are the
business artifact that tracks those returns; the inventory
ledger gets a new `source_type` to record the physical
movement.

**Locked design decisions (from session discussion):**

1. **No "Requested" intermediate state.** Operator enters the
   supplier's RMA # at creation. New RMAs land in
   `Authorized` directly. Lifecycle:
   `Authorized → Shipped → Credited` (or `Refused`).
2. **Replacement linkage on Purchases.** When a supplier
   ships a replacement part, that PO carries
   `replacement_for_rma_id` (FK → `rmas.rma_id`, nullable).
   Reports can join in either direction: "this PO replaced
   RMA #N" / "RMA #N was replaced on PO #M".
3. **Accounting stays manual.** No automatic AP credit memo
   on `Credited`. Daily Close / GL hooks are a Phase B
   concern; revisit when §3 (BUSINESS_ACCOUNTING) lands.
   *(See note at bottom of this section.)*
4. **Phase 1 scope** = shop-stock RMAs **plus** a single
   `pick_return` movement type. Defects discovered during
   install go through a "Return to Stock" button on the
   JobPart row, which fires `applyDelta(+qty, "pick_return",
   jobPartId)`; the part is then RMA'd through the same
   shop-stock flow. Two ledger rows for the install-defect
   case. KISS, but honest.

**Schema (Phase 1, single patch + schema.sql update):**

```sql
rmas
  rma_id          SMALLINT  PK GENERATED BY DEFAULT AS IDENTITY
  supplier_id     SMALLINT  FK -> suppliers.supplier_id
  rma_number      VARCHAR(60) NOT NULL          -- supplier's #
  status          VARCHAR(20) NOT NULL          -- Authorized/Shipped/Credited/Refused
  created_date    DATE      NOT NULL DEFAULT CURRENT_DATE
  shipped_date    DATE      NULL
  credited_date   DATE      NULL
  notes           TEXT      NULL
  created_by      VARCHAR(60) NOT NULL

rma_items
  rma_item_id     SMALLINT  PK GENERATED BY DEFAULT AS IDENTITY
  rma_id          SMALLINT  FK -> rmas.rma_id
  product_id      SMALLINT  FK -> products.product_id
  quantity        INTEGER   NOT NULL
  unit_cost       NUMERIC(12,2)                 -- snapshot at return time
  reason          VARCHAR(20) NOT NULL          -- defective/wrong_part/damaged_shipping/over_shipped/customer_return/other
  source_purchase_id SMALLINT NULL              -- which PO it came in on, if known
  notes           TEXT      NULL                -- free-text for "other"

-- Linkage on existing purchases table (separate ALTER):
ALTER TABLE purchases
    ADD COLUMN replacement_for_rma_id SMALLINT NULL
    REFERENCES rmas(rma_id);
```

**New `inventory_movements.source_type` values:**

- `rma_return`  → delta = −qty, source_id = `rma_item_id`.
  Fired by `Inventory::shipRma()` when an RMA transitions to
  `Shipped`.
- `pick_return` → delta = +qty, source_id = `job_part_id`.
  Fired by the JobPart "Return to Stock" button when a
  mechanic finds a part defective mid-install.

**Stages:**

**Stage 1 — Schema**
- [ ] Patch `database/patches/021_rmas_and_pick_return.sql`:
      - `CREATE TABLE rmas`, `CREATE TABLE rma_items` (idempotent).
      - `ALTER TABLE purchases ADD COLUMN replacement_for_rma_id`.
      - FK guards in `DO $$ ... $$` blocks (no IF NOT EXISTS for FKs).
      - Index `(rma_id)` on `rma_items` for the detail-view query.
- [ ] Update `database/schema.sql` to mirror.
- [ ] Update `database/seed_data.sql` if needed (no FK changes
      to existing seeds expected — RMAs start empty).
- [ ] Note in commit message: rebuild ALS + restart the service.

**Stage 2 — Inventory plumbing**
- [ ] `Inventory::shipRma(rmaId)` — walks `rma_items`, calls
      `applyDelta(productId, -qty, "rma_return", rmaItemId)`
      per line. Sets `rmas.status = "Shipped"` and
      `shipped_date = CURRENT_DATE` on success.
- [ ] Extend `describeSourceType()` in `ProductDetail.cpp`:
      - `rma_return`  → `bi-arrow-return-left` /
        "Returned to Supplier".
      - `pick_return` → `bi-arrow-return-right` /
        "Returned from Job".
- [ ] Extend the Movements grid mini-view click-through:
      - `rma_return`  → mini RMA dialog (header + supplier
        + reason).
      - `pick_return` → mini Job dialog (reuse existing
        `showJobPickModal`, since source_id is still
        `job_part_id`).

**Stage 3 — RMA UI**
- [ ] Register `Rma` and `RmaItem` in `EntityRegistry.cpp`.
- [ ] `RmaList` page (mirrors `PurchaseList` shape) +
      sidebar entry under "Suppliers" group.
- [ ] `RmaDetail` page (mirrors `PurchaseDetail`):
      - Header: RMA #, supplier, status badge, dates.
      - Line items grid (Add / Remove / edit qty / reason).
      - Status transitions:
        - `Authorized → Shipped` → fires
          `Inventory::shipRma()`, decrements stock,
          writes `rma_return` movements.
        - `Shipped → Credited` (or `Refused`) — flag
          flip only, no stock movement.
- [ ] "Initiate RMA" button on `PurchaseDetail` — pre-fills
      supplier and offers PO line items as candidates with
      qty defaulting to received qty.

**Stage 4 — Pick Return**
- [ ] Add "Return to Stock" button on each JobPart row in
      `JobDetail`. Confirms with a small dialog (qty +
      reason free-text). Fires
      `Inventory::applyDelta(productId, +qty, "pick_return",
      jobPartId)` and reduces or removes the JobPart.
- [ ] Verify the pick_return ledger row renders correctly
      in the Movements grid (icon + label + click-through
      to the originating Job).

**Stage 5 — Replacement-PO linkage**
- [ ] On `RmaDetail` (status = Shipped or later), add
      "Receive Replacement" button → opens a small dialog
      to create a new `Purchase` row with
      `replacement_for_rma_id = currentRma`, then routes to
      the standard PO Receive flow.
- [ ] On `PurchaseDetail`, if `replacement_for_rma_id` is
      set, show a small banner / link: "Replaces RMA #N".
- [ ] On `RmaDetail`, if any Purchase has
      `replacement_for_rma_id = thisRma`, show the linked
      PO(s) in a small "Replacements" panel.

**Stage 6 — Polish + docs**
- [ ] Update `docs/USERS_GUIDE.md` Products → Movements
      legend with the two new actions; add a new
      `## RMAs (Returns)` section under Purchase Orders.
- [ ] Update `docs/DEVELOPMENT_LOG.md` with a Feature
      section once Stage 1-5 land.
- [ ] Update `docs/BACKLOG.md` to flip §4b to in-progress
      / done as stages land.

**Accounting hook (deferred — note for §3 BUSINESS_ACCOUNTING):**

When the accounting module lands, add a Daily Close hook
that, for each RMA transitioning to `Credited`:

- Posts an AP credit memo against the supplier
  (`accounts_payable` debit, inventory credit-of-credit
  reversal).
- Sets `rmas.gl_posted = TRUE` (new column at that time).

Until then, the operator marks an RMA `Credited` when the
supplier's actual credit arrives in their books, and
reconciles in the supplier's accounting system manually.

**Open items / known gaps**

1. **Customer-return → RMA flow.** A customer brings back a
   defective part. The customer-return half of that (refund,
   POS-side credit) is out of scope here; we just expose
   `customer_return` as one of the `reason` enum values so
   the shop can RMA it forward without inventing a parallel
   tracking model.
2. **Multi-line RMAs across suppliers.** An RMA is
   single-supplier. If three different suppliers' parts go
   back in the same week, that's three RMAs.
3. **RMA cancellation.** Not modeled. Status `Refused` covers
   "supplier rejected our return". A pre-Authorized cancel
   doesn't exist because we skipped the Requested state.
   Acceptable trade-off for KISS.

---

### §4c — Bin / location pillbox (Phase 0)

The Movements ledger now answers *what moved* and *who moved
it*; it doesn't answer *where the part lives*. Phase 0 adds
that — one canonical bin location per product, stored as a
JSONB pillbox payload, with the type vocabulary editable in
a new Settings → Inventory tab.

**Locked design decisions (from session discussion):**

1. **Pillbox UI** — each product carries an ordered sequence
   of pills, where each pill displays the **type keyword
   concatenated with the free-form value** (space-separated,
   no brackets / no colons). Examples: `Site Yard1`,
   `Room 3`, `Bin 12`. The user composes only as many
   levels as they actually use — a small product might be
   just `Bin 12`. Internally the pill still has separate
   `type` and `value` fields in JSONB; only the rendering
   is concatenated.
2. **Storage = JSONB column on `products`.** Single column,
   no new tables. Shape:
   ```json
   [{"type":"Site","value":"Yard1"},
    {"type":"Room","value":"3"},
    {"type":"Bin","value":"12"}]
   ```
3. **Type vocabulary lives in `app_config`.** A single row
   keyed `inventory.location_types` whose value is a JSON
   array of strings. Default seed:
   `["Site","Room","Aisle","Rack","Shelf","Bin"]`.
   Editable from Settings → Inventory tab. Adding a type
   doesn't require DDL; deleting one that's still referenced
   by some product's pillbox is allowed (the existing data
   keeps that pill, the type just doesn't appear in the
   dropdown for new entries).
4. **Set-style validation.** Each type appears **at most
   once** per product. Enforced client-side on save in the
   pillbox widget. Order is preserved (the user picks the
   order broad → narrow).
5. **Single-bin only.** One location per product. Multi-bin
   / per-bin quantity is **deferred to Phase 1** — if/when
   needed, the migration is: create `product_locations`
   table, seed one row per product from `bin_location` +
   `units_in_stock`, drop `bin_location`.

**Schema (Phase 0, single patch):**

```sql
ALTER TABLE products
    ADD COLUMN bin_location JSONB NULL;

INSERT INTO app_config (config_key, config_value)
VALUES ('inventory.location_types',
        '["Site","Room","Aisle","Rack","Shelf","Bin"]')
ON CONFLICT (config_key) DO NOTHING;
```

ALS rebuild + restart required so the JSONB column reflects
as a JSON-typed attribute on the Product resource.

**Stages:**

**Stage 1 — Schema**
- [ ] Patch `database/patches/022_products_bin_location.sql`
      with the ALTER + INSERT above. Idempotent.
- [ ] Mirror in `database/schema.sql` (column add only —
      `app_config` seed lives in `seed_data.sql`).
- [ ] Update `database/seed_data.sql` to insert the default
      `inventory.location_types` row for fresh installs.
- [ ] Note in commit message: rebuild ALS + restart.

**Stage 2 — Pillbox widget**
- [ ] New `include/PillboxLocationEdit.h` /
      `src/PillboxLocationEdit.cpp` reusable Wt widget.
- [ ] Two modes: **edit** (chips + dropdown + value input
      + add/remove buttons) and **view** (chips only,
      no controls). Constructed with the current types
      list (pulled from `AppSettings`).
- [ ] Parses the JSONB payload on construct;
      `serialize()` returns the JSONB string for save.
- [ ] Set-style validation: dropdown filters out types
      already present in the pillbox.
- [ ] Empty-state: "Click + to set a location."

**Stage 3 — ProductDetail integration**
- [ ] Add `bin_location` to the Product entity in
      `EntityRegistry.cpp` (type "JSONB" — needs a new
      `customEditField` override to swap in the pillbox
      widget for that one column).
- [ ] Read view: render the pillbox in view mode.
- [ ] Edit dialog: replace the default WLineEdit with the
      pillbox in edit mode; serialize on save.

**Stage 4 — ProductList + Sourcing + Pick read-only display**
- [ ] New "Location" column on ProductList showing the
      compact pillbox (or "—" if unset).
- [ ] Sourcing modal / JobDetail's part-pick interaction
      shows "Find at: …" using the pillbox.

**Stage 5 — Settings → Inventory tab**
- [ ] New tab in the Settings page: "Inventory".
- [ ] Editor for `app_config.inventory.location_types`:
      reorderable list of strings, add / rename / remove.
      Save writes the JSON back to app_config.
- [ ] Toast on save; AppSettings reloads after write so
      open Product edit dialogs pick up the new vocabulary
      next time they open.

**Stage 6 — Docs**
- [ ] Update `docs/USERS_GUIDE.md` Products section: add a
      "Location" subsection covering the pillbox UX and the
      Settings → Inventory tab.
- [ ] Update `docs/DEVELOPMENT_LOG.md` once the stages
      land.
- [ ] Update `docs/BACKLOG.md` to flip §4c to in-progress
      / done as stages land.

**Open items / explicit non-goals**

1. **Multi-bin per product.** Out of Phase 0. Migration
   path documented in decision #5 above.
2. **`inventory_movements.location_id`.** Not added in
   Phase 0 — location is a product attribute, not a
   movement attribute, when each product has exactly one
   bin. Lands with Phase 1.
3. **Bin-aware Pick / Receive flow.** Phase 0 just
   *displays* the location to the mechanic; Phase 1's
   per-bin qty is what makes the prompt meaningful.

---

### §4d — PO Detail UX, lifecycle gates, line routing, auto-refill

User direction (2026-04-27 session): "I need to see PO lines in
the PO detail screen. A summary as well, use standard card styles
on the three parts. Parts Manager and admin can perform these
operations. If a PO is submitted, it should not be updatable
except for receiving into inventory. If a PO is being reviewed
for approval by the admin, then they can update it, add
additional lines, for both inventory, and for the primary Job
linked to the PO. So, when receiving, we need to be able to route
a part being received to a Job directly, or to Inventory if
that's what it's for. Later we'll produce auto POs to refill
depleted stock to levels weekly, and those POs although
auto-generated by an agent process, will still need an approval
by admin or Parts Manager."

The schema already has a `job_purchases` join table (M:N). It is
unused by any UI today. §4d treats that table as the linkage
primitive for "this PO is for Job #N" instead of inventing a new
`purchases.job_id` column.

**Phase 1 — PO Detail summary card (visual)** *(small, ship now)*

- [x] New `PurchaseDetail` summary card sits at the top of the
      page (above the existing `.detail-form` field grid).
      Shows: PO #, supplier, status pill, PO Date, Expected Date,
      line count, total cost. Uses the existing `.detail-form`
      card class for visual consistency.
- [x] Existing PurchaseDetail field form keeps its `.detail-form`
      card; line items grid keeps its `.child-grid-section`. Net
      effect: Summary, Details, Lines as three cards.

**Phase 2 — Status-locked edit + Auth tighten — LANDED**

- [x] Edit button hidden on `PurchaseDetail` when status is
      anything other than `New`. Submitted/Ordered/Partial/
      Received POs are read-only at the field level.
- [x] Approve & Order button hidden when status is not `New`
      (was already admin-only; now also status-gated).
- [x] Edit button restricted to Administrator and Parts Manager
      even when status is `New`. Mechanic + Office Manager are
      view-only on existing POs.
- [x] Mechanic still creates drafts via the inline-sourcing flow
      on Add JobPart (Stage 4b path). They just can't edit them
      after.
- [x] When status >= Ordered, the only operation is Receive
      (status -> Received), which now runs through Phase 4's
      per-line receive UI.

**Phase 3 — Linked Jobs displayed via line routing — LANDED**

D1 LOCKED 2026-04-27: linkage is **per PO line** via
`purchase_items.route_to_job_part_id`, not per PO header. A PO
can therefore reference zero, one, or many Jobs naturally — the
linked-Jobs set is computed by joining each routed line to its
JobPart -> Job. The legacy `job_purchases` table stays in place
for now (still written by `JobList.cpp:485` during Job-create
flows that pre-date this design); future cleanup can drop the
table when the legacy path retires.

- [x] `PurchaseDetail` summary card adds a "Linked Jobs" line
      showing the deduped set of Jobs touched by this PO's
      routed lines (e.g. `Linked Jobs: #42, #57`). Each Job # is
      clickable -> `JobDetail`. Hidden entirely on
      inventory-only POs (no clutter when there's nothing to
      link). Source: PurchaseItem.route_to_job_part_id ->
      job_parts.job_id, deduped client-side.
- [x] No schema change. No EntityRegistry change beyond the
      Phase 4 PurchaseItem additions.
- [ ] No "Linked Job" picker on the PO Edit dialog — linkage
      is a per-line property, edited via the per-line Route
      column in Phase 4's grid.

**Phase 4 — PurchaseItem routing + per-line status — LANDED**

Schema patch 021 + C++ wiring shipped together (state machine,
per-line Receive UI, Inventory::receivePurchase per-line aware,
Stage 4b deferred-PATCH route_to_job_part_id back-fill). User
must run patch 021 + ALS rebuild + Smitty rebuild + service
restart. The checkbox list below remains for traceability.



Two related additions to `purchase_items` go in one DDL patch:
**routing** (which destination this line goes to on receive) and
**per-line status** (the line's own lifecycle, so partial-receive
is honest about which lines have actually arrived). Per-line
status uses **Option A — PO-side lifecycle only** (`pending` /
`ordered` / `received` / `cancelled`); Pick / Install / Inspect
states stay on `JobPart.received` and the line items grid joins
to that at render time for the end-to-end pipeline view.

- [ ] DDL patch `database/patches/NNN_purchase_items_status_and_route.sql`:
      ```sql
      ALTER TABLE purchase_items
          ADD COLUMN IF NOT EXISTS status                VARCHAR(20),
          ADD COLUMN IF NOT EXISTS route_to_job_id       SMALLINT NULL,
          ADD COLUMN IF NOT EXISTS route_to_job_part_id  SMALLINT NULL;
      UPDATE purchase_items SET status = 'pending' WHERE status IS NULL;
      ```
      No string-literal `DEFAULT` per LESSONS.md (the app always
      writes status). Routing columns nullable (NULL = inventory,
      the default). FK guards in `DO $$ ... $$` blocks per project
      convention. Mirror in `database/schema.sql`. Backfill makes
      every existing line `pending` so old POs aren't stuck in
      undefined limbo.
- [ ] `EntityRegistry.cpp` — extend PurchaseItem with `status`,
      `route_to_job_id`, `route_to_job_part_id`.

**Status state machine:**
- [ ] New PO created via any path → all lines start `pending`.
- [ ] Admin clicks Approve & Order on a `New` PO → batch PATCH
      flips every `pending` line on that PO to `ordered`. Header
      goes `New` → `Ordered` as today.
- [ ] Per-line Receive (in the new receive UI below) → that line
      flips `ordered` → `received`. Header auto-aggregates after
      every line transition:
      - All lines `received` → header `Received`.
      - Some `received`, rest `ordered` → header `Partial`.
      - All still `ordered` → header stays `Ordered`.
- [ ] Cancel paths: a per-line cancel flips that line to
      `cancelled`. If every line is `cancelled`, header → `Cancelled`.
      (Mechanic Cancel-PO is still deferred per Decision C, so for
      now only Admin / Parts Manager can cancel.)

**Routing UI:**
- [ ] On `PurchaseDetail`, the Line Items grid gets a **Route**
      column. While status is `New`, admin + Parts Manager can
      edit per-line: Inventory (default) / Job #M ▸ JobPart #N.
      Picker lists every open Job's JobParts (filter by
      Job.status not in ("Completed","Invoiced")). No header-
      level pre-filter — D1 says a single PO can mix Jobs.
- [ ] The same grid shows the per-line **Status** as a small pill
      next to the line, sharing the existing `dash-job-badge` +
      `badge-*` classes. For lines whose status = `received` AND
      that have a `route_to_job_part_id`, the pill also surfaces
      the linked JobPart's `received` state ("received → picked"
      etc.) so the user sees the full pipeline in one place.
- [ ] **Inline-PO-from-Source auto-routes back to the originating
      JobPart.** Stage 4b's Create PO action runs while the Add
      JobPart dialog is still open and the JobPart hasn't been
      saved yet, so we can't set `route_to_job_part_id` at PO
      POST time. Use the deferred-PATCH pattern: extend the
      existing `createdPoFlag` shared state into a struct
      capturing the new PurchaseItem id; after the Add JobPart
      Save handler succeeds (and we have the new JobPart id),
      PATCH the PurchaseItem with `route_to_job_part_id =
      newJobPartId`. If the user cancels the Add JobPart
      dialog after creating the PO, the line stays unrouted
      (effectively "to inventory") — better than orphaning the
      whole PO. Admin can re-route from the PurchaseDetail grid
      while status is still `New`.

**Receive flow:**
- [ ] Replace the bulk-receive that today fires when the header
      flips to "Received" with a **per-line receive UI**: each
      `ordered` line gets a Receive button on its grid row. The
      button transitions just that one line, runs the appropriate
      side effect (inventory bump for unrouted, JobPart received-
      flip for routed), and updates the header status via the
      auto-aggregation rule above.
- [ ] `Inventory::receivePurchase` (the bulk variant) becomes a
      thin wrapper that walks every `ordered` line and calls the
      per-line receive in turn. Keep it as a convenience for the
      "everything arrived in one shipment" case.
- [ ] For routed lines: receive operator picks per-line
      destination per D3 — **landed at receiving** (PATCH
      the JobPart's `received = 2`, default) or **skip
      inventory** (PATCH `received = 3`). Either way, DO NOT
      bump `products.units_in_stock` (the line is earmarked
      for a Job, not for general inventory). Write the
      movement row with `source_type = "po_receive_to_job"`
      so the ledger shows the line's true destination.
- [ ] For unrouted lines: current `po_receive` behaviour
      (applyDelta to inventory).

**Auth + visibility:**
- [ ] `Auth` gate on the receive button (per-line): Admin +
      Parts Manager only. Mechanic / Office Manager view-only on
      `PurchaseDetail`.

**§4d Phase 4b — Admin "+ Add Line" on PurchaseDetail — LANDED**

User direction (vendor-on-the-phone scenario): admin needs to
drop "20 leafsprings" onto an in-progress draft PO without
leaving the page. This is the manual-add path that complements
the inline-Source flow on Add JobPart (which auto-creates the
line plus routes it).

- [x] "+ Add Line" button on the Line Items title bar of
      PurchaseDetail. Visible to Admin + Parts Manager AND only
      while status == New. Hidden once the PO is approved
      (matches Phase 2's overall lock-on-Ordered rule).
- [x] Add Line dialog with Product (combo, sorted), Quantity,
      Unit Cost (currency-attached). Lines land at status =
      "pending" so they pick up the bulk pending -> ordered
      flip on Approve & Order alongside the existing lines.
      Routing is Inventory-only in this dialog; route-to-job
      lines come in through Add JobPart -> Source -> Create PO
      on the Job side.
- [x] After save, `recomputePoTotal` walks every non-cancelled
      line, sums qty * unit_cost, and PATCHes
      `Purchase.total_cost` so the summary card stays in sync.
      Then `loadRecord` refreshes the page; DataBus events on
      PurchaseItem + Purchase propagate to other watchers
      (Pending Deliveries doesn't fire yet because the line is
      pending, not ordered).
- [-] Edit Line / Remove Line — deferred. The current path
      (admin asks, "what's wrong?") is to delete the whole PO
      and re-create it. If that becomes a friction point, add
      Edit Line first (per-line qty / unit cost / route on
      pending lines only); Remove Line is a separate destructive
      affordance and worth a separate decision.

**§4e — Dashboard Pending Deliveries panel — LANDED**

Replaced the JobPart-centric Pending Part Deliveries panel
(`received == 0`, button labelled "Receive" but actually meant
"mark as ordered") with a PO-driven panel sourced from
`PurchaseItem.status='ordered'`. Conceptual tie to
`inventory_movements`: a row drops off this panel the moment its
line transitions to `received`, so the panel is "what's still
missing from the ledger."

- [x] `Dashboard::loadPendingParts` rewritten to fetch
      `PurchaseItem` rows with `status='ordered'`, joined to
      Purchase + Supplier + Product for display. Pre-Phase-4
      installs (status NULL) get an empty panel until the
      operator applies patch 021.
- [x] Per-row display: Product, Qty, Supplier, Expected (with
      "(overdue)" suffix if past today), To (Inventory or
      JobPart #N). Sort: overdue first, then expected_date asc,
      then line value desc.
- [x] Row click navigates to `PurchaseDetail` for the parent PO
      so the operator can review or run the per-line Receive
      flow (Phase 4 UI). New
      `SmittyApplication::navigateToPurchaseDetail` method
      added; old "Receive" button on the panel removed (the
      Phase 4 per-line UI on PurchaseDetail is the only path now).
- [x] Dashboard auto-refresh now also subscribes to `Purchase`
      and `PurchaseItem` DataBus events so creating / approving
      / receiving a PO updates the panel without a manual
      refresh.

**Phase 5 — Auto-PO refill agent** *(spec ready; pick when accounting lands)*

A cron-driven scan that drops draft replenishment POs into
status=New for admin / Parts Manager to review and Approve.
Closes the loop: every product whose stock dips below its
reorder_level gets sourced automatically, but a human still
clicks Approve & Order, so the agent never fires a PO without
review.

**Locked design decisions:**

- **Where it runs.** Standalone CLI binary
  (`build/smitty-refill-agent`) invoked from cron once a week.
  Reasons: testable in isolation, no impact on the long-running
  Wt server process, restartable independently of Smitty,
  failures are loud (cron mailing the operator on non-zero
  exit). Deferred: in-process Wt timer would tangle the Wt
  session lifecycle and force a long-running connection that
  doesn't scale to multi-tenant.
- **Trigger condition.** A product is a candidate iff:
  - `discontinued = 0`
  - `reorder_level > 0`
  - `supplier_id IS NOT NULL` (no PO without a supplier)
  - `units_in_stock < reorder_level`
  - There is **no** open PurchaseItem for this product on a
    PO whose header status is in (`New`, `Ordered`, `Partial`).
    Querying via `PurchaseItem.status IN ('pending','ordered')`
    is the same set after Phase 4. This guards against
    double-ordering: if last week's draft is still under review
    and stock is still low, the agent skips that product this
    week.
- **Approval.** Same as the manual draft path. Drafts land in
  status=New, the human clicks Approve & Order. No automation
  on the approval gate.
- **Supplier choice.** v1 uses `products.supplier_id` (the
  primary). v2 can call `Sourcing::Engine::findBestOffers`
  against `supplier_product` rows for smarter sourcing.
  Deferred — works fine without it.

**Open questions for user (answer before code):**

P5-Q1. **Quantity to order.** Smitty's `products` table has
       `reorder_level` but no target / max level, so "restock
       to what?" is open. Options:
       (a) `reorder_level - units_in_stock` — restocks to
           exactly the threshold. Minimum order, frequent
           reorders.
       (b) `2 * reorder_level - units_in_stock` — restocks to
           2x the threshold. Reasonable buffer.
       (c) Add a `target_level` column on products and let
           the user set a per-product target. Most flexible,
           more setup.
       Recommendation: **(b)** for v1 (simple, sensible
       default). Add (c) later if users want per-product
       control.

P5-Q2. **Cadence.** Once a week (Sunday night cron) is the
       initial proposal — gives admin a fresh review queue
       Monday morning. Daily would be noisier; monthly would
       miss seasonal demand spikes. Lock weekly?

P5-Q3. **Empty-PO behavior.** If no products fall below
       threshold for a given supplier, the agent shouldn't
       create an empty PO. Single supplier with one line is
       fine — single-line restock is normal. Confirm.

P5-Q4. **Notes content.** The notes field on the auto-generated
       PO should make it obvious it's machine-generated.
       Proposal:
       `"[auto-refill] generated <YYYY-MM-DD HH:MM>;
        threshold restock to <target>"`
       — humans can edit before approve. Acceptable?

P5-Q5. **Run-as identity.** Auto-generated PurchaseItem rows
       need a `created_by` (none today, but inventory_movements
       has it). The PO itself doesn't track creator; the line
       items don't either. For v1, just stamp the notes field
       with `[auto-refill]`; in a follow-up, add a
       `purchases.created_by` column (also unblocks Decision
       C's mechanic Cancel scoping). Confirm the v1 stamp
       approach is enough for now.

**Stages:**

A. **CLI binary + bootstrap**
- [ ] New target `smitty-refill-agent` in `CMakeLists.txt`,
      sourced from a small `src/refill_agent/main.cpp` plus
      whatever shared headers it pulls in (`ApiClient`,
      `AppSettings`, json). Reuses the existing ALS plumbing,
      so the agent talks the same JSON:API as Smitty itself.
- [ ] Argv: `--dry-run` (compute and log the would-be POs,
      don't POST), `--threshold-multiplier <N>` (override the
      P5-Q1 default if locked), `--als-url <URL>`, `--verbose`.
      Defaults via env vars (`ALS_API_URL`) like Smitty.
- [ ] Logging: stdout for normal operation, stderr for errors
      so cron picks them up.

B. **Candidate selection**
- [ ] `selectCandidateProducts()` — fetches `/api/Product`,
      filters per the trigger conditions above, builds a
      `vector<Candidate>` keyed by supplier_id.
- [ ] `hasOpenPurchase(productId)` — fetches
      `/api/PurchaseItem?filter[product_id]=N` and walks for a
      `status` in (`pending`, `ordered`). Simple per-product
      check; runs once per candidate. Cache the supplier+status
      keyed lookup if performance becomes an issue.

C. **Group + draft**
- [ ] `groupBySupplier(candidates)` -> `map<supplierId,
      vector<Candidate>>`.
- [ ] For each supplier:
      - POST `/api/Purchase` with `supplier_id`, `status='New'`,
        `notes='[auto-refill] generated …'`, `total_cost = sum`.
      - For each candidate: POST `/api/PurchaseItem` with
        `purchase_id`, `product_id`, `quantity` (per P5-Q1),
        `unit_cost` (last PO unit_cost for this product if any,
        else `products.unit_price`), `status='pending'`.
- [ ] On any POST failure: log loudly to stderr with the
      product / supplier id, continue with the next supplier
      (don't halt the whole run on one failure).

D. **Cron + ops**
- [ ] Document in `docs/DEVELOPMENT_LOG.md` (or a new
      `docs/OPS_RUNBOOK.md` if it grows): the cron line
      (`@weekly /opt/smitty/build/smitty-refill-agent
      >>/var/log/smitty-refill.log 2>&1`), how to dry-run, how
      to read the log.
- [ ] Optional dashboard hint (not blocking): a small status
      tile on the Dashboard "Last refill run: <timestamp>, N
      drafts created, M skipped (already on order)." Sourced
      from a new `app_config` row the agent stamps at the end
      of each run. Defer if not needed.

E. **Visibility**
- [ ] PurchaseList default filter / sort: surface `status=New`
      POs with `notes LIKE '[auto-refill]%'` first so admin
      sees them at the top of the review queue Monday morning.
- [ ] Optional: a "Generated" badge column on PurchaseList that
      reads "auto" if the notes prefix matches, else blank.

**Out of scope (defer):**
- Multi-supplier sourcing for the same product (use
  `Sourcing::Engine` instead of `products.supplier_id`).
- Demand-driven thresholds (recompute reorder_level from
  weekly burn rate). Manual setting is fine for v1.
- Email-the-admin "review queue is N items" digest. Cron
  mail on non-zero exit covers errors; volume on the queue
  itself is admin-pulled, not pushed.
- Any change to the operational schema beyond Phase 5
  Q1/Q5 follow-ups (target_level, purchases.created_by).

**Open decisions:**

D1. **JobPart <-> PO linkage shape. LOCKED 2026-04-27.**
    Linkage is **per line**, not per PO header. A JobPart
    is linked to a specific PurchaseItem via the Phase 4
    `purchase_items.route_to_job_part_id` column. A PO can
    therefore touch zero, one, or many Jobs naturally — the
    set of linked Jobs is the SELECT DISTINCT of Jobs
    referenced through routed lines. The legacy
    `job_purchases` join table remains in schema for the
    JobList.cpp:485 path (Job-create-time linkage) but is
    not the going-forward model; future cleanup can drop
    it once that legacy path retires.

D2. **Routing destination granularity. LOCKED 2026-04-27.**
    Per-line `route_to_job_part_id` (precise — the receive
    operator doesn't have to pick a JobPart at receive time).
    NULL = "to inventory," and that's the common case for
    bulk restocks (e.g. "12 DD-15 oil filters" — a single PO
    can mix routed lines and inventory lines on the same
    supplier order). `route_to_job_id` stays as a guard for
    cases where the Job is known but the JobPart isn't tracked
    yet — left for Phase 4 receive UI to decide if it's
    needed in practice.

D3. **What does a JobPart look like after PO routing?
    LOCKED 2026-04-27.** Receive operator picks per line at
    receive time: either **landed at receiving** (JobPart
    `received = 2`, mechanic Picks normally) or **skipped
    inventory entirely** (JobPart `received = 3`, goes
    straight to "picked"). The receive UI surfaces both as
    a two-option button on routed lines; default to `= 2`.
    `Inventory::applyDelta` writes the matching movement row
    in either case; the line never bumps
    `products.units_in_stock` because it's earmarked for
    a Job, not for general inventory.

---

### §5 — Fleet Dispatcher integration

Cross-app integration with the Fleet Dispatcher app (separate
repo, separate schema, same Postgres instance). Full context and
Smitty's design position live in
`docs/INTEGRATION_SMITTY.md` — read it before touching any of
these stages, especially the "Smitty response v1" section at the
bottom which encodes the six answers + three cross-cutting
constraints.

**Cross-cutting constraints (locked, from session):**

- **C.1 additive-only.** No `DROP COLUMN`, no renames, no type
  changes on shared tables. `ADD COLUMN IF NOT EXISTS` +
  `CREATE TABLE IF NOT EXISTS`. Deprecated fields go nullable
  and stop being written; they do not leave.
- **C.2 shared column set + semantics + VIN correlation.**
  *(softened per Fleet response v1 F.3, accepted in Smitty
  response v2 S.1.)* The extended `vehicles` shape,
  `maintenance_schedules`, and `vehicle_out_of_service`
  carry identical column names, types (modulo native PK
  types), and semantics between Smitty and Fleet. **Native
  PK types + local FKs are allowed** — Smitty keeps
  `SMALLINT`, Fleet keeps `UUID`. The sync replicates by
  VIN, not by identical row images. Column-set drift is a
  bug; PK-type divergence is expected.
- **C.3 Fleet holds the on/off switch.** Smitty is a passive
  party. When Fleet's admin toggles integration off, Smitty
  sees NULLs / stale mirror data. UI must degrade gracefully.
  No matching toggle on Smitty side.
- **C.4 Smitty is a profit center serving multiple customers;
  Fleet is one of them.** *(new from Fleet response v1 B.1–B.4.)*
  Fleet-owned rigs are a subset of Smitty's vehicles; third-
  party vehicles / jobs must never cross the boundary. The
  Fleet-token traffic is scoped by `customer_id = 'FLEET'` and
  by a set of Fleet-owned VINs (both enforced at the middleware
  layer, not client-supplied filters).
- **C.5 At-cost totals cross to Fleet; retail / markup stays
  in Smitty.** *(new from B.3.)* Parts + labor at-cost per line
  is exposed on the Fleet-token `/api/Job*` responses. Retail
  pricing (`unit_price`, `rate`, `amount`, `total_cost`,
  `estimated_cost`, `actual_cost`, per-line pricing) is
  stripped by the token-registry field-stripping profile
  before response. Phase 1 adds `job_parts.unit_cost` and
  `job_labor_items.hourly_cost` so at-cost is a real column,
  not a derivation.
- Also see `docs/DESIGN_DECISIONS.md` "Integration schemas are
  additive-only", "Integrated apps share identical DDL for
  shared tables" (softened decision body per S.1), and
  "Multi-tenant integrations use middleware-enforced scoping +
  field stripping" for the same rules in decision form.

**Locked answers to `docs/INTEGRATION_SMITTY.md §9`:**

1. Canonical granularity: **one VIN = one canonical vehicle**
   (power unit and trailer each their own row).
2. Maintenance intervals: **both mileage + days, whichever
   comes first**; `interval_miles` and `interval_days` both
   nullable, either or both may be set.
3. Sync mechanism: **JSON:API pull for Phase 1-3, Kafka in
   Phase 5** — C++ Wt frontend never touches Kafka; Python-
   side producer on ALS row-change hooks.
4. Valuation SoR: **Smitty publishes cost history; Fleet
   computes valuation.** Smitty does not roll up asset value.
5. Identity fallback: `vin` → `(customer_id, license_plate)`
   → `unit_number`. Fleet authoritative on VIN conflict.
6. Resource names + auth: `/api/Vehicle`, `/api/Job`,
   `/api/JobPart`, `/api/JobLaborItem`, `/api/JobNote`,
   `/api/MaintenanceSchedule` (new), `/api/VehicleOutOfService`
   (new). Auth via shared `X-Service-Token` header +
   LAN-only exposure in Phase 1-3; JWT deferred to Phase 5.

**Phase 1 — Schema + service-token middleware** *(updated for v2)*

Landing this phase is a prerequisite for everything else in
§5. Fleet applies the shared-column-set DDL to its schema in
lockstep (native UUID PKs, per S.1).

- [ ] Patch `database/patches/023_fleet_integration.sql`:
      - `ALTER TABLE vehicles ADD COLUMN IF NOT EXISTS`
        for `fleet_vehicle_id VARCHAR(60)` (renamed from
        `fleet_equipment_id` per Fleet F.6), plus
        `odometer_miles`, `odometer_as_of`,
        `operational_status`, `asset_type`, `dot_number`,
        `fuel_type`, `specs JSONB`. All nullable. No FK on
        `fleet_vehicle_id` — this is a mirror pointer,
        not a local relationship.
      - `ALTER TABLE customers ADD COLUMN IF NOT EXISTS role
        VARCHAR(20)` (from Smitty v2 S.3.a). Nullable.
        Values: `'fleet_house' | 'external' | 'owner_operator'`.
      - `ALTER TABLE job_parts ADD COLUMN IF NOT EXISTS
        unit_cost REAL` (from Smitty v2 S.3.d).
      - `ALTER TABLE job_labor_items ADD COLUMN IF NOT EXISTS
        hourly_cost REAL` (same).
      - `CREATE TABLE IF NOT EXISTS maintenance_schedules`
        per the DDL in `docs/INTEGRATION_SMITTY.md §R.4`.
        Local FK `vehicle_id → vehicles(vehicle_id)`
        added in a guarded `DO $$` block; no cross-app FK
        on `vin`.
      - `CREATE TABLE IF NOT EXISTS vehicle_out_of_service`
        with the same shape rules.
      - Indexes per the DDL section.
- [ ] Mirror in `database/schema.sql`. Additive, at the
      bottom of the Smitty extensions block. No changes to
      existing columns.
- [ ] Update `database/seed_data.sql`:
      - FK constraints for the two new tables. TRUNCATE
        list prepended with the new tables so a re-seed
        doesn't FK-violate.
      - Insert the `'FLEET'` house customer row
        (`customer_id='FLEET'`,
        `company_name='Fleet Dispatcher (house)'`,
        `role='fleet_house'`) per Smitty v2 S.3.a.
        **[Fleet confirm the id.]**
- [ ] ALS-side service-token middleware. Small Python
      shim in the ApiLogicServer project that:
      - Checks `X-Service-Token` against a **token registry**
        (env-var-loaded or on-disk JSON). Requests without a
        recognised token get 401.
      - Each token row carries a **scope**: `customer_id`
        (required) and an optional `vin_set`. Middleware
        rewrites read queries to `WHERE customer_id =
        <scope.customer_id>` on `/api/Vehicle` / `/api/Job`
        and equivalent joined filters on
        `/api/MaintenanceSchedule` +
        `/api/VehicleOutOfService` (Smitty v2 S.3.b).
      - Each token row optionally carries a
        **field-stripping profile** (Smitty v2 S.3.d): the
        Fleet token strips `jobs.{estimated_cost,
        actual_cost, parts_total, labor_total, job_total,
        discount_percent}`, `job_parts.{unit_price, amount}`,
        `job_labor_items.{rate, amount}` from responses.
        Keeps `unit_cost` and `hourly_cost` (the at-cost
        side per B.3).
      - Endpoints scoped by the middleware: `/api/Vehicle`,
        `/api/Job*`, `/api/MaintenanceSchedule`,
        `/api/VehicleOutOfService`. Other endpoints stay
        session-authenticated as today (mechanic /
        dispatcher UI).
- [ ] Populate `unit_cost` / `hourly_cost` on write going
      forward:
      - `job_parts.unit_cost`: pulled from
        `supplier_product.unit_cost` at Pick / Receive time
        (Smitty already has this in the sourcing engine).
        Set by `Inventory::applyDelta` +
        `Inventory::receivePurchase`.
      - `job_labor_items.hourly_cost`: from a new
        `app_users.hourly_cost REAL` column (additive) or
        a role-default in `AppSettings` until that lands.
        Simplest: `AppSettings::mechanicHourlyCost()`
        (single number) for Phase 1; per-mechanic in a
        later phase.
- [ ] Note in commit message: rebuild ALS + restart both
      services after applying the patch.

Deploy-order sanity for Phase 1 (in order):

1. `psql "$DB" -f database/patches/023_fleet_integration.sql`
2. `ApiLogicServer rebuild-from-database --db_url="$DB" --project_name=.`
3. Install the token-registry config / env var on the ALS
   side (Fleet supplies its `SMITTY_SERVICE_TOKEN` — Smitty
   generates + hands off).
4. `sudo systemctl restart als-smitty.service`
5. Smoke: `curl -gs -H "X-Service-Token: $TOKEN"
   http://localhost:5655/api/Vehicle | head -c 200`
   → should return Fleet-scoped vehicles only.
6. `sudo systemctl restart smitty-services.service` (no C++
   change in Phase 1; restart is precautionary).

**Phase 2 — Fleet → Smitty pull (mirror stays fresh)**

- [ ] `scripts/reconcile_from_fleet.py` — pulls
      `GET /api/Equipment` from Fleet, upserts into
      `smitty.vehicles` by VIN. Idempotent. Log-only on
      failure. Env-var `FLEET_API_URL` +
      `FLEET_SERVICE_TOKEN`.
- [ ] Systemd timer unit `scripts/smitty-fleet-pull.timer` +
      `.service` running the reconcile every 5 minutes.
      Install parallels the existing
      `scripts/smitty-services.service` shape.
- [ ] Log-scan check to add to
      `tests/USER_TESTING_SCENARIO.md`: "after N minutes,
      the last Fleet pull is recent and processed count is
      non-zero when Fleet has activity."

**Phase 3 — Smitty → Fleet publish (readable)**

- [ ] Verify Fleet's scheduled pull hits the same endpoints
      through the shared-token middleware — no Smitty code
      change beyond Phase 1. Coordinate the endpoint list +
      cadence with Fleet.
- [ ] Add a `resource: MaintenanceSchedule` and
      `resource: VehicleOutOfService` block to
      `model/app_model.yaml` if the ALS rebuild doesn't
      auto-surface them for the admin UI.

**Phase 4 — Maintenance UI on Smitty (first user-visible slice)**

- [ ] Register `MaintenanceSchedule` and
      `VehicleOutOfService` in `src/EntityRegistry.cpp`.
- [ ] VehicleDetail: new **Maintenance** tab.
      - Upcoming / Due / Overdue rows from
        `maintenance_schedules`, filtered by
        `vehicle_id = currentRecordId`.
      - Current OOS window (if any) from
        `vehicle_out_of_service` where `to_ts IS NULL`.
      - Mileage panel: `odometer_miles` + `odometer_as_of`
        with "No mileage on file" empty-state per C.3
        graceful-degrade rule.
- [ ] "Schedule Service" dialog: creates a
      `maintenance_schedules` row.
      `service_type` (dropdown of common truck-service
      values), `interval_miles`, `interval_days`,
      `next_due_on`, `next_due_odometer`. Either interval
      may be blank per locked Q2.
- [ ] "Start OOS" / "End OOS" header actions on
      VehicleDetail. Start creates a
      `vehicle_out_of_service` row with `to_ts NULL`; End
      PATCHes `to_ts = CURRENT_TIMESTAMP`.
- [ ] Update `docs/USERS_GUIDE.md` Vehicles section with
      the Maintenance tab + OOS controls.

**Phase 5 — Optional Kafka bridge**

Deferred by design — not part of the current planning
horizon. When it comes up:

- [ ] Python producer on ALS row-change hooks emitting
      `vehicle-service.v1` topic per the event contract
      in `docs/INTEGRATION_SMITTY.md §6`.
- [ ] Python consumer for `vehicle.v1` (odometer +
      operational status) that upserts into
      `smitty.vehicles`. Replaces or augments the Phase 2
      pull daemon.
- [ ] JWT handoff between the two ALS instances if the
      shared-token approach becomes limiting.

**Open items awaiting Fleet response (v2):**

- Confirmation of `'FLEET'` as the house-customer
  `customer_id` per Smitty v2 S.3.a. Fleet can pick a
  different value; it's a one-line seed change.
- Confirmation of `unit_cost` / `hourly_cost` column names
  + the NULL-legacy handling per Smitty v2 S.3.d. Column
  add is committed; the source-of-truth for the cost value
  is Smitty-internal and can adjust without changing the
  API contract.
- Fleet's answer to the RMA-vs-integration priority
  question in `docs/INTEGRATION_SMITTY.md §R.6`. Still
  open per Fleet's own note.

---

### Standardize detail-page header actions (top-right float)

Inventory across detail pages today is uneven — Customer / Product /
Order each put their Save button at the **bottom** of the form,
Vehicle and Purchase are read-only with an undiscoverable
`showEditDialog()`, and Job is the outlier that already has
Generate Invoice + Print pinned to the **top-right** of the
`.page-header`. Bring everyone to the Job pattern: page-level
action buttons and flag checkboxes live in a shared header
strip, right-floated next to the title.

**Target pattern (exemplar: `src/JobDetail.cpp:143-163`)**
- Title on the left of `.page-header`, a flex row of actions on
  the right (current class `.job-header-actions`, renamed to
  `.detail-header-actions` so it's not Job-specific).
- Primary action = `action-btn` pill; secondary =
  `action-btn action-btn-secondary`; destructive =
  `action-btn action-btn-danger`.
- Optional flag checkboxes (e.g. Product's Discontinued) live in
  the same strip, left of the buttons. One compact label, no
  surrounding form-group box.
- Save-status toast (currently in `.detail-save-bar`) moves into
  the strip too, left of the Save button, so inline success/error
  feedback stays near the action that produced it.

**Shared scaffolding**
- [x] Rename `.job-header-actions` → `.detail-header-actions` in
      `resources/css/smitty.css`; added `.detail-header-flag` and
      `.detail-header-status` rules for the flex-row-sized
      checkbox label and inline save-status toast. Orphaned
      `.detail-save-bar` block removed.
- [x] Added helpers on `EntityDetailView`:
      `ensureHeaderActions()`, `addHeaderAction(label, extra)`,
      `addHeaderFlag(label, checked)`, `addHeaderStatus()`, and
      `clearHeaderActions()`.
- [x] `EntityDetailView::buildUI()` caches `headerContainer_` so
      helpers drop widgets in without walking up from
      `titleText_->parent()`.

**Page-by-page migration**

- [x] **JobDetail** — hand-rolled button group replaced by
      `addHeaderAction("Generate Invoice", "job-invoice-btn")`
      and `addHeaderAction("Print", "job-print-btn")`. Removed
      the `dynamic_cast`-through-`titleText_->parent()` path and
      the now-redundant `jobActionGroup_` member.
- [x] **CustomerDetail** — bottom save bar removed; Save button
      lives in the header via `addHeaderAction("Save")` with
      `saveStatus_` from `addHeaderStatus()`.
- [x] **ProductDetail** — same Save relocation as Customer.
      Discontinued flag moved from inline form-grid widget to
      `addHeaderFlag("Discontinued", …)`; the field loop now
      skips the `discontinued` column entirely.
- [x] **OrderDetail** — Save moved to the header. Shipped /
      locked invoices skip the Save button creation entirely
      (preserves the previous hide-when-shipped rule); saveRecord
      no-ops on `!saveStatus_`. Ship-address pencil untouched
      (it's a section-level trigger).
- [x] **VehicleDetail** — override `populateFields` to call base
      then `addHeaderAction("Edit")` wired to `showEditDialog()`.
      Edit dialog reaches the `customer_id` combo via the
      existing `customEditField` override.
- [x] **PurchaseDetail** — mirror of Vehicle: `populateFields`
      override adds **Edit** → `showEditDialog()`.

**Acceptance / smoke test**
- [ ] Every detail page shows at least one action widget in the
      top-right of the header, with consistent pill styling and
      spacing.
- [ ] Customer / Product / Order Save produces a toast in the
      header strip (success/error colour matches the removed
      `.detail-save-bar` styling).
- [ ] Product's Discontinued checkbox toggles and persists on
      Save just as it did inline.
- [ ] Order's Save is hidden on a shipped invoice (as before).
- [ ] Vehicle and Purchase Edit buttons open the existing edit
      dialog and saves round-trip.

**Out of scope for this task** (tracked separately)
- List pages — user has explicitly queued those as the next
  pass after detail pages are consistent.
- Converting Vehicle / Purchase from dialog-edit to
  inline-editable (Customer-style). The Edit-button approach
  is the minimum change for standardization; the deeper UX
  question — "should read-only detail pages exist at all?" —
  is a separate design decision, not blocking this pass.



`src/CustomerDetail.cpp` previously had its own flat tab bar
(Orders / Vehicles / Jobs) using `.child-tab-*` classes. Brought
in line with the Job Detail pill tabs, factored into a shared
`.smitty-tab-*` namespace so Job, Customer, and Vehicle all
share one set of selectors.

- [x] Rename `.job-tab-*` → `.smitty-tab-*` in `smitty.css`;
      removed the now-unused `.child-tab-*` block (its only
      consumer was CustomerDetail).
- [x] JobDetail migrated to the new class names.
- [x] CustomerDetail now uses `WStackedWidget` with
      `.smitty-tab-btn` / `.smitty-tab-btn-active`, and a
      `.smitty-tab-tagline` under each tab. Per-tab titlebar
      replaced by the "+" affordance on a
      `.line-item-add-row` (mirrors JobDetail's Labor / Parts
      panels).
- [ ] Smoke test: Customer detail renders, tab switching still
      works, active pill highlights cleanly.

### Add tabs to Vehicle Detail (Service History)

`src/VehicleDetail.cpp` was a flat form (EntityDetailView default).
It now renders a pill-tab bar below the field form, sharing the
`.smitty-tab-*` CSS with Job and Customer detail pages.

- [x] Tab bar with **Service History** — list of Jobs on this
      vehicle (`fetchAllByField("Job", "vehicle_id", vehicleId)`),
      newest first (sorted desc by `created_date`), each row
      showing Job ID, Created, Description, Status, Labor, Parts,
      and Total. Rows are clickable and navigate to the
      corresponding Job detail.
- [x] Empty state when no jobs: "No service history yet."
- [x] Tagline under the Service History tab matching the pattern
      used elsewhere.
- [ ] Future (parking): **Images** tab pulling
      `customer_id`-scoped images for vehicle-level photos
      (if/when we extend the images feature to vehicles).
- [ ] Future (parking): The vehicle's own field form (owner, VIN,
      year, make, model, plate, notes) currently sits above the
      tab bar rather than as its own "Vehicle" tab — matching
      the JobDetail / CustomerDetail pattern where the parent
      entity's fields stay above its child-entity tabs. Revisit
      if UX feedback says otherwise.

Shared CSS factoring complete — Job, Customer, and Vehicle all
use `.smitty-tab-bar` / `.smitty-tab-btn` /
`.smitty-tab-btn-active` / `.smitty-tab-stack` /
`.smitty-tab-panel` / `.smitty-tab-tagline`.

### Job Notes — dedicated `job_notes` table + Notes tab

Move from the single free-form `jobs.notes` column to a proper note
history per job. Adds a fourth tab on Job Detail ("Notes") with a list
of timestamped entries and an "Add Note" action. The legacy
`jobs.notes` column stays in the DB for backwards compatibility but is
no longer rendered / written by the UI.

- [x] `database/patches/006_job_notes_table.sql` — table + FK + indexes
- [x] `database/schema.sql` + `database/seed_data.sql` updates
- [x] `EntityRegistry.cpp` — `JobNote` entity registered
- [ ] Phase B: Notes tab on `JobDetail` (list of notes with timestamp,
      Add Note dialog, hide `notes` field from the Job fields form)
- [ ] User applies patch 006 + rebuilds ALS so `/api/JobNote` works

### Job Images — tabbed UI + `images` table + mobile upload

Store images captured in the field (mobile app or desktop upload) and
display them on the Job Detail page as a third tab alongside Labor and
Parts. Images can belong to a Job, a Customer, or both.

**Scope of "mobile app":** this repo's Wt web UI served from a mobile
browser. The `<input type="file" accept="image/*" capture="environment">`
flow launches the device camera directly from the upload dialog. Any
native-mobile client consumes the same `/api/Image` JSONAPI endpoint.

**Staged so a timeout mid-phase does not lose work:**

#### Phase 1 — Data layer (DDL + registry)
- [ ] `database/patches/004_images_table.sql` — idempotent: renames
      legacy `job_images` → `images`, adds `customer_id`, `title`,
      `note`; drops the string `DEFAULT`s on `image_type` / `mime_type`
      (ALS reflection bug per `LESSONS.md`); makes `job_id` and
      `image_type` nullable; adds FKs + indexes.
- [ ] `database/schema.sql` — replace the legacy `job_images`
      `CREATE TABLE` with the new `images` shape so fresh installs and
      VCP deployments start clean.
- [ ] `database/seed_data.sql` — add FKs (`fk_images_jobs`,
      `fk_images_customers`) and indexes matching the patch.
- [ ] `EntityRegistry.cpp` — register `Image` entity with fields
      `job_id, customer_id, title, note, mime_type, captured_at,
      captured_by, image_id`. Omit `image_data` from the list columns
      (large CLOB — never renders in a table; fetched only for the
      detail modal).
- [ ] User applies patch + rebuilds ALS + verifies `/api/Image` with
      `curl`.

#### Phase 2 — Jobs page tab restructure
- [ ] Refactor `JobDetail.cpp`: wrap the existing Labor + Parts areas
      under two tabs inside a `WStackedWidget` switched by a tab bar;
      add an empty third tab "Images" (content in Phase 3).
- [ ] Tab bar styling in `smitty.css`: active tab underline, hover
      state, theme-aware.
- [ ] Preserve totals card (Labor+Parts flow into it regardless of
      active tab).
- [ ] Regression: labor / parts dialogs, PDF print, generate-invoice
      still work.

#### Phase 3 — Images grid UI — LANDED
- [x] Images tab renders a responsive CSS grid (`auto-fill` at
      min 180px) of `.job-image-card` cards, each with rounded
      corners, hover shadow, a 4:3 thumbnail (data URL from
      `image_data`), title, and a truncated note line.
- [x] Click card opens the full-size preview modal (Phase 5).
- [x] Inline in `JobDetail::loadJobImages()` — no separate
      `ImageCard` helper needed; the card body is small.
- [x] Empty state: "No images yet." text hidden when the
      grid is non-empty.

#### Phase 4 — Upload flow — LANDED (desktop path, multi-file batch)
- [x] "Upload Image" button at the top of the Images tab.
      Shared button, no user-agent sniffing.
- [x] `WDialog` with `WFileUpload` (`setMultiple(true)`), shared
      title (single line) and note (multi-line) inputs applied
      to every file in the batch.
- [x] Save closes the dialog immediately, then each file is
      POSTed sequentially on a 0 ms single-shot `WTimer` so the
      UI stays responsive and placeholder cards drain visibly
      one at a time. Per-file success / failure toasts.
- [x] `WFileUpload` capped via `setFileTextSize(10240)`
      display hint, plus a real 10 MB post-upload size check
      against the spool file before the POST. Rejects with
      "Image too large (max 10 MB)." Server-side
      `fileTooLarge` signal surfaces the request-size cap too.
- [x] Wt httpd launch flag `--max-request-size=20480` (20 MB)
      added to `scripts/start.sh` and documented in `CLAUDE.md §2`.
- [x] On save: read file into memory, base64 via
      `Auth::base64Encode`, POST to `/api/Image` with
      `job_id` / `customer_id` (resolved from the job) /
      `title` / `note` / `mime_type` (from file extension) /
      `image_data` / `image_type="job"` / `captured_by`.
- [x] DataBus Image event triggers an immediate grid refresh
      so the new card appears without a manual reload.
- [-] Mobile-specific "Take Photo" capture-only button —
      parked. The current universal button works from mobile
      browsers; a camera-first shortcut stays on the roadmap
      for when a native mobile client ships.

#### Phase 5 — Image detail modal — LANDED
- [x] Click a card opens a wide modal (760px / 92vw) with
      full-size img, the note rendered below in a soft card,
      and a meta line ("Captured: <timestamp> by <user>").
- [x] Delete button visible only when the current role has
      `canDelete("jobs")` (i.e. Admin by default). Office
      Manager does NOT currently have `canDelete` on jobs; if
      the user wants OfficeManager to clean up photos, adjust
      the role privileges in Settings → Privileges.
- [-] Title in the dialog title bar rather than the body —
      already handled: the dialog title is set from the
      image's title (or "Image" when empty).
- [-] `captured_at` reformat per `AppSettings.dateFormat` —
      parked. Today the timestamp is shown as ALS returns it
      (ISO-ish). Low priority UX polish.

#### Phase 6 — Customer-level images
- [ ] Mirror the Images tab on `CustomerDetail.cpp` so customer-only
      images (not tied to a job) have a place.
- [ ] `onEntityChanged("Image")` DataBus subscription refreshes both
      JobDetail and CustomerDetail Images tabs automatically.
- [ ] Upload dialog opened from CustomerDetail sets `customer_id`
      and leaves `job_id` null; from JobDetail sets both
      `customer_id` (resolved from the job) and `job_id`.

#### Phase 7 — Ergonomics on both form factors
- [ ] Smoke-test the upload flow on:
      * a real phone (mechanic in the bay, both camera and
        gallery paths),
      * a desktop browser (office attaching an image that came in
        by text / email — the common "from the road" scenario).
- [ ] Optional "Take Photo" quick-action button (mobile only,
      `capture="environment"`) alongside the main Upload button,
      so mechanics have a one-tap camera path.
- [ ] Responsive breakpoint: on narrow screens collapse the card grid
      to 2 columns.
- [ ] Note in `USER_TESTING_SCENARIO.md` with TC-IMG-* test cases.

#### Phase 8 — Server-side image resize (deferred)
Parked. If DB bloat becomes an issue or users complain about upload
speed, add a resize step before the POST:
- Downscale any dimension > 2048 px to fit 2048 px longest side
- Re-encode JPEG at quality ~85
- Strip EXIF (privacy + size)
Can be done client-side in JS (using `<canvas>`) or server-side in
an ALS pre-insert hook. Leaning JS — keeps the Wt server off the
hook for image manipulation.

#### Phase 9 — Drag-and-drop upload onto the Images tab (deferred)
Parked as option C of the original multi-file UX discussion
(options A and B covered in the shipped batch-upload flow). Power-
user affordance: drop files from Finder / email client directly
onto the Images tab without clicking "Upload Image" first.

- [ ] Drop zone wrapping the `imagesGrid_` container, with hover
      styling showing "Drop images here to upload" when a drag
      enters the tab.
- [ ] Use Wt's `Wt::WFileDropWidget` (ships with Wt) — handles
      the HTML5 drag-and-drop API, upload buffering, and per-file
      progress signals. Cleaner than hand-rolling
      `dragover` / `drop` JS handlers.
- [ ] Drop fires the same queued-upload pipeline as the dialog
      today (`PendingUpload` struct, `processNextUpload()`
      timer). No duplicated POST code.
- [ ] Optional: drag-and-drop also lands on the Customer detail
      Images tab once Phase 6 ships.
- [ ] Visual: dashed border appears on drag-enter, solid+highlighted
      on drag-over, reverts on leave / drop.

**Out of scope even at Phase 9**
- Reordering cards by drag-within-grid. Low value, added
  complexity in the layout. Revisit only if users ask.

### Cross-cutting cleanup
- [ ] After Phase 1 lands, append an Image-specific diagnosis entry
      to `LESSONS.md` (e.g. CLOB round-tripping through JSONAPI).
- [ ] Bump DEVELOPMENT_LOG with a "Job Images" feature entry once
      Phase 5 ships.

### Decisions locked
- **Image size cap: 10 MB pre-base64.** iPhone HDR JPEGs can reach
  8 MB; 10 MB gives headroom without opening the door to ProRAW
  (25-75 MB). ≈13.3 MB base64 per record in DB.
- **Wt httpd flag needed:** `--max-request-size=20480` (20 MB) so
  the multipart upload + form overhead fits.
- **Customer-level images: in scope (Phase 6).**
- **Native mobile app: out of scope.** Same `/api/Image` contract
  for any future native client.

### Still open
1. **Thumbnail generation** — thumbs are currently the same CLOB as
   full size (browser scales via CSS). 100+ images per job will be
   slow. Candidate for Phase 8 (server-side resize) or a separate
   `image_thumb` column.

### Catalog provider abstraction — single internal API for external parts sources

Plan for letting Smitty Services pull VIN decode + parts lookup +
cross-reference data from many external sources (Freightliner /
PartsPro, Volvo Impact, MAN MANTIS, PACCAR, NHTSA vPIC,
HeavyDutyXRef, DataOne, FleetPride …) **without** the UI ever
naming any one of them. Context: `docs/PARTS_CATALOG_RESEARCH.md`
plus the user clarification on 2026-05-16 — *"the API should be
modular, consolidate the API calls so that international and other
makes and sources such as OEM all use a single API. All interfaces
use a single API."*

**Design shape (mirrors `include/PaymentGateway.h`):**

```
include/CatalogProvider.h
  enum class CatalogCapability { DecodeVin, OpenInCatalog, CrossRef, Search };
  struct VinQuery / VinResult
  struct CrossRefQuery / CrossRefMatch
  struct CatalogLinkQuery / CatalogLink
  struct SearchQuery / SearchHit
  class  CatalogProvider {                 // pure virtual
      virtual std::string name() const = 0;       // "nhtsa-vpic"
      virtual std::string displayName() const = 0; // "NHTSA vPIC"
      virtual CatalogCapabilitySet supports() const = 0;
      virtual VinResult decodeVin(const VinQuery&) { return {}; }
      virtual CatalogLink openInCatalog(const CatalogLinkQuery&) { return {}; }
      virtual std::vector<CrossRefMatch> crossRef(const CrossRefQuery&) { return {}; }
      virtual std::vector<SearchHit>     search(const SearchQuery&)     { return {}; }
  };
  class CatalogProviderRegistry { /* same shape as PaymentGatewayRegistry */ };
```

The UI **never** instantiates a concrete provider. It calls
`CatalogProviderRegistry::instance().providersWith(capability)` and
fans the request out. The registry is the single API surface.

Concrete adapters are independent translation units. Adding MAN
or FleetPride later is a new `.cpp` file plus one registration line
in `main.cpp` — zero UI changes.

**User interaction model (per the 2026-05-16 clarification):**

- **Parts database** stays exactly where it is — the existing
  `products` / `supplier_product` tables driven through ALS. The
  catalog providers do **not** write to those tables; they're a
  read-only data feed that the user pulls *into* the parts DB via
  explicit "Use this part" actions.
- **Job screen is the dispatch point.** When adding/editing a
  JobPart, the existing sourcing panel (see `include/Sourcing.h`,
  `src/SourcingDialog.cpp`) gains a second tab — "External
  catalogs" — that queries every registered `CatalogProvider`
  whose `supports()` matches the query type. Results are merged
  and ranked; clicking a row either (a) opens an OEM link-out, or
  (b) prefills a new JobPart row.
- **Vehicle screen** also calls into the registry: the "Decode
  VIN" button runs every `DecodeVin`-capable provider and merges
  the first non-empty value per field. Same single-API pattern.

**Why this is *one* API even though every external source is
different:** every concrete provider lives behind the same four
virtuals. A link-out-only OEM (Freightliner) returns
`CatalogLink{ url, label }` and reports `DecodeVin` /
`CrossRef` as unsupported. A free decoder (NHTSA vPIC) supports
only `DecodeVin`. A subscription cross-ref (DataOne, Car
Databases) supports `DecodeVin` and `CrossRef`. The Job screen
asks the registry "give me everything that does cross-ref for
this part number" and the registry just hands back a `vector` —
the screen doesn't know or care what's underneath.

**Schema impact (across all phases):** none for the abstraction
itself. Adapters that ingest data (e.g. caching a cross-ref hit
onto `products.cross_ref` so it sticks) follow CLAUDE.md §7.2
(patch + schema.sql + ALS rebuild). The current plan does not
cache — every external call is on-demand — so no schema change
until a perf or offline requirement says otherwise.

#### Phase 1 — Define the abstraction + registry  (no external calls) — LANDED

Established the contract. Zero adapters wired in. UI untouched.

- [x] `include/CatalogProvider.h` — `CatalogProvider` interface
      with name / displayName / supports + four virtuals
      (decodeVin, openInCatalog, crossRef, search), all with empty
      default impls so adapters only override what they advertise.
      `CatalogCapability` enum class (unsigned underlying, bitmask)
      with `|`, `&`, and `hasCapability()` helpers. Value types:
      `VinQuery/VinResult`, `CatalogLinkQuery/CatalogLink`,
      `CrossRefQuery/CrossRefMatch`, `SearchQuery/SearchHit` —
      each result carries `providerName` so merged UI rows can
      attribute their source. `CatalogProviderRegistry` singleton:
      `registerProvider`, `get`, `providersWith(capability)`,
      `availableNames()`. Shape mirrors `PaymentGatewayRegistry`
      including first-registration-wins.
- [x] `src/CatalogProvider.cpp` — registry impl. No concrete
      adapters yet.
- [x] `CMakeLists.txt` — added `src/CatalogProvider.cpp` to
      SOURCES.
- [x] `src/SmittyApplication.cpp` — registration block lives
      next to the existing `PaymentGatewayRegistry` block (the
      gateway register happens there, not in main.cpp as the
      plan originally said). Header included; instance call
      materializes the singleton; commented line shows the
      pattern Phase 2's NhtsaVinAdapter registration will use.
- [x] **Acceptance**: standalone smoke test (FakeDecoder +
      FakeLink) verifies empty-by-default, registration order,
      first-registration-wins, capability filtering, get(),
      default virtuals returning empty. New TU compiles with
      `-Wall -Wextra -Wpedantic` clean. Full Wt build is the
      operator's job (`cmake --build build` on the Mac).

#### Phase 2 — First adapter: NHTSA vPIC  (free, validates the abstraction) — IMPLEMENTED

Simplest adapter: one capability (`DecodeVin`), no auth, no key,
no settings. Picked first to prove the contract under a real
network call before any auth/dealer-code work layers on top.
Awaits real-VIN smoke test on the Mac to flip to LANDED.

- [x] `include/catalog/NhtsaVinAdapter.h` +
      `src/catalog/NhtsaVinAdapter.cpp` — implements
      `CatalogProvider`, advertises `DecodeVin` only. Hits
      `https://vpic.nhtsa.dot.gov/api/vehicles/DecodeVinValues/<vin>?format=json`
      with a 5 s timeout. Maps `Results[0]` into the full
      `VinResult` set. vPIC `ErrorCode != "0"` is surfaced as a
      partial-decode warning but fields still flow through —
      partial data is often still useful on HD trucks. Logs the
      whole decoded row for Phase 5's paid-decoder decision.
- [x] `include/catalog/HttpUtil.h` + `src/catalog/HttpUtil.cpp` —
      shared "GET URL -> body string" used by every catalog
      adapter. Kept separate from `src/ApiClient.cpp` since
      ApiClient is scoped to ALS / JSON:API; HttpUtil has no
      header opinions and follows redirects. 5 s default
      timeout, SSL verify on, SmittyServices UA string,
      consistent `[<providerName>]` log format.
- [x] `src/SmittyApplication.cpp` — registers
      `std::make_shared<catalog::NhtsaVinAdapter>()` next to the
      PaymentGateway block. (The plan originally said `main.cpp`
      — corrected here since the existing gateway registration
      lives in `SmittyApplication`.)
- [x] `include/CatalogProvider.h` /
      `src/CatalogProvider.cpp` — added
      `decodeVinViaRegistry(vin)` free function so both UI call
      sites share the fan-out/merge logic. First-non-empty-per-
      field wins; first contributing provider's name is recorded
      on the result; empty-providers case returns `NO_PROVIDER`.
- [x] `src/VehicleList.cpp` Create dialog — VIN row gains a
      "Decode" button (`.btn-pill-grey`). Disabled until ≥ 11
      chars (live-updated on key-up). Click runs the registry
      fan-out and fills `year/make/model` (and the Description
      field if blank). Status text confirms which provider
      decoded; toast on failure.
- [x] `src/VehicleDetail.cpp` — "Decode VIN" header action
      added next to "Edit", visible only when the vehicle has a
      VIN ≥ 11 chars. Click PATCHes only the currently-empty
      fields and reloads the record. Toasts on every outcome.
      Skipped injecting into the generic edit dialog — the
      one-click header action is the cleaner UX and avoids
      touching `EntityDetailView`.
- [x] `CMakeLists.txt` — both new TUs added to SOURCES.
- [x] **Standalone syntax check**: HttpUtil.cpp and
      NhtsaVinAdapter.cpp parse with stubbed Wt/curl headers
      under `-Wall -Wextra -Wpedantic`. Merge helper unit-
      smoke-tested with three fake providers (no-providers,
      single-provider, two-providers-with-overlap).
- [ ] **Operator acceptance** (Mac build):
      `cmake --build build -j` clean, then exercise the Decode
      button on three real VINs — light pickup (e.g.
      `1FTFW1ET5DFA12345` Ford F-150), medium-duty (e.g.
      `4UZACKDS5HCJL1234` Freightliner M2), and Class 8 (e.g.
      `1XKWDB0X12J123456` Kenworth). Confirm year/make/model
      fill on at least the first two; capture the Class 8 server
      log line for the Phase 5 decision gate.

#### Phase 3 — Link-out adapters: Freightliner, Volvo, MAN, PACCAR — IMPLEMENTED

Four `OpenInCatalog` adapters wired up. UI lives on
`VehicleDetail` only this phase; **JobPart row buttons moved to
Phase 4** so the sourcing surface is touched once when the
cross-ref tab also lands (JobDetail is 3.3k lines — twice was
wasteful). Single configurable `LinkOutAdapter` class with four
registered instances replaced the four-near-identical-files
plan — same registry shape, half the code.

- [x] `include/catalog/LinkOutAdapter.h` +
      `src/catalog/LinkOutAdapter.cpp` — configurable adapter
      driven by `LinkOutConfig{ name, displayName, buttonLabel,
      defaultUrl, makes }`. URL template supports `{vin}`,
      `{part}`, `{dealer}` tokens (URL-encoded substitution).
      Reads the runtime URL from `AppSettings`; falls back to
      compiled-in default when the admin hasn't overridden.
- [x] `include/CatalogProvider.h` — added `servesMake(make)`
      virtual to `CatalogProvider` with default `return true`,
      so existing adapters (NHTSA) don't need to change.
      `LinkOutAdapter` overrides it: case-insensitive match
      against the configured makes set, empty-set wildcard,
      empty-make permissive.
- [x] `AppSettings` — added generic `catalogLinkUrl(name)` /
      `setCatalogLinkUrl` and `catalogLinkDealerCode(name)` /
      `setCatalogLinkDealerCode`. Backed by a single
      `std::map<std::string,std::string> catalogLinks_`. `load()`
      scans for `catalog_link_*` keys (variable set per adapter);
      `save()` writes every entry. Generic-by-name so Phase 4's
      HeavyDutyXRef adapter can reuse it.
- [x] `src/SmittyApplication.cpp` — registers four
      `LinkOutAdapter` instances after the NHTSA decoder:
      - `freightliner` → `https://www.dtnaparts.com/excelerator/`,
        serves {Freightliner, Western Star, Sterling, Detroit,
        Detroit Diesel, FCCC}.
      - `volvo` → `https://www.partsasist.com/`, serves {Volvo,
        Mack} (PartsASIST covers both since Mack is Volvo Group).
      - `man` → `https://mantisweb.man.eu/`, serves {MAN}.
        Speculative URL — admin can edit in Settings.
      - `paccar` → `https://www.paccarparts.com/`, serves
        {Kenworth, Peterbilt, DAF, PACCAR}.
- [x] `src/Settings.cpp` — new **Catalogs** tab (5th tab in the
      vertical tab list, book icon). Renders one settings-section
      per registered `OpenInCatalog` provider — URL input
      (placeholder = compiled-in default), Dealer code input,
      served-makes badge. Adding a fifth adapter later
      auto-appears with no edit to Settings.cpp.
      `saveCatalogSettings()` writes every row through the
      generic accessors and toasts.
- [x] `src/VehicleDetail.cpp` — header actions now include one
      generated link-out button per `OpenInCatalog`-capable
      provider whose `servesMake(vehicle.make)` returns true.
      Falls back to all providers when make is unknown. Buttons
      use `Wt::WLink(Url, ...)` with `target=NewWindow`.
- [x] `CMakeLists.txt` — `src/catalog/LinkOutAdapter.cpp` added
      to SOURCES.
- [x] **Standalone test**: smoke test verifies capability
      filtering (LinkOuts don't show under DecodeVin),
      case-insensitive `servesMake`, empty-makes wildcard,
      unknown-make permissive, URL token substitution
      (with URL-encoding for special chars), Settings override
      vs compiled-in default. Phase 1 and Phase 2 smoke tests
      re-run clean after the `servesMake` addition.
- [ ] **Operator acceptance** (Mac build): build clean, then
      with a vehicle whose make is "Freightliner" only the
      PartsProX button shows; change make to "Volvo" and the
      button row swaps to PartsASIST. Settings → Catalogs lets
      admin set a real PartsProX URL with `{vin}` and confirm it
      opens correctly with the VIN substituted. Mobile parity
      still pending Phase 4 decision (likely an ALS proxy
      resource).

**Deferred to Phase 4 (with the SourcingDialog cross-ref tab):**
- JobPart row link-out buttons — touching `JobDetail.cpp`
  (3310 lines) for a single button row is wasted scope when
  Phase 4 needs to add a "External catalogs" sourcing tab in
  the same file. Both land together.

#### Phase 4 — Cross-reference adapter + sourcing-panel UX — IMPLEMENTED

This is where the "single API" really pays off — the cross-ref
adapter is wholly unrelated to the link-outs and the decoders,
but plugs into the same registry, and the Job-screen sourcing
panel grows one tab that consumes the registry's cross-ref
capability. Took the ToS-safe "search-link" path rather than
scraping; a real result-fetching CrossRef provider (paid
DataOne in Phase 5) plugs in via the same interface.

- [x] `include/catalog/CrossRefSearchAdapter.h` +
      `src/catalog/CrossRefSearchAdapter.cpp` — generic,
      configurable CrossRef adapter. Driven by
      `CrossRefSearchConfig{ name, displayName, brandLabel,
      searchUrlTemplate, makes }`. **No scraping** — each call
      returns a single `CrossRefMatch` whose `url` is a
      populated search endpoint, `description` tells the user
      what clicking will do, `confidence` is 0.0 so a real
      result-returning provider's rows sort above these
      whenever one exists. Same `{part}` / `{make}` token
      substitution with URL-encoding as `LinkOutAdapter`;
      Settings override beats compiled-in default;
      case-insensitive `servesMake` + empty-set wildcard.
- [x] `src/SmittyApplication.cpp` — registers two
      CrossRef-capable adapters:
      - `heavydutyxref` →
        `https://www.heavydutyxref.com/?search={part}`, serves all makes.
      - `finditparts` →
        `https://www.finditparts.com/products/search?q={part}`,
        serves all makes.
- [x] `include/SourcingDialog.h` — optional `vehicleMake`
      parameter (defaults to empty). Existing two call sites in
      `ProductDetail.cpp` and `JobDetail.cpp` keep working
      unchanged; the make filter is permissive when empty.
- [x] `src/SourcingDialog.cpp` — split into "Suppliers" and
      "External catalogs" tabs when a CrossRef provider is
      registered; falls back to the single-pane layout when
      none is registered (so a deployment that disables external
      catalogs sees no UI change). The External catalogs tab:
      part-number input + Search button + filtered-merged result
      table (Brand / Part # / Description / Source / Open
      anchor). Spinner-free because every Phase 4 CrossRef call
      is synchronous + fast (no fetch); Phase 5's DataOne
      adapter is where threading matters and that decision is
      flagged in its own phase.
- [x] `CMakeLists.txt` — `src/catalog/CrossRefSearchAdapter.cpp`
      added to SOURCES.
- [x] **Standalone test**: CrossRefSearchAdapter smoke test
      covers capability filtering (CrossRef under registry,
      OpenInCatalog adapter doesn't show), empty-query
      behaviour, populated-result shape, Settings URL override
      beating compiled-in default, `{part}` / `{make}`
      substitution with URL encoding.

**Phase 3 JobPart-row link-out buttons subsumed here:** the
existing **Source** button on each JobPart row now opens the
two-tab dialog. Clicking the External catalogs tab and then
**Open** on a result row delivers the same "jump to OEM
catalog with VIN/part prefilled" UX without adding per-row
button clutter to a 3.3 k-line JobDetail.

**Open question — mobile parity (CLAUDE.md §12):**
the mobile sourcing panel needs the same merged-results UX.
Likely answer is an ALS proxy resource (`POST /api/catalog/
cross-ref`) calling into the same `CatalogProviderRegistry`
on the server, so the mobile client doesn't re-implement
each adapter and any future API keys stay server-side.
Defer the decision until the mobile sourcing screen is
actually being touched.

- [ ] **Operator acceptance** (Mac build): build clean;
      from a Job row, click Source on a JobPart and confirm
      both tabs render — Suppliers tab unchanged, External
      catalogs tab shows two providers when a part number
      is typed (HeavyDutyXRef + FinditParts), each Open
      link launches the respective site search in a new tab.

##### Phase 4 follow-up — Vehicle value type

Per the 2026-05-16 clarification ("perhaps we need an object
named Vehicle to pass into the sourcing for parts; vehicle
should have vehicle make and the VIN as well as other info").
Replaces the flat `vehicleMake` string parameter so VIN and
the rest of the vehicle context flow through the same
plumbing.

- [x] `include/Vehicle.h` — header-only POD struct
      `{vehicleId, vin, make, model, year, description,
      customerId, licensePlate}` plus a `fromRecord(json)`
      factory that accepts either the full JSON:API record
      shape (`{id, attributes:{...}}`) or a bare attributes
      object. Empty-by-default; `empty()` predicate; numeric
      fields stored as string so callers don't worry about
      JSON type juggling.
- [x] `CrossRefQuery` gained a `vin` field;
      `CrossRefSearchAdapter` now also substitutes `{vin}`
      in its URL template. `CatalogLinkQuery` already had
      `vin` so no change there.
- [x] `SourcingDialog::open(..., const Vehicle& vehicle =
      Vehicle{})` replaces the flat `vehicleMake` parameter.
      Default keeps the two existing call sites
      (`ProductDetail`, `JobDetail`) compiling without edits;
      `ProductDetail` keeps passing nothing, `JobDetail` now
      passes the parent Job's Vehicle.
- [x] External catalogs tab now badges both Make and VIN
      when present, and propagates both into every
      `CrossRefQuery`.
- [x] `JobDetail` populates a parallel
      `std::map<std::string, Vehicle> vehiclesById_` in the
      existing Vehicle-loading loop in `loadLookups()`. New
      private helper `currentVehicle()` resolves the current
      Job's parent Vehicle from the cache. Adds the Vehicle
      lookup at the SourcingDialog::open call site.
- [x] **Standalone test**: `Vehicle::fromRecord` smoke test
      covers empty input, full JSON:API record shape, bare
      attributes shape, null-tolerant field reads. All four
      prior catalog smoke tests re-run clean after the
      `CrossRefQuery.vin` addition.

#### Phase 5 — Optional paid adapter: DataOne (or Car Databases)

Triggered only if the Phase 2 logs show NHTSA vPIC missing
fields the shop actually needs on HD trucks, *or* if Phase 4
shows HeavyDutyXRef cross-ref hit-rate is too thin.

- [ ] **Decision gate**: 20+ real VINs through Phase 2, 20+ real
      OEM numbers through Phase 4. Tally what's missing. If
      coverage is acceptable, **skip this phase entirely**.
- [ ] If proceeding: `src/catalog/DataOneAdapter.cpp` — supports
      `DecodeVin` and `CrossRef`. Reads `dataone_api_key` from
      `AppSettings`. Hidden in Settings unless `enabled = true`.
      Key stays server-side; never ship it in the C++ binary or
      surface it through ALS to the mobile client.
- [ ] Adapter ordering: registered *after* free providers so the
      free ones run first; DataOne only fills gaps. The registry
      iteration order matches registration order (same as
      PaymentGatewayRegistry), so this is just a `main.cpp`
      sequence choice — no extra plumbing.

#### Phase 6 — Deferred: dealer-credentialed integration

Captured so future sessions don't re-litigate.

- [ ] Whenever Imagery Motor Services has a signed DTNA / Volvo
      / PACCAR dealer agreement, investigate what feed each OEM
      offers (most likely AS2/EDI for *ordering*, not catalog).
- [ ] If a feed exists, it almost certainly lives behind an
      SFTP/AS2 gateway, **not** as a `CatalogProvider`. Design
      a separate background ingest service writing to dedicated
      tables; surface results through normal ALS resources.
      `CatalogProvider` stays a UI-facing read API.

#### Risks and open questions

1. **External catalog ToS.** HeavyDutyXRef, FinditParts, etc.
   are unlikely to permit automated scraping in their ToS. Each
   adapter must be reviewed individually before it ships.
   Default safe stance: link-out only.
2. **URL drift.** Every OEM may change query-param shapes
   without notice. Mitigation: every link template is a
   Settings field, patchable without a redeploy.
3. **vPIC rate limit.** No hard quota, but "use reasonable
   judgment". Phase 2 calls vPIC once per Save-Vehicle. Don't
   add a decode-on-type preview.
4. **HD-truck VIN coverage in vPIC.** Class 7-8 is partial.
   This is the trigger for Phase 5's decision gate.
5. **Mobile parity vs. duplication.** Phase 4 raises: do
   adapters live in C++ only (proxy through ALS for mobile),
   or do we re-implement each adapter in TypeScript? Lean
   toward the ALS proxy — keeps API keys server-side and
   avoids two implementations of the scrape logic. **Open;
   decide at Phase 4 kickoff.**
6. **Threading.** Adapter calls are synchronous libcurl
   today. With 5+ providers behind one Decode-VIN click that
   could be a 5 s × 5 = 25 s wait worst case. If real-world
   testing shows this hurts, parallelize in the registry (each
   provider on its own `std::thread`, join with a deadline).
   Don't optimize before measuring.

#### Acceptance — feature done when

- [ ] `CatalogProvider` / `CatalogProviderRegistry` exist and
      have zero call sites that bypass them. Grep for direct
      curl/HTTP calls to OEM hosts returns zero hits outside
      `src/catalog/`.
- [ ] At least three adapters registered: NHTSA vPIC, one OEM
      link-out, one cross-ref source. All three reachable from
      the Job sourcing panel.
- [ ] Settings page renders the provider list dynamically from
      the registry.
- [ ] `docs/DEVELOPMENT_LOG.md` Feature Log entry appended
      (CLAUDE.md §9.7).
- [ ] `docs/PARTS_CATALOG_RESEARCH.md` updated with a "Status"
      header at the top pointing at the implemented adapters.

---

## Recently completed (branch `claude/plan-pos-payments-889ew`)

### POS plan + repo scaffolding
- [x] `docs/POS_PAYMENTS_PLAN.md` — phased plan (cash → Stripe → PayPal),
      `PaymentGateway` abstraction, schema design, security notes
- [x] `CLAUDE.md` — session-start brief: architecture, pitfalls,
      workflow rules
- [x] `tasks/TODO.md` + `tasks/LESSONS.md` scaffolding

### Data-refresh overhaul (DataBus)
- [x] `include/DataBus.h`, `src/DataBus.cpp` — per-session event bus
- [x] `SmittyApplication` owns the DataBus; `DataBus::current()` accessor
- [x] `ApiClient::{createRecord, updateRecord, deleteRecord}` emit
      `entityChanged(resource)` on HTTP success
- [x] `EntityListView::buildUI()` auto-subscribes; virtual
      `onEntityChanged(resource)` default refreshes on own resource
- [x] `JobList`, `VehicleList`, `OrderList`, `PaymentList` reload
      cross-entity lookups on related-entity changes
- [x] `Dashboard` + `MechanicDashboard` refresh on Job/JobPart/JobLabor/
      Customer/Vehicle changes
- [ ] **Smoke test in browser** — see Test plan below

### Payments table (POS Phase 0 unblock)
- [x] `database/patches/001_add_payments_table.sql` — idempotent DDL
      (table + FKs via DO block + indexes + rollback)
- [x] `database/schema.sql` — CREATE TABLE added to Service Center section
- [x] `database/seed_data.sql` — FKs and indexes appended
- [x] `ApiClient::parseResponse` hardened to detect HTML bodies and
      return a diagnosis-friendly error instead of
      `invalid literal; last read: '<'`
- [x] Patch applied on user's DB; ALS rebuilt via
      `ApiLogicServer rebuild-from-database`; `/api/Payment` live

### Build hygiene
- [x] `-Wno-deprecated-declarations` at target scope in CMakeLists.txt
      to silence Apple Clang warnings from Wt headers

---

## Smoke-test checklist (user, next time the app is running)

- [ ] **Bug 1 — vehicle picker refresh:** from Vehicles page, add a
      vehicle; navigate to Jobs; click "New Job"; select the customer
      who owns the new vehicle; confirm the vehicle appears in the
      dropdown.
- [ ] **Bug 2 — dashboard refresh:** from Jobs page, create a job;
      navigate to Dashboard; confirm the new job appears without
      hitting the Refresh button.
- [ ] **Payment list loads** without any error banner.
- [ ] **Add Payment dialog** saves and the list refreshes.
- [ ] **Delete a row** (any list that supports it) and confirm the row
      disappears and the list footer updates.
- [ ] **Sanity regressions:** open Customer / Order / Product / Job /
      Purchase list pages and confirm each still renders.

---

## Next up (pick one)

### POS Phase 1 — Cash payments — LANDED
See `docs/POS_PAYMENTS_PLAN.md` §9 and `docs/DEVELOPMENT_LOG.md`
"POS Phase 1 (Cash payments)" for the narrative.
- [x] `EntityRegistry.cpp` Payment entity aligned with patch 001
      (order_id, job_id, status, cashier, gl_posted added).
- [x] `PaymentGateway` interface + registry + `CashGateway`
      (`include/PaymentGateway.h`, `src/PaymentGateway.cpp`,
      `src/CashGateway.cpp`). Registered in `SmittyApplication`.
- [x] `ApiClient::createRecord` / `updateRecord` now take an
      optional `relationships` JSON:API block. CashGateway fills
      it for customer / order / job FKs so ALS's rule engine
      accepts the POST (see LESSONS.md: "Missing Parent").
- [x] `PaymentDialog` — reused by POS page + Customer Detail Pay
      button. Invoice summary, Amount with Pay-in-Full, Method
      dropdown from registry, cash-specific Tendered/Change,
      Notes, Cancel / Charge.
- [x] `PosPage` with client-side balance math, search, DataBus
      subscription. Sidebar entry + `navigateTo("pos")` registered.
- [x] `database/patches/010_pos_menu_item.sql` — idempotent seed
      of menu_items id=11 + role_menu_privileges for Administrator
      + Office Manager. `schema.sql` mirrored.
      `Auth::defaultPrivilegesForRole` added `pos` to both roles.
- [x] Customer Detail Orders tab — Status column replaces the
      `shipped_date` paid checkmark with a real balance-based
      "Pay $<amount>" button.
- [x] `PaymentReceipt` PDF (`include/PaymentReceipt.h`,
      `src/PaymentReceipt.cpp`) — one-page receipt auto-opened on
      successful charge from both entry points.
- [x] Test scenarios drafted in `tests/USER_TESTING_SCENARIO.md`:
      TC-POS-001 (patch 010 + sidebar visibility), TC-POS-002
      (unpaid list computation), TC-POS-003 (POS charge +
      receipt), TC-POS-004 (Customer Detail Pay button),
      TC-POS-005 (search).
- [ ] User to apply patch 010, build, and run TC-POS-001..005.
- [-] Backend (ALS): `/api/invoices/unpaid`, `/api/orders/{id}/balance`,
      `/api/pos/cash`, `v_invoice_balances` view — **deferred to
      Phase 2**. Phase 1 hits ALS's vanilla JSON:API; client-side
      balance math is plenty fast at current data volumes, and the
      server-side orchestrator only starts to earn its keep when
      gateway secrets and webhook verification matter (Stripe).

### Toaster / slide-over dialogs (UX direction)

User is reconsidering the strict modal dialog pattern. Two related
but distinct patterns to spec before code lands.

**Pattern A — toasts for transient status** (notifications).
Small rectangular popup, lower-right corner, slides in and auto-
dismisses after ~4 s. Non-blocking. Stackable. Good fit for:
- "Payment $X recorded for Invoice #Y" (today shown in the
  PosPage status line).
- "Customer saved" / "Order deleted" success flashes.
- Non-fatal API error summaries (today shown as inline status).

**Pattern B — slide-over panels for form flows** (drawers).
Full-height panel that slides in from the right over the current
page, overlay dims the background but doesn't block the sidebar.
Good fit for:
- Add User, Add Customer, Add Order (today `WDialog`).
- PaymentDialog (Take Payment).
- Image upload dialog.

The receipt preview and confirmation dialogs (Delete User, Delete
Row) probably stay as centered `WDialog` — they're truly modal
decisions.

#### Open questions — ANSWERED 2026-04-22

1. **Scope.** Toasts only for now. Form dialogs stay as
   `WDialog`. Phase B (slide-over drawers) deferred.
2. **Animation.** Pure CSS transitions, no new JS dep.
3. **Stacking.** Max 3 visible. Overflow drops oldest.
4. **Dismissal.** Both — auto-dismiss after 4 s AND a × close
   button.
5. **Slide direction.** Right edge.
6. **Width.** Mobile-responsive breakpoint behavior — toast
   `min-width: 280px / max-width: 380px` on desktop,
   `max-width: none` under 600 px viewport so the host
   stretches edge-to-edge on phones / kiosks.

#### Phase A — Toast notifications (non-blocking, status-only)

- [x] `include/Toast.h` + `src/Toast.cpp` — `Toast::show(message,
      kind)` free function. Lazy-creates the host on the
      application root on first call (no
      `SmittyApplication`-side init). Kinds: Info, Success,
      Warning, Error with coloured left border per kind.
      Auto-dismiss 4 s via `WTimer`; manual × close.
- [x] CSS in `resources/css/smitty.css`: `.toast-host` (fixed
      bottom-right, `pointer-events: none` so it doesn't block
      the page), `.toast` with `@keyframes toast-slide-in`,
      `.toast-info|success|warning|error` left-border variants,
      `@media (max-width: 600px)` for full-width on phones.
- [ ] Wire into the existing success-status call sites:
      `PosPage` ("Payment recorded"), `CustomerDetail` Save /
      Add X success, `Settings` saveGeneralSettings (today
      silent on success — toast it).
- [ ] Replace the status line below the POS invoice table
      with toasts; keep the line for persistent info like
      "2 unpaid invoices — total owed $X".

#### Phase B — Slide-over panels (replace form dialogs)

- [ ] `include/SlidePanel.h` + `src/SlidePanel.cpp` — a
      `WContainerWidget` that behaves like `WDialog` (accept /
      reject signals, a contents area) but renders as a
      right-edge drawer. Backdrop click dismisses; Esc
      dismisses. Width configurable, default 480px.
- [ ] Port `PaymentDialog` first — it has the cleanest boundary
      (one caller, clean paid() signal). Validate the pattern
      end-to-end before doing the other form dialogs.
- [ ] Port `Settings` add/reset/delete user dialogs.
- [ ] Port `CustomerDetail` add order / add vehicle dialogs.
- [ ] Port `JobDetail` add labor / add part dialogs.
- [ ] Port the image upload dialog once it lands.

#### What stays as `WDialog`

- Confirmation dialogs (Delete X, Discard changes) — a modal
  is the right pattern for "are you sure".
- `PaymentReceiptDialog` — the PDF preview reads better
  centered than slid in from the edge.
- Any future dialog that really is "block until you respond".

#### Migration rules

- New dialog code: pick one of Toast / SlidePanel / WDialog
  per the guidance above.
- Existing `WDialog` usage: leave alone until its feature
  is touched for another reason. Don't bulk-convert.
- CLAUDE.md §7.1 gets a third column once SlidePanel lands —
  "dialog type decision tree".

---

### Job parts vs. Products — formalize the relationship

`job_parts` rows currently carry a free-text `description`, an
optional `product_id` FK, and pricing fields (`unit_price`,
`quantity`, `discount`/`markup_pct`, `amount`). In practice mechanics
sometimes pick a known Product, sometimes type a one-off, and the
two paths drift over time:

- A row with `product_id` and a hand-typed description that no
  longer matches the linked product's name.
- Two rows for the same physical part, one linked, one ad-hoc.
- Pricing entered manually instead of pulled from
  `products.unit_price`, so margin tracking is unreliable.
- Inventory (`products.units_in_stock`) doesn't always decrement
  when a part is consumed on a job.

**Decisions to make before coding**

1. **Are job parts always derivable from Products, or is "ad-hoc
   part" a first-class case?** If first-class, we accept the
   floating description forever and just need clearer UX. If not,
   typing a description should always end with picking-or-creating
   a Product.
2. **When a row links a Product, do we copy price at link time
   (snapshot) or always read live?** Snapshot survives Product
   price changes; live tracks them. Most POS systems snapshot.
3. **Inventory: should adding a JobPart for a known Product
   decrement `units_in_stock`, and removing it (or marking
   not-installed) re-increment?** Mechanic Dashboard already has
   a 6-state parts workflow planned; this is where it earns its
   keep.
4. **Do we want a "create new Product from this JobPart" button**
   for the case where a mechanic types something that doesn't
   exist yet?

**Open follow-up entries from these decisions**

- New schema patch for any non-null FK / column changes on
  `job_parts` (e.g. `product_snapshot_price NUMERIC`).
- JobPart add dialog redesign (Products autocomplete — see the
  next TODO entry).
- Update `EntityRegistry.cpp` JobPart definition to reflect
  whatever columns we settle on.
- DEVELOPMENT_LOG entry once the chosen flow lands.

Out of scope for this task: rewriting the Mechanic Dashboard
parts state machine. That's tracked separately.

### Products autocomplete on JobParts (and elsewhere)

User-driven idea: typing a part description on a Job should pop a
short list of matching Products underneath the input as the user
types, with arrow-key + click-to-pick. If nothing matches, a
"+ Add as new part" option appears at the bottom that opens the
existing Product create dialog pre-filled with the typed text.

Wt has `Wt::WSuggestionPopup` built in for exactly this — no new
dependency, server-side filtering on each keystroke.

**Phase 1 — JobParts (the requested case)**
- [ ] New helper `include/ProductSuggestionPopup.h` /
      `src/ProductSuggestionPopup.cpp` — wraps `WSuggestionPopup`
      with a Smitty-flavoured matcher loop over the Products
      lookup map (already cached in JobDetail / JobList).
      Surfaces `productPicked(productId)` and `addNewRequested(text)`
      signals.
- [ ] JobDetail Add Part dialog: replace the current
      Product `WComboBox` with a `WLineEdit` + the new popup.
      Defaults to the typed text if the user types past the
      matches.
- [ ] If "+ Add as new part" is chosen, open the existing
      Product create dialog pre-filled with the typed text;
      on save, link the resulting `product_id` into the JobPart
      row before submit.
- [ ] DataBus: subscribe to `Product` events so the suggestion
      list refreshes when a new Product lands (covers the
      add-then-link path above).
- [ ] CSS: small dropdown shadow + max-height with scroll;
      highlighted hovered row, keyboard-arrow navigable (Wt
      handles arrows natively).

**Phase 2 (parking lot, after Phase 1 ships)**
- [ ] Same widget on the Order add-line dialog (pick a Product
      for an invoice line item).
- [ ] Same on the Purchase add-item dialog (which Product is
      this PO line for).
- [ ] Customer picker on Job-create / Order-create (long
      customer lists outgrow the current `WComboBox`).

**Out of scope for this task**
- Server-side fuzzy matching. Client-side substring on the
  cached lookup map is fast enough for a few thousand
  Products. If catalog grows past that we add an
  `/api/products?q=` server filter (parking-lot item).
- Recently-used picks list. Nice-to-have but not required.

### Paid invoice should show its Receipt and an "Invoice Paid!" banner

Today the receipt PDF is only viewable once, in the preview dialog
that opens at the moment of payment (PosPage / CustomerDetail Pay
button). After the user closes the dialog there is no way back to
that PDF, and no visual indication on Invoice Detail that the
invoice has actually been paid in full. This is the gap the user
asked about: "when we preview a paid invoice, it should show the
Receipt. Invoice Paid!"

**Behaviour we want**

- Open an Invoice (Order Detail). If the invoice's outstanding
  balance is `<= 0` based on captured Payments:
  - A prominent **PAID** banner (green pill or stamp) appears at
    the top of the invoice, with the date of the final payment
    that closed it.
  - A **Receipt** action button is visible in the header strip
    (next to Print / Generate Invoice).
  - Clicking Receipt opens the existing `PaymentReceiptDialog`
    populated from the invoice's payment(s) — same iframe preview
    + Done / Download / Print buttons that the post-charge flow
    already uses.
- For partially paid invoices: show a smaller `Partial — paid
  $X / total $Y` chip in the same spot, with the same Receipt
  action available (it can render a "consolidated" receipt
  listing every captured payment to date, or one receipt per
  payment, see open question below).

**Surfaces that should also expose a "View receipt" path**

- **PaymentList** rows (sidebar → Payments): add a "Receipt"
  action so any past payment can be re-opened from the central
  payments table. Most direct fix and unblocks bookkeeping.
- **CustomerDetail Orders tab**: today the Status column shows
  `✓ Paid` text or a `Pay $balance` button. Make the `✓ Paid`
  text clickable, opening the same receipt dialog.

**Implementation outline (in order)**

- [ ] New `PaymentReceipt::Data buildDataForExistingPayment(int paymentId)`
      static. Fetches `/api/Payment/{id}` for the seed values
      (paymentId, orderId, customerId, amount, method, cashier,
      dateTime, notes, reference) and then runs the same
      customer / OrderDetail / Payment-sum scans `buildData()`
      already does.
- [ ] PaymentList — per-row "Receipt" action button. Click ->
      `buildDataForExistingPayment(payment_id)` -> show
      `PaymentReceiptDialog`.
- [ ] CustomerDetail Orders tab — make `✓ Paid` clickable and
      route the click to the same flow (pull the most recent
      captured payment for the order, show its receipt).
- [ ] Order Detail (`OrderDetail.cpp`) — add the PAID banner
      when balance == 0, "Partial — paid $X / total $Y" chip
      otherwise. Add a `Receipt` button to the header-actions
      strip alongside the existing Print / Generate Invoice
      buttons.

**Open question to settle before code**

- For an invoice with multiple captured payments, do we render
  **one consolidated receipt** (lists all payments + grand total
  paid + balance) or **one receipt per payment** (with a
  per-payment picker if there are several)?
  Recommendation: **consolidated** for the Invoice page (what the
  user typically wants — proof the whole bill is settled) and
  **per-payment** from PaymentList (you came in via a specific
  payment row, you want that row's receipt). `PaymentReceipt::Data`
  already supports both shapes — its `lines[]` is the invoice
  detail and `amountPaid` is the running total, so a consolidated
  build is `buildDataForExistingPayment(<latest payment id>)` and
  the per-payment build is the same with the chosen id.

**Out of scope (park)**

- Editing or voiding past payments. Receipts are read-only.
  Refund / void is a Phase 4 plan item per
  `docs/POS_PAYMENTS_PLAN.md` §9.

### Accounting module — GL / AR / AP, license-gated

The full design lives in `docs/BUSINESS_ACCOUNTING.md` (5 phases:
Schema & GL Foundation, AP Module, Daily Close, AR & AP Views,
Financial Reports). This entry tracks the **delta** the user just
called out and the cross-cutting requirements that doc doesn't
yet capture.

**New cross-cutting requirements**

- **Licensed feature.** Accounting is a separately-purchased
  add-on, not part of the base service-center product. Smitty
  must:
  - Check a license entitlement at startup.
  - Hide the entire accounting menu group (sidebar entries, GL,
    AR, AP, Reports, Daily Close) when the entitlement is
    absent.
  - Block direct navigation to those routes for users without
    the entitlement, even if they bookmark the URL.
  - Continue posting `gl_posted` flags on operational records
    (jobs, payments, purchases) only when the entitlement is
    present, so unlicensed installs aren't silently accruing
    half-posted GL state.
- **Vendors are first-class** in the chart of accounts via the
  AP module, alongside customers via AR. The existing
  `suppliers` table is the vendor master; AP bills attach to
  supplier_id the same way AR balances attach to customer_id.
  `docs/BUSINESS_ACCOUNTING.md` §6 already designs this — just
  flagging that vendors aren't an afterthought.
- **No payroll.** The COA mentions "Labor Expense (future
  payroll link)" but payroll is **explicitly out of scope** for
  the licensed accounting module. Mechanic wages stay manual /
  external until a separate payroll add-on is scoped.

**Concrete trigger points the operational code must emit**

Once the accounting module is licensed and active, these four
existing operational events become GL-posting triggers
(detail in `BUSINESS_ACCOUNTING.md` §3):

| Operational event              | GL impact                                  |
|--------------------------------|--------------------------------------------|
| Payment recorded (POS)         | Dr Cash (or Stripe-clearing), Cr AR         |
| Invoice generated from Job     | Dr AR, Cr Service Revenue (Labor + Parts)  |
| Purchase Order received        | Dr Parts Inventory, Cr AP                  |
| Vendor bill paid (AP payment)  | Dr AP, Cr Cash                              |

The `gl_posted` flag on each operational table prevents
double-posting; per CLAUDE.md §6 it's owned by the Daily Close
flow, not by the operational code.

**License-gate scaffolding (new work, not in BUSINESS_ACCOUNTING.md)**

**Mechanism — LOCKED (2026-04-22).** Option A: license key in
`app_config`, validated against an Imagery Business Systems
license server on boot. The license server is a **purchasing
platform with two surfaces**:

- **Public web portal** for customers to buy, view, and renew
  licenses. End users get sent here to complete a renewal.
- **JSON:API** for client-installed software (Smitty) to check
  entitlements. Built on ALS like everything else Smitty talks
  to, so the C++ client reuses the existing JSON:API plumbing.

Expiration reminders live **in Smitty itself** (the app watches
the expiry it got from the last validation and surfaces a
reminder as the date approaches). The reminder's call-to-action
deep-links the user back to the public portal to complete the
renewal — renewal never happens inside Smitty.

- **API base URL default:** `https://licensing.imagery-business-systems.com/api`.
- **Override:** startup env var `LICENSE_SERVER_URL` (matching
  the pattern of `ALS_API_URL` for the operational ALS).
- **Portal (renewal) URL default:**
  `https://licensing.imagery-business-systems.com/renew`.
- **Override:** startup env var `LICENSE_PORTAL_URL` (separate
  from the API URL so staging / prod can split cleanly if
  needed).
- **Caching:** Smitty caches the last good entitlement response
  in `app_config` with a timestamp so short license-server
  outages don't lock users out. Re-validate on every boot and
  every 24h of continuous uptime.
- **Existing License Server repo.** Imagery has a separate repo
  with a running License Server and some features already built.
  **Before writing any `LicenseManager` client code in this
  repo, read its `README.md` and root-level files** so the
  Smitty client talks the real API shape rather than one
  invented here. User to share the repo URL when the time
  comes.

- [ ] New `LicenseManager` singleton (`include/LicenseManager.h`)
      with `bool hasFeature(const std::string& featureKey)`.
      `featureKey` examples: `"accounting"`, `"payroll"` (future),
      `"multi-location"` (future). Also exposes
      `expiresAt(featureKey)` and `portalUrl()` for the
      reminder / renewal UX below.
- [ ] `LicenseManager::validate()` called on startup from
      `SmittyApplication`, after `ApiClient::setBaseUrl` and
      `AppSettings::load`. POSTs the key to the license server,
      parses the JSON:API response for the feature set + expiry,
      caches into `app_config`.
- [ ] `SmittyApplication::createLayout` consults
      `LicenseManager::hasFeature("accounting")` before adding
      the GL / AR / AP / Reports / Daily Close pages to the
      `WStackedWidget` and the sidebar.
- [ ] `Auth::canView` consults the same — `accounting`-prefixed
      menu keys return false when entitlement is absent, so
      role-based privilege checks layer on top of license
      checks.
- [ ] Settings page gains a **License** subsection (admin-only)
      showing current entitlements + expiry, a paste-in for the
      license key, and a **"Renew / Manage on portal"** button
      that opens `LicenseManager::portalUrl()` in a new tab.
      Re-validates on Save.
- [ ] `BUSINESS_ACCOUNTING.md` updated with a new §0
      "License gating" cross-referencing the LicenseManager
      and listing exactly which menu keys / routes require
      `accounting`.

**Expiration reminders + renewal flow**

- [ ] At every login (and every 24h uptime re-validate),
      `LicenseManager` computes `daysUntilExpiry(featureKey)`
      for each active feature. When a feature is within the
      reminder window, Smitty surfaces a reminder in-app with
      a **"Renew on portal"** call-to-action that opens the
      portal URL in a new browser tab.
- [ ] **Reminder ladder (defaults, tune later):**
  - 30 days: subtle toast on login, Info kind.
  - 14 days: persistent banner across the top of every page,
    Warning kind, dismissable for the session.
  - 7 days: same persistent banner, Warning kind,
    **non-dismissable** — always visible until renewed.
  - Expired: feature disabled immediately on next boot; the
    License subsection in Settings shows a red "License
    expired — renew on portal" state; any attempt to open a
    gated page routes back to Dashboard with a toast.
- [ ] The reminder is **per-feature** — if a customer buys
      Accounting and Multi-location and one expires before
      the other, only the expiring one reminds.
- [ ] Renewal flow (end-user):
  1. Click "Renew on portal" → browser opens
     `${portalUrl}?key=<licenseKey>&feature=<featureKey>`.
  2. Customer completes purchase on the portal.
  3. Portal issues an updated entitlement on the license
     server.
  4. Customer clicks **"Refresh license"** button in Smitty's
     Settings → License (calls `LicenseManager::validate()`
     immediately rather than waiting for the 24h cycle).
  5. Banner clears; feature continues working.
- [ ] The refresh button is mechanically the same as the
      existing Settings Save on the License key field — it
      hits `/api/License/validate` and updates the cache.
      Surface it as a button anyway so users have a clear
      "I just paid, make it work now" affordance.

**Decided — 2026-04-22**

- **Deployment model: install everything, gate at the UI.**
  Every Smitty install ships with the full schema (operational
  tables + accounting tables + `gl_posted` columns on jobs /
  payments / purchases). Feature availability is enforced in
  the C++ client via `LicenseManager::hasFeature`. Tradeoff: a
  few unused tables on unlicensed installs. Benefit: no JIT
  DDL machinery, one supported schema shape, and a future
  license unlock can back-post the entire operational history
  into the GL rather than starting at the unlock date.
- **`gl_posted` on operational rows:** always write `0` on
  insert, regardless of license state. Daily Close (licensed,
  accounting module only) is the only writer that flips it to
  1. Matches the "install everything, gate UI" decision above.
- **Reminder ladder (30 / 14 / 7 / expired):** accepted as
  designed. Defaults to land as above; tune after real-world
  feedback.

**JIT DDL deployment — parked as a future option, not a
direction.** Keeping a short note here so the idea survives:
if install footprint becomes a problem for unlicensed
customers (it won't at the current scale, but might if more
licensed modules land and each adds its own tables), we can
revisit a pattern where the feature-specific tables live in
their own schema / migrations that get applied at license
activation. Not planned work.

**Parked — still open before code**

- [ ] **License server auth shape.** How does Smitty
      authenticate per-customer when calling the license
      server API (probably the license key itself is the
      auth token, but confirm against the existing License
      Server repo's docs before writing `LicenseManager`).
- [ ] **Sale → customer key delivery flow.** How does the
      key reach the end user after purchase (customer portal,
      email, install wizard)? Out of scope for Smitty-Services
      repo but affects how we test license-refresh.
- [ ] **Vendor refunds / credit memos** in the AR/AP modules.
      Refund is a Phase 4 POS plan item; AR credit memos are
      a separate decision.

**Out of scope (park separately)**

- Payroll (per user direction).
- Multi-currency GL (USD only for the foreseeable).
- Multi-entity / multi-location consolidation.
- Bank reconciliation against an external feed (manual
  reconciliation through the GL detail view is fine for v1).

### POS hardware integration — receipt printer + cash drawer
User ordered a thermal receipt printer and a cash drawer for
demo purposes. Both should fire automatically on a successful
cash charge; the page-size PDF receipt stays as-is for email /
archive use, and the printer gets a dedicated tape-format render.

**Hardware assumptions (lock these once the units arrive)**
- [ ] Confirm make/model of each unit. Most likely: an
      ESC/POS-compatible 80mm printer (Epson TM-T20, Star
      TSP100, or a generic) and a 12V/24V solenoid drawer
      wired to the printer's RJ11/RJ12 "drawer kick" port.
- [ ] Confirm connectivity: USB, Ethernet, Bluetooth, or
      serial. Network-attached is easiest for a server-side
      app — the C++ process opens a TCP socket on port 9100
      and streams ESC/POS bytes. USB requires libusb or a
      CUPS raw queue.

**`AppSettings` (new fields, General or new POS panel)**
- [ ] Receipt printer enabled (bool).
- [ ] Transport: "network" | "cups" | "usb".
- [ ] Network host + port (default 9100 if transport=network).
- [ ] CUPS queue name (if transport=cups).
- [ ] Cash drawer kick enabled (bool, default true when printer
      is enabled).
- [ ] Drawer kick pin (0 or 1 — ESC/POS supports two pins, most
      printers wire the drawer to pin 0).

**`ReceiptPrinter` module (new, `include/ReceiptPrinter.h` +
`src/ReceiptPrinter.cpp`)**
- [ ] `bool ReceiptPrinter::print(const PaymentReceipt::Data&)`
      — formats a tape-style receipt (narrow column, double-
      height totals, cut at end) and sends it to the configured
      transport. Returns false on I/O error; caller surfaces to
      the cashier via a status message.
- [ ] `bool ReceiptPrinter::kickDrawer()` — sends ESC p 0 25 250
      (the standard drawer-kick pulse). Safe to call when no
      drawer is attached — the printer just ignores the
      extra command.
- [ ] Thin ESC/POS command builder (init, bold on/off, align
      left/center/right, cut paper, feed N lines, drawer kick).
      Kept as a single .cpp so we don't spread printer bytes
      across the codebase.

**`TapeReceipt` format (decide with user before coding)**
- Narrow column (~42 chars at 80mm width).
- Header: business name (bold, double height), address, phone,
  blank line.
- Meta block: Date / time, Invoice #, Customer name, Cashier.
- Lines: description (truncate to ~20 chars), qty, line total
  right-aligned.
- Totals: subtotal, discount, total, amount tendered, change.
- Footer: "Thank you" line, blank space, cut.
- Open question: do we also print a merchant copy (duplicate
  receipt)? Common for cash-drawer setups. Default: one copy;
  make it configurable.

**Wire-up**
- [ ] `PosPage` and `CustomerDetail` Pay button: after
      `PaymentDialog::paid()` fires successfully, call
      `ReceiptPrinter::print(data)` + `kickDrawer()` (if cash).
      Errors log but don't roll back the payment — the payment
      is committed; reprint can be manual from the preview
      dialog.
- [ ] Add a **Print Tape Receipt** button to
      `PaymentReceiptDialog` as a third primary so the cashier
      can re-print from the preview without re-charging.
- [ ] On non-cash methods (Stripe / PayPal in later phases),
      print the tape receipt but do NOT kick the drawer.

**Testing**
- [ ] Smoke test with the printer on and the drawer connected:
      charge, receipt prints, drawer pops, close drawer.
- [ ] Smoke test with the printer off: charge still succeeds,
      a "receipt printer offline" status appears on the POS
      page, payment is still recorded.
- [ ] Smoke test a mis-configured IP: same as printer-off path
      (socket timeout surfaces cleanly).

**Security note** — the ESC/POS protocol is unauthenticated. On
a shared LAN this is fine (printer is effectively a dumb device);
if the printer is ever internet-routable, firewall port 9100
aggressively.

**Out of scope (park)**
- Barcode / QR printing on the receipt (trivial add once the
  module exists — ESC/POS GS command — but not needed for the
  demo).
- Receipt emailing (different flow; use the existing page-size
  PDF).
- Customer-facing pole display integration.

### Filter-dropdown repopulate (small UX gap)
- [x] `EntityListView::repopulateFilterCombo(combo, allLabel, map)`
      static helper — clears + refills a `WComboBox` from a lookup
      map, preserving current selection by label when possible.
- [x] `JobList` calls it for its customer filter on any
      Customer/Vehicle DataBus event (co-located with the existing
      `loadLookups()` call in `onEntityChanged`).
- [x] `OrderList` calls it for customer + employee combos on
      Customer/Employee events.
- [x] `VehicleList` calls it for the owner combo on Customer
      events.
- [-] `PurchaseList` / `ProductList` — skipped; Supplier and
      Category have no management UI in the app, so their lookup
      maps never go stale at runtime.

### Dedicated TypeScript mobile app (Vite)

Instead of retrofitting mobile ergonomics onto the Wt web UI, build a
**separate, mobile-first frontend** in TypeScript with Vite as the
build tool and a dedicated mobile stylesheet. Same ALS backend
(`/api/*` JSON:API) as the contract boundary — the Wt desktop app
and the TS mobile app are siblings that talk to the same server.

**Why a separate app (not responsive Wt)**
- Touch-optimised UI patterns (big tap targets, swipe, pull-to-
  refresh, native-feeling transitions) are painful to retrofit onto
  server-side widgets.
- Device integrations that matter most on mobile (camera capture,
  push notifications, offline cache, share-sheet receive) are
  cleanest in a PWA / native wrapper that owns its own service
  worker, not in a Wt session.
- Separating the codebases lets the desktop UI stay "thick-client
  office tool" and the mobile UI stay "kiosk / bay / on-the-road
  tool" without either compromising the other.

**Target audience & scope for v1**
- **Mechanic on the bay / on the road** — primary user.
- Flows: log in; see My Jobs; drill into a job; view / advance
  parts and labor status; add a note; snap/upload an image;
  receive pushes for notes and assignments.
- Explicitly **out of scope for v1:** POS, full invoicing, product
  catalog, admin screens. Those stay on the Wt desktop app.

**Stack (locked, matching Student-Onboarding-Mobile house style)**
- **React 18** + **TypeScript 5.5**
- **Vite 5** build tool
- **Ionic 8** UI component library — replaces the "custom CSS"
  idea; Ionic gives native-feeling mobile primitives
  (ion-tabs, ion-modal, ion-refresher, ion-item) and themeable
  CSS variables that match the project's CSS-var philosophy.
- **Capacitor 6** as day-1 wrapper. Runs as a web PWA, installable
  iOS app, and installable Android app from the same codebase
  (the reference calls this stack "VCP" — Vite + Capacitor +
  Preferences).
- **sql.js** (WASM SQLite) for offline storage — see Offline
  below.
- **Axios** for API calls with a Bearer-token interceptor.
- **Capacitor Preferences** plugin for session / settings storage.
- **Capacitor Network** plugin for online/offline detection.

**Offline mode (day-1, user-controllable)**
The reference ships a toggle under Settings → Data Storage that
switches between online (ALS) and offline (local SQLite). Switching
to offline downloads relevant data; switching back replays a FIFO
sync queue to the server. Smitty's mechanic on the bay has spotty
wifi — this is worth having day one.
- [ ] `src/offline/db.ts` — sql.js init + schema migrations,
      persisted as base64 in Capacitor Preferences.
- [ ] `src/offline/offlineStore.ts` — CRUD against the local DB.
- [ ] `src/offline/syncQueue.ts` — FIFO mutation queue with retry.
- [ ] `src/offline/SyncManager.ts` — Capacitor Network listener
      that auto-sync on reconnect.
- [ ] `src/hooks/useOffline.ts` — React hook exposing online /
      offline state.
- [ ] Yellow offline banner + blue syncing indicator in the tab
      shell.
- [ ] Which entities to mirror locally for v1: Jobs (mine or
      recent), JobPart, JobLaborItem, JobNote, Image (metadata
      only — skip the CLOB bytes for offline to avoid bloat).

**Auth (mirror the reference, with one change)**
The reference authenticates by querying `GET /AppUser/?filter[email]=`
directly — it **does not** use ALS's built-in auth because ALS ships
its own `UserRole` entity that clashes with the app's roles.
- [ ] Smitty already has `roles` + `app_users` + `role_menu_privileges`
      (patch 007). Before wiring mobile auth, decide whether to
      rename to `AppUser` / `AppUserRole` for parity with the
      reference — it prevents the same ALS built-in clash and
      makes the two mobile apps share a mental model. **Leaning
      yes.** Track as a separate DDL patch.
- [ ] `src/api/client.ts` — Axios instance with interceptor that
      injects `Bearer {session_token}` and handles 401 refresh.
- [ ] `src/api/auth.ts` — login flow: filter AppUser by email,
      check `is_active` + `login_enabled`, generate 64-char
      random session token client-side, fetch
      `GET /AppUser/{id}/AppUserRoleList`, stash in Preferences.
- [ ] **Password hashing:** the reference uses bcrypt on the
      client (`src/api/crypto.ts`). Smitty's desktop currently
      uses base64 "hashing" (`src/Auth.cpp` — known bad, in
      LESSONS.md). The persistent-auth blocker listed below
      includes swapping both frontends to bcrypt together so the
      stored hash format agrees.
- [ ] `src/auth/AuthContext.tsx`, `LoginPage.tsx`, `RegisterPage.tsx`,
      `ProtectedRoute.tsx` — same shapes as reference.

**Env + config**
- `VITE_ALS_API_URL` — base URL, default
  `http://localhost:5656/api`. Smitty's dev ALS runs on 5659, so
  override via `.env.local` or the Docker build-arg; reference
  convention is 5656 and VCP Stack Builder wires it.
- `VITE_DEV_MODE` — `""` (production), `"api"` (no-licensing),
  `"bypass"` (mock user, no backend). Three-tier dev mode as per
  reference.
- Content-Type `application/vnd.api+json` — same as the C++
  ApiClient.
- 15-second timeout on API calls.

**Project structure (mirror the reference)**
```
mobile/                         (submodule)
├── package.json
├── vite.config.ts
├── tsconfig.json
├── capacitor.config.ts
├── index.html
├── Dockerfile                  (multi-stage node:20-alpine)
├── server.js                   (serves dist/ on 0.0.0.0:3000)
├── model/
│   └── app_model.yaml          (ALS model mirror, synced with server)
└── src/
    ├── api/                    (client.ts, auth.ts, types.ts,
    │                            plus per-resource modules:
    │                            job.ts, jobPart.ts, jobLabor.ts,
    │                            jobNote.ts, image.ts, customer.ts)
    ├── auth/                   (AuthContext, LoginPage, ProtectedRoute)
    ├── mechanic/               (feature folder — My Jobs, Job Detail,
    │                            Add Note, Upload Image)
    ├── offline/                (db, offlineStore, syncQueue, SyncManager)
    ├── hooks/                  (useOffline, useFormDraft)
    ├── components/             (TabShell with offline banner, shared)
    ├── config/
    │   └── devMode.ts
    └── theme/                  (Ionic CSS-var overrides to align with
                                 the desktop app's --bg-*/--text-*/--accent-blue)
```

**Deployment (match reference)**
- Dev: `npm install && npm run dev` — Vite on port 5173.
- Build: `npm run build` → static `dist/`.
- Serve standalone: `npm start` → `node server.js` on `0.0.0.0:3000`.
- Docker: multi-stage node:20-alpine, build-args for
  `VITE_ALS_API_URL` / `VITE_DEV_MODE`.
- Capacitor native: `npm run cap:sync`, `cap:ios`, `cap:android`.
- VCP: stack builder wires the container to ALS automatically —
  accept the 5656 default in prod, dev developers set their own.

**Repo layout — git submodule**
The mobile app lives in its own repository
(`thomasgpeters/Smitty-Services-Mobile`) and is mounted into this
repo as a **git submodule** at `mobile/`. This keeps the two
codebases independently versioned with independent CI/CD pipelines
while still giving developers a single `git clone --recursive`
experience.

```
Smitty-Services/              (this repo)
├── src/                       (C++ Wt desktop app — unchanged)
├── mobile/                    (submodule -> Smitty-Services-Mobile)
│   ├── package.json
│   ├── vite.config.ts
│   ├── tsconfig.json
│   ├── public/
│   └── src/
│       ├── api/               (thin wrapper over ALS JSON:API —
│       │                       endpoints + shapes match the C++
│       │                       ApiClient, not reimplementing its
│       │                       logic)
│       ├── features/
│       ├── components/
│       ├── styles/            (mobile-first stylesheet)
│       └── main.tsx
└── .gitmodules                (registers the submodule)
```

Submodule mechanics to get right:
- [ ] **Create the mobile repo** (`thomasgpeters/Smitty-Services-Mobile`)
      on GitHub with its own CI/CD (build / lint / test / PWA
      asset gen / deploy to CDN or S3).
- [ ] Add the submodule from this repo:
      `git submodule add git@github.com:thomasgpeters/Smitty-Services-Mobile.git mobile`
- [ ] Clone instructions update: `git clone --recursive …` or
      `git submodule update --init --recursive` post-clone.
      Note this in the main `README.md`.
- [ ] **Pin policy:** this repo tracks a specific mobile commit.
      Bumping the pin is an explicit PR here (`cd mobile && git
      pull origin main && cd .. && git add mobile && commit`).
      Keeps desktop and mobile releases coordinated.
- [ ] **CI/CD isolation:** desktop pipeline builds C++ only;
      mobile pipeline builds Vite / PWA only. Neither touches the
      other. The mobile pipeline is free to ship on its own cadence.
- [ ] **CLAUDE.md on the mobile side:** a fresh `CLAUDE.md` inside
      the mobile repo keyed to React / Vite / TS conventions, with
      a pointer back to the Smitty-Services server contract
      (`/api/*`). This repo's `CLAUDE.md` gets a new §12 briefly
      describing the split and the submodule workflow.

**Backend prerequisites (blockers)**
- [ ] **Persistent auth.** The C++ app still uses in-memory users
      (`src/Auth.cpp` + `CLAUDE.md §6.2`). The TS app needs a real
      auth endpoint — session cookie or bearer JWT issued by ALS
      (or a thin auth microservice). Both frontends should share
      this. **This is the big gating item.**
- [ ] **CORS.** ALS must allow the mobile origin during dev
      (`http://localhost:5173` by default for Vite) and whatever
      prod origin ships.
- [ ] **Schema plumbing already landed:** `user_devices`,
      `user_preferences`, `notifications` tables are on the
      notifications plan — they'll be built in that phase and the
      mobile app consumes them.

**Feature links back to this plan**
- **Images:** the universal Wt Upload button (desktop path)
  stays as-is. The mobile app ships its own camera-first uploader
  with `capture="environment"`, PWA share-target so a photo
  shared *to* the app from the OS gallery lands in the right
  job. Phase 7 (mobile ergonomics) effectively moves here.
- **Notifications:** the mobile app registers its push subscription
  on first opt-in, hits the same `/api/notifications/*` routes,
  and renders pushes via its own service worker.
- **Notes:** Add Note dialog on mobile is a full-screen sheet
  (not a modal dialog); the Notify multi-select becomes a
  chip-style picker.

**Phase-in**
1. **Skeleton + auth.** Scaffold `mobile/` (submodule, repo
   `Smitty-Services-Mobile`), wire login against AppUser table
   via the direct-JSON:API-filter pattern from the reference,
   basic Ionic tab shell, My Jobs list. Forces the persistent-auth
   + bcrypt decision for both frontends.
2. **Job detail (read).** Parts, labor, notes, images — view-only.
3. **Job detail (write).** Advance part status, advance labor
   status, claim job, add note.
4. **Camera + upload.** Capacitor Camera plugin on native, `<input
   type="file" capture="environment">` on web. Image uploaded via
   the same `/api/Image` endpoint the Wt app will use.
5. **Offline mode.** Wire sql.js + SyncManager; mirror Job /
   JobPart / JobLaborItem / JobNote locally; queue mutations.
6. **Push notifications.** Capacitor Push plugin (native) + Web
   Push (PWA). Opt-in flow, device registration in
   `user_devices`, in-app inbox backed by `notifications`.
7. **Capacitor native packaging.** `cap:sync`, `cap:ios`,
   `cap:android`; store submission pipeline on the mobile repo's
   own CI/CD.

**Out of scope (note it for later)**
- Sharing a single component library between Wt and TS. Different
  rendering models; not worth the bridge.
- Rewriting the desktop app in TS. No plans.

---

### Migration to NUMERIC(12,2) for amount columns
`payments.amount`, `jobs.job_total`, etc. currently use `REAL`. For
production POS this should be NUMERIC(12,2). Track in `docs/BACKLOG.md`
alongside the existing accounting work.

### Mobile push notifications — notify parties involved on new Job Note
Future release. Notes on a job go back and forth between office and
mechanic (and may need to loop in parts / admin as the conversation
evolves). The Add Note dialog gets a **multi-select "Notify"
dropdown** populated with the parties involved in the job; the
author picks zero or more recipients and each gets a push on their
phone.

**Opt-in model (decided)**
Notifications are **opt-in per user**, regardless of device.
Nobody gets pinged unless they have explicitly turned notifications
on for their account and granted permission on the device they're
using. A user who hasn't opted in:
- Does not receive any pushes.
- Does not appear in the "Notify" dropdown as a selectable
  recipient (or shows grayed-out with a "— not enabled" suffix so
  the author understands why they can't pick them).

Opt-in lives in a new **`user_preferences` table** (decided — we'd
rather start with a dedicated prefs table than bolt columns onto
`app_users`, because quiet hours and per-trigger flags are coming
and they belong here):

```sql
CREATE TABLE user_preferences (
    user_id                SMALLINT  PRIMARY KEY REFERENCES app_users(user_id),
    notifications_enabled  SMALLINT  NOT NULL DEFAULT 0,   -- 0 = opt-out (default), 1 = opt-in
    quiet_hours_start      TIME,                           -- NULL = always on
    quiet_hours_end        TIME,
    notify_on_note         SMALLINT  NOT NULL DEFAULT 1,
    notify_on_part         SMALLINT  NOT NULL DEFAULT 1,
    notify_on_status       SMALLINT  NOT NULL DEFAULT 1,
    notify_on_payment      SMALLINT  NOT NULL DEFAULT 0,
    updated_at             TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

Schema notes (following lessons from prior patches):
- Integer defaults (`DEFAULT 0`, `DEFAULT 1`) — safe; no
  string-literal DEFAULTs that would trip the ALS reflection
  quirk captured in `tasks/LESSONS.md`.
- Nullable `quiet_hours_*` so "always on" is expressible without
  magic time values.
- One row per user, keyed by PK — simpler than a per-pref key/value
  table and fast enough at this scale.

Flow:
- Turning the master toggle ON from Settings triggers the
  browser / OS permission prompt, registers the current device
  token in `user_devices`, and UPSERTs
  `user_preferences.notifications_enabled = 1`.
- Turning it OFF flips the flag to 0 and marks the user's
  `user_devices` rows `active = 0` without deleting them, so a
  later re-opt-in reuses the registration history.
- Per-trigger flags (`notify_on_note` etc.) are visible in a
  future Settings sub-section, hidden in v1.

**"Parties involved in the job"** — candidates populated in the dropdown
- The assigned mechanic (`jobs.mechanic_id`)
- The job's associated employee, if the schema carries one
- Any user who has previously authored a note on this job
  (implicit subscription — once you've joined the thread, you
  stay in the loop unless de-selected manually)
- All Admins and Office Managers (always available as pickable
  recipients, even if they haven't touched the job yet)
- Explicit "watchers" if a `job_watchers` table is added later
  (optional future)

Only users with `notifications_enabled = true` AND at least one
active `user_devices` row are eligible; others appear greyed out
in the dropdown.

The note author is excluded from the **default** selection but can
be added manually (edge case: confirming receipt on a shared
device).

**UI**
- [ ] Replace the earlier "Notify mechanic" checkbox idea with a
      multi-select widget (`WSelectionBox` in Extended mode or a
      chip-style picker) inside `showAddNoteDialog()`. Label:
      "Notify".
- [ ] Each row shows `"<display name> — <role>"` so the picker is
      meaningful (e.g. "Mike the Mechanic — Mechanic", "Office
      Manager — Office Manager").
- [ ] Default selection: assigned mechanic + all prior note
      authors on this job (minus the current author).
- [ ] Helper text: "Pick the teammates who should see this note on
      their phone."
- [ ] If nobody is selected, fire no push (save the note only — a
      valid pattern for drive-by status updates).

**Backend**
- [ ] POST body extends with `notify_user_ids: [<username>, ...]`
      (or a side-call to a dedicated endpoint — decide during
      build).
- [ ] Custom ALS route `/api/notifications/note` takes
      `{job_id, note_id, recipients: [username, ...]}`, resolves
      each recipient's active device tokens from `user_devices`,
      dispatches one push per token, records each send in
      `notifications` for auditing.
- [ ] Tokens returning 410 Gone / invalid-registration flip to
      `active=false` in `user_devices`.

**Infrastructure (decide before building)**
- [ ] Provider: Firebase Cloud Messaging (single SDK for iOS +
      Android + web push), native APNs + FCM pair, or web-push
      only (browser Notifications API, no native app required).
      **Leaning FCM** if native mobile is on the roadmap;
      web-push first if we want zero mobile-app work.
- [ ] New table `user_devices` — (user_id, device_token, platform,
      last_seen, active). A user can have multiple devices;
      registration happens once from their phone / browser.
- [ ] New table `notifications` — (id, job_id, note_id, recipient,
      sent_at, provider_response, status). Powers an in-app inbox
      later and drives retry on transient failure.
- [ ] Secrets: `FCM_SERVER_KEY` / APNs `.p8` loaded from env only,
      never in the C++ client (per `CLAUDE.md §11.5`).

**Triggers (extend later — start with Note only)**
- Job Note added → selected recipients
- Part received → mechanic waiting on that part
- Job status → Complete → office / owner
- Payment received → office

**UI polish**
- [ ] After save, show a toast "Notified N teammate(s)" or "Note
      saved (no one notified)" so the author knows what went out.
- [ ] **Settings page — Notifications section** (new):
      - Master toggle "Enable notifications on this device" which
        on first-on triggers the browser / OS permission prompt
        and registers the current device in `user_devices`.
      - When on, a list of the user's registered devices with a
        per-row de-register button (e.g. remove an old phone).
      - Optional quiet hours picker.
      - Optional per-trigger preferences once additional triggers
        beyond Note exist (Part received, Status → Complete, etc.).
- [ ] **Bell icon on each note card** (confirmed with user):
      - Shows when at least one recipient was notified for that
        note.
      - Tooltip lists the recipients ("Notified: Mike, Office
        Manager").
      - Small sizing, positioned near the meta line so it doesn't
        clash with the floating delete X.
      - Click opens a small popover with the full send log
        (recipient, sent_at, delivery status) pulled from the
        `notifications` audit table — useful when someone says
        "I never got the ping."

**Security / privacy**
- [ ] Device tokens are per-user secrets — never expose
      cross-user via API.
- [ ] Rate-limit: max N pushes per recipient per hour to prevent a
      runaway script from spamming a device.
- [ ] Audit trail in `notifications` with timestamp, author,
      recipient, trigger, job context.
- [ ] Authorization: only users with write access to a job (admin,
      office manager, or the assigned mechanic) can fire a
      notification on it.

**Phase-in suggestion**
1. **Opt-in plumbing + web push for the Note trigger.** Ship the
   Settings toggle, `user_preferences.notifications_enabled`,
   `user_devices`, `notifications` audit table, multi-select
   dropdown, bell icon on note cards, and web-push delivery
   (browser Notifications API — no mobile app yet). Most of the
   engineering value lands here.
2. **FCM + native mobile.** Reuse the same schema and dropdown;
   swap the push provider. Each user's device rows now include
   iOS / Android entries alongside web push.
3. **Additional triggers.** Parts received, status → Complete,
   payment received. Each is just a new call site emitting to the
   same `/api/notifications/*` endpoints.

---

## Parking lot

_Items noticed mid-task that belong elsewhere (usually `docs/BACKLOG.md`)._

- Base64 "hashing" in `src/Auth.cpp:12-44` — still not a real hash.
  Known hazard; must be fixed before production. (See `CLAUDE.md §6.2`.)
- In-memory user store — `Auth::seedDefaultUsers()` recreates four
  defaults every restart. `addUser`/`resetPassword` don't persist.

---

## Review — session 2026-04-19

**Landed:** POS plan doc, CLAUDE.md, tasks/ scaffolding, DataBus signal
for cross-entity refresh, payments table (patch + schema), ApiClient
HTML-response hardening, Wt deprecation-warning suppression.

**Outstanding:** browser smoke test of the DataBus changes. All code
compiled locally on user's machine (sandbox had no Wt/Boost). Payments
endpoint confirmed live by the user after ALS rebuild.

**Lessons captured in `tasks/LESSONS.md`:**
- Registry presence ≠ table presence — always grep `schema.sql` first.
- `parse_error '<'` means ALS returned HTML; diagnose table/resource/
  running-state before touching C++.
- Apple Clang leaks deprecation warnings through `SYSTEM PRIVATE`
  includes.
- ALS needs `rebuild-from-database` after DDL, not just a restart.
- Meta files at repo roots use UPPERCASE (TODO, LESSONS, CLAUDE, README).

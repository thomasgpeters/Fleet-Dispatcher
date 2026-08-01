# Design Decisions

This document records the key architectural and design decisions made throughout the Smitty Services project, along with the reasoning behind each choice.

---

## 1. Technology Stack

### C++17 with Wt Framework

**Decision:** Use C++17 and the Wt (Witty) web framework for the full-stack application.

**Rationale:**
- Wt provides a server-side widget model that generates HTML/JS automatically, eliminating the need for a separate frontend framework
- C++17 offers modern language features (structured bindings, `std::optional`, `if constexpr`) while maintaining performance
- Single-language stack simplifies deployment and debugging
- Wt handles WebSocket/long-polling transparently for reactive UI updates

### JSONAPI Backend via ApiLogicServer

**Decision:** Use a JSONAPI-compliant REST backend rather than direct database access.

**Rationale:**
- Decouples the UI layer from the database layer
- ApiLogicServer auto-generates CRUD endpoints from the database schema
- JSONAPI specification provides a standard format for relationships, pagination, and filtering
- Allows the backend to be swapped or extended independently

### Single PDF Library (pdfgen)

**Decision:** Use PDFGen (`pdfgen.h`/`pdfgen.c`) as the sole PDF generation library across all reports.

**Rationale:**
- Single-file C library with zero external dependencies
- Public domain license — no licensing concerns
- Supports text, images (PNG, JPG, BMP, PPM), lines, rectangles, and multi-page documents
- Small footprint (~1500 lines) — easy to audit and maintain
- Consistent API surface across all report types through the `PdfReport` base class
- No need for heavyweight libraries like libharu, PoDoFo, or Qt PDF modules

### nlohmann/json (Header-Only)

**Decision:** Use nlohmann/json for all JSON parsing and serialization.

**Rationale:**
- Header-only — no build complexity
- Intuitive API that reads like Python dictionaries
- Widely used and battle-tested in production C++ codebases
- Handles JSONAPI response structures cleanly

---

## 2. Architecture Patterns

### Entity Registry Pattern

**Decision:** Centralize all entity metadata (columns, types, labels, display names) in a single `EntityRegistry` singleton.

**Rationale:**
- Avoids duplicating column definitions across list views, detail views, and edit dialogs
- New entities can be added by registering them in one place (`EntityRegistry.cpp`)
- Generic `EntityListView` and `EntityDetailView` base classes can render any entity
- Custom detail pages (e.g., `JobDetail`) override hooks rather than reimplementing the full widget

### Base Class with Virtual Hooks

**Decision:** `EntityDetailView` provides a base UI with virtual hooks (`populateFields`, `addChildContent`, `customEditField`) that subclasses override.

**Rationale:**
- Eliminates boilerplate — the base class handles record loading, field rendering, and the edit dialog framework
- Subclasses only override what they need to customize (e.g., combo boxes for foreign keys, child grids for line items)
- Consistent look and feel across all detail pages

### AppSettings Singleton

**Decision:** Use a singleton for application-wide settings (API endpoint, currency, date format, discount tiers).

**Rationale:**
- Settings are global and rarely change during a session
- Any component can access settings without dependency injection
- Settings page writes to the singleton; all views read from it

---

## 3. Job Workflow Design

### Status-Based Lifecycle

**Decision:** Jobs follow a fixed status progression: New -> In Progress -> Waiting Parts -> Road Test Pending -> Complete.

**Rationale:**
- Maps to real-world service center workflow
- "Waiting Parts" status integrates with the parts receiving pipeline
- "Complete" status locks all editing to prevent accidental modification of finished work
- Dashboard filters use status to show active vs. completed jobs

### Reactive Totals (Client-Side Computed)

**Decision:** Labor Total, Parts Total, Sub-Total, Discount, and Total are computed reactively on the client whenever line items change, rather than being stored and loaded from the server.

**Rationale:**
- Eliminates stale data — totals always reflect the current line items
- `renderTotals()` is called after every add/delete operation on labor or parts
- The computed totals are persisted back to the job record via `updateJobTotals()` for use in lists, dashboards, and reports
- Header fields (`labor_total`, `parts_total`, `job_total`, `estimated_cost`, `actual_cost`) are hidden from the detail form since they're shown in the reactive Job Totals card

### Tier-Based Discount from Customer Revenue

**Decision:** Discount percentage is computed automatically from the customer's annual payment revenue using configurable tier thresholds, not manually entered per job.

**Rationale:**
- Rewards loyal customers automatically based on their spending history
- 4-tier system (configurable in Settings) allows flexible discount structures
- Revenue is calculated by summing all Payment records for the customer
- Highest qualifying tier wins — incentivizes customers to reach the next threshold
- Computed discount is persisted to the job record so reports and lists reflect it

### Parts Receiving Workflow

**Decision:** Parts go through a multi-stage workflow: Pending (0) -> Received (1) -> In-Stock (2) -> Installed (3).

**Rationale:**
- Tracks the full lifecycle of a part from order to installation
- "Received" indicates the part has arrived at the shop
- "In-Stock" creates/updates a Product inventory record (`units_in_stock` incremented)
- "Installed" decrements inventory, confirming the part is on the vehicle
- Dashboard "Pending Part Deliveries" shows only Pending (0) parts
- Each status transition has a distinct button (Receive / Stock / Install) with appropriate actions

---

## 4. UI/UX Decisions

### Print and Generate Invoice Buttons at Header Level

**Decision:** Place the "Print" (blue) and "Generate Invoice" (standard) buttons in the page header, at the same level as the page title, floating right.

**Rationale:**
- Immediately visible without scrolling
- Consistent with common document-oriented UIs where print/export actions are top-level
- Blue print button draws attention as the primary action
- Standard-styled invoice button is secondary, less prominent
- Both float right to avoid interfering with the title and back navigation

### Dark/Light Theme via CSS Custom Properties

**Decision:** Implement theming through CSS custom properties (`--bg-page`, `--text-primary`, etc.) toggled by a `data-theme="dark"` attribute on `<html>`.

**Rationale:**
- Pure CSS solution — no server round-trip needed for theme changes
- Single stylesheet with variable overrides keeps bundle size small
- NavBar and SideBar use dark backgrounds in both themes for visual consistency
- Toggle is instant via JavaScript `setAttribute`

### Inline Section Totals

**Decision:** Show bold right-aligned totals below each section table (e.g., "Labor Total (4.50 hrs): $675.00") in addition to the Job Totals summary card.

**Rationale:**
- Users can see the running total for each section without scrolling to the bottom
- Totals update reactively alongside the summary card
- Matches common invoice/estimate layouts users are familiar with

### Computed Fields Hidden from Edit Dialog

**Decision:** Fields like `labor_total`, `parts_total`, `job_total`, `estimated_cost`, `actual_cost`, and `discount_percent` are excluded from the edit dialog.

**Rationale:**
- These values are computed from line items and tier rules — manual editing would create inconsistencies
- Reduces form clutter in the edit dialog
- Users interact with the source data (line items, settings) rather than derived values

### Mechanic Dashboard as Kiosk-First Design

**Decision:** Build a dedicated `MechanicDashboard` view with an entirely separate UI optimized for touchscreen kiosk operation, rather than adapting the existing Dashboard or JobDetail pages.

**Rationale:**
- Mechanics operate with greasy hands — they tap with a knuckle, not a fingertip. All interactive elements need minimum 48–54px touch targets with generous spacing
- A keyboard-free design avoids the need for an on-screen keyboard or physical keyboard in the shop bay
- Keeping it as a separate view (not a responsive variant of Dashboard) means the office-facing UI stays unchanged — no compromises in either direction
- The two-panel `WStackedWidget` pattern (cards view / detail view) mirrors how a mechanic thinks: "show me my jobs" → "show me this job's parts and labor"

### Touch Target Sizing for Shop Floor Use

**Decision:** Set minimum button height to 48–54px with 14–28px padding, and use `touch-action: manipulation` to disable double-tap-to-zoom.

**Rationale:**
- Apple and Google HIG recommend minimum 44px touch targets; 48px+ accounts for imprecise taps with knuckles or gloved fingers
- `touch-action: manipulation` prevents the 300ms delay and accidental zoom on mobile browsers
- Semi-large "View Details" buttons span the full card width for easy acquisition
- The "Work Complete" button is the largest (54px, bold green, top-right) because it's the most consequential action

### Visual Progress Pipeline

**Decision:** Display a 4-stage horizontal progress tracker (Receive Parts → Disassembly → Assembly → Test) at the top of the job detail view, with done/active step highlighting.

**Rationale:**
- Gives the mechanic an at-a-glance understanding of where the job stands in the physical workflow
- The stages map to the real-world shop floor process, not the administrative job statuses (New, In Progress, etc.)
- Stage completion is inferred from parts status data rather than requiring the mechanic to manually advance stages
- Arrow connectors between stages reinforce the left-to-right progression

### Per-Part Action Buttons (Not Batch)

**Decision:** Each part card has its own action button (Receive / Stock / Install) rather than a batch "advance all parts" button.

**Rationale:**
- Parts arrive at different times — an alternator may arrive Monday while the gasket comes Wednesday
- One-tap-per-part matches the physical reality of handling individual parts
- The button label changes based on the part's current status, so the mechanic always knows what the next step is
- Color-coding (green for Receive, purple for Stock, blue for Install) provides visual differentiation without reading text

---

## 5. PDF Report Architecture

### PdfReport Base Class

**Decision:** All PDF reports extend a common `PdfReport` base class that wraps the pdfgen library.

**Rationale:**
- Consistent page layout (margins, header, footer, page numbers) across all reports
- Shared helper methods: `addText`, `addTextBold`, `addTextRight`, `drawTableHeader`, `drawTableRow`, `drawSectionTitle`, `formatCurrency`, `checkPageBreak`
- Subclasses only implement `generateContent()` — the base handles page lifecycle
- Company name and branding centralized in one place

### Job Work Order PDF Layout

**Decision:** The Job PDF is designed to fit on 1-2 pages with automatic page breaks.

**Rationale:**
- Most jobs have a manageable number of labor and parts items
- Page breaks checked before each section and each table row
- Layout: header -> job info -> customer/vehicle two-column -> labor table -> parts table -> totals box -> notes
- Totals box is right-aligned (250pt wide) with background fill for visual emphasis
- Notes section only appears if the job has notes

---

## 6. Data Integrity Decisions

### Amount Auto-Calculation

**Decision:** Line item amounts are auto-calculated (labor: `hours * rate`, parts: `quantity * unit_price`) and rounded to 2 decimal places.

**Rationale:**
- Prevents arithmetic errors from manual entry
- `std::round(amount * 100.0) / 100.0` ensures consistent cent rounding
- Users enter the inputs (hours, rate, qty, price); the system computes the result

### Job Totals Persisted on Change

**Decision:** After every line item add/delete, `updateJobTotals()` writes the computed `labor_total`, `parts_total`, and `job_total` back to the job record via API.

**Rationale:**
- List views and dashboards can display job totals without re-fetching line items
- Reports and invoices can reference stored totals for consistency
- The reactive UI and stored values stay in sync because persistence happens on every change

### Inventory Integration on Part Status Change

**Decision:** Stocking a part increments `Product.units_in_stock`; installing a part decrements it.

**Rationale:**
- Maintains accurate real-time inventory counts
- If no Product record exists when stocking, one is automatically created
- The `product_id` link is stored on the JobPart for future reference
- Prevents negative stock (`newStock = max(0, currentStock - quantity)`)

---

## 7. Configuration Design

### 4-Tier Discount System

**Decision:** Support 4 configurable discount tiers based on customer annual revenue.

**Rationale:**
- Tier 1 is a base discount applied to all customers (can be 0%)
- Tiers 2-4 have revenue thresholds — customers crossing a threshold get the higher discount
- Percentages and thresholds are editable in the Settings page without code changes
- The system checks tiers in order (1 -> 4), with the highest qualifying tier winning

### Horizontal Tabs for Menu Privileges by Role

**Decision:** Display the per-role privilege matrix using horizontal tabs (one tab per role) instead of vertically stacking all role tables on the page.

**Rationale:**
- Reduces visual clutter — only one role's privilege table is visible at a time
- Keeps the page height manageable (9 menu items × 4 roles = 36 rows was excessive when stacked)
- Uses `WStackedWidget` to show/hide tab panes without server round-trips
- Tab bar styling (underline-active pattern) is consistent with modern UI conventions
- Each tab retains its own set of checkboxes stored in `rbacCheckboxes_`, so switching tabs preserves state

### Date Format Presets

**Decision:** Offer 6 preset date formats rather than a free-form format string.

**Rationale:**
- Covers the most common US, European, and ISO formats
- Prevents invalid format strings that could break date rendering
- Custom parser supports: `YYYY`, `MM`, `DD`, `MMMM` (full month), `MMM` (short month), `dddd` (full weekday), `ddd` (short weekday), `Do` (ordinal day)

---

## Forward-looking direction

### Push business rules into ALS, not C++ orchestration

**Direction:** Net-new business rules (constraints, aggregates, derivations, "when X happens, do Y" reactions) should land in the ApiLogicServer middleware as declarative rules, **not** in the C++ Wt frontend as multi-call orchestration. Existing C++ orchestration that meets the criteria below should be migrated as opportunities arise — but not refactored ahead of need.

**Why:**

- **Single source of truth across clients.** Smitty has at least two consumers of the ALS API today (the C++ Wt desktop app and the React/Ionic/Capacitor mobile app in `mobile/`), with more plausibly to come (Node integrations, batch jobs, the parent VCP control plane). A rule expressed in C++ only protects users who came in through the desktop. A rule expressed in ALS protects every client by construction.
- **Atomicity.** ALS rules execute inside the same transaction as the request that triggered them. Today's `Inventory::applyDelta()` issues two HTTP calls in sequence — a `PATCH /Product` and a `POST /InventoryMovement`; if the second fails after the first succeeded we log loudly and accept reconciliation drift (`src/Inventory.cpp:55-63`). An ALS rule version of this would be one DB transaction; the second insert can't strand the first.
- **Declarative > imperative.** ALS rules describe *what* the invariant is ("Product.units_in_stock = sum(child quantities)" / "after Purchase.status -> Received, fan out to PurchaseItems"), not the *how*. That style audits better and lets non-Smitty domain owners read the logic.
- **VCP alignment.** The parent Visual Control Plane project plans to surface natural-language rule authoring as a generic capability across business apps (Smitty, Student Onboarding, Contractor Quotes, etc.). Smitty's rules, expressed in ALS, become a reference implementation and dogfood input for that generic feature.

**What's a natural ALS-rule candidate** (start here when migrating):

- **Aggregates.** `Job.parts_total = sum(JobPart.amount)`, `Job.labor_total = sum(JobLaborItem.amount)`, `Customer.lifetime_revenue = sum(Order.total_amount)`. Today these are computed client-side in `JobDetail::recomputeTotals()` and similar — convert to ALS aggregate rules and the client just reads them.
- **Reactions to status transitions.** `Inventory::receivePurchase()` (`src/Inventory.cpp`) → an "after Purchase.status changes to 'Received', for each PurchaseItem write +qty into the ledger and bump Product.units_in_stock" rule. `Inventory::applyDelta()`'s ledger insert → an "after Product.units_in_stock UPDATE, INSERT into inventory_movements" rule.
- **Constraints.** `units_in_stock >= 0` (today's `applyDelta` clamps imperatively at line 33). `JobPart.quantity > 0`. `Payment.amount > 0`. ALS expresses these as `@validate` rules; violations come back as 4xx without C++ guards.
- **Sums-into-cache.** Phase B of `tasks/TODO.md §4c` (when multi-bin lands) wants `products.units_in_stock = sum(product_locations.qty)`. That's a textbook ALS sum-aggregate.

**What stays in C++** (UI concerns, non-rule territory):

- Wt widget rendering (pillbox, Movements grid, dialogs, toasts).
- Display formatting (date strings, currency symbol prefixing, icon mapping).
- Authorization gates at the *display* layer (`Auth::canEdit(...)` to hide buttons). Authorization at the *write* layer should still live in ALS — RBAC rules are exactly the kind of thing that has to fire regardless of client.
- Click-through navigation, tab state, keyboard shortcuts.

**Migration policy:** opportunistic, not preemptive. When a new business rule comes up, prefer ALS first — only fall back to C++ orchestration if the rule needs information the middleware doesn't have. When a C++ orchestration is touched for an unrelated reason and a clean ALS-rule version would be a smaller diff than the local edit, take the migration. Don't rewrite working code just to relocate it — Smitty is a live app on a small team.

**Learning curve:** the team needs ALS-rule fluency before this becomes the default. Pick one easy candidate to ship as a proof-of-concept (the inventory-movements ledger insert is a strong choice — single-table reaction, well-bounded, already known to have an atomicity gap), measure the C++ delta removed, then decide whether the next layer of rules pulls in the same direction.

---

## Integration invariants

Rules that bind every cross-app integration Smitty participates in (today: Fleet Dispatcher; future: potentially Student Onboarding, Contractor Quotes, etc.). Locked from the Fleet Dispatcher planning session; codified here so they don't need to be re-negotiated every integration.

### Integration schemas are additive-only

**Decision:** Schema changes made in service of a cross-app integration — new columns on shared tables, new tables that the other side mirrors — are **`ADD COLUMN IF NOT EXISTS` and `CREATE TABLE IF NOT EXISTS`** only. No `DROP COLUMN`, no renames, no type changes to existing columns.

**Rationale:**

- The other side has already published data into columns Smitty added. Dropping the column silently loses that data on the peer.
- Column-rename in one Postgres schema propagates as a "column disappeared" to the peer's ALS reflection. Better to leave the old column, mark it deprecated in a header comment, and add the new one alongside.
- Type widening (`SMALLINT` → `INTEGER`) is safe *at the DB level* but breaks byte-for-byte identical DDL (see the next decision), so it's also disallowed on shared tables without a coordinated cross-app migration.
- Rollback of a Smitty deploy must never orphan mirror data the peer has already sent. Additive-only makes rollback trivial: the old binary ignores the new columns.

**Scope:** every integration table listed in a `docs/INTEGRATION_*.md` doc's "shared schema" section. Non-integration tables (Smitty's own `jobs`, `job_parts`, `payments`, etc.) are unaffected by this rule and follow the normal §7.2 patch workflow.

**Deprecation path** (if you really do need to retire a column): mark it deprecated in the schema comment for one release cycle, ensure both apps have stopped writing to it, then plan a coordinated drop as its own cross-app migration. Never unilateral.

### Integrated apps share identical DDL for shared tables

**Decision:** When two apps integrate over shared table shapes (e.g. Smitty ↔ Fleet Dispatcher on `vehicles` + `maintenance_schedules` + `vehicle_out_of_service`), the DDL for those tables shares an **identical column set + semantics + correlation-key convention.** Native primary-key types and local foreign keys are allowed to differ between the apps (Smitty uses `SMALLINT` PKs, Fleet uses `UUID`); each app maintains its own copy in its own Postgres schema; the sync layer replicates by correlation key (VIN for the Fleet ↔ Smitty vehicles surface), not by identical row images.

**Softened from byte-identical DDL** per Fleet response v1 F.3 / Smitty response v2 S.1 (`docs/INTEGRATION_SMITTY.md`). The original invariant was unworkable because Fleet's golden rules use UUID PKs + typed FK relationships while Smitty uses SMALLINT PKs + `JSONB` specs. Requiring byte-identical DDL would force one side to violate its house conventions. Sharing column-set + semantics is the achievable invariant that preserves what actually matters — a single JSON:API attribute contract both sides write against.

**Rationale:**

- Divergent column names or divergent semantics for the same-named column means the sync layer has to translate between shapes, which is an ever-growing source of bugs and version-skew.
- Identical column set + semantics means either side's ALS reflection produces the same JSON:API attribute set (modulo PK type); consumers on both sides write against one contract.
- Native PK types are locally-scoped — they do not appear in the JSON:API attribute contract in a way that matters for the peer app, because correlation goes through the correlation key (VIN), not the local PK.
- The **on/off toggle** on the peer side (Fleet holds the switch today) works cleanly because each app can operate on its local copy independently — no dangling FKs into the other schema.

**Scope:** every integration table. The column set + semantics are authored once in the "shared schema" section of the integration doc, then applied by both sides using each app's native PK conventions.

**Enforcement:** when a change to a shared table is proposed, it lands as a coordinated patch on both repos. **Column-set drift is a bug; PK-type divergence is expected.** A drift check that compares column names + types (excluding PKs) between the two apps' `schema.sql` files is the CI gate this decision eventually wants — not built yet, but the invariant is enforceable.

### Peer app owns the integration on/off switch; Smitty degrades gracefully

**Decision:** For integrations where Smitty is the passive party (Fleet Dispatcher, today), the peer app owns the runtime toggle. **Smitty does not add a matching switch.** Mirror fields on shared tables simply become NULL / stale when the peer stops publishing. Smitty's UI must render sensibly against NULL and stale data — no error surface, no dependency on the mirror being fresh.

**Rationale:**

- Two toggles multiply the failure modes: on both / on one / on the other / off both, with different behaviours per combination. One toggle is one truth.
- Smitty's UI must be honest about staleness regardless of the toggle state — if the last odometer push was three days ago (whether because Fleet's toggle is off, or because the pull is stuck), the vehicle detail page shows "as of 3d ago" or "no mileage on file". The UI treatment doesn't need to know why the mirror is empty.
- Rollback of a Smitty deploy that included integration UI must not leave users looking at a dead toggle. Not having one avoids the class of bug entirely.

**Scope:** every integration where Smitty is the mirror side rather than the source of truth. When Smitty is the source (e.g. Smitty → Fleet publishing work orders), Smitty controls what it publishes but doesn't gate at a runtime toggle — publishing is on by default; disabling it is a config change, not an admin UI switch.

**UI patterns for graceful degrade:**

- Empty-state text on mirror fields: "No mileage on file" / "Status unknown" / "No maintenance schedule set", not blank cells.
- Staleness indicators where the freshness matters: "Odometer 128,340 mi — as of 3d ago" rather than showing the value without provenance.
- Buttons that depend on a mirror value (e.g. Schedule Service, which needs an odometer for mileage-based intervals) stay clickable but the dialog flags the missing input rather than silently defaulting.

### Multi-tenant integrations use middleware-enforced scoping + field stripping

**Decision:** When Smitty serves multiple customer types through the same JSON:API surface (Fleet Dispatcher, a subset of Smitty's customers; future integrations may add Student Onboarding, Contractor Quotes, etc.), the **scoping and field-stripping happen at the ALS-side service-token middleware, not in client-supplied query filters.** Each service token is bound to a scope in a token registry; the middleware rewrites reads and strips response fields per that scope before the JSON:API response is returned.

Landed in Fleet response v1 B.1–B.4 and Smitty response v2 S.3.b + S.3.d (`docs/INTEGRATION_SMITTY.md`).

**Rationale:**

- **Correctness by construction, not by trust.** Client-supplied filters (`?filter[customer_id]=FLEET`) can be omitted, tampered with, or missed on a new endpoint. Middleware enforcement makes third-party data structurally invisible to the peer token — there is no client-side path through which Fleet could accidentally receive a third-party customer's job data.
- **Contract stability.** Peer app writes against the JSON:API contract; middleware pins the scope invisibly. Changes to the scope (e.g. Fleet acquires a new customer subset) are configuration changes on Smitty's side, not contract renegotiations.
- **Business-model confidentiality.** Smitty's retail pricing / per-job negotiated margins are a competitive concern that must not cross the boundary even accidentally. Stripping the fields at the response layer means the C++ frontend and ALS storage keep the full picture; only the peer's view is redacted.
- **Same shape works for every future integration.** Student Onboarding, Contractor Quotes, and any other multi-tenant peer follow the same token-registry + scope + strip-profile pattern. One implementation covers the class.

**Scope:** every endpoint reachable by a service token (`X-Service-Token` header today; likely JWT later). The middleware applies the scope filter *and* the strip profile before the JSON:API response leaves ALS. Endpoints reachable only by session-authenticated users (mechanic dashboard, admin UI) are unaffected.

**Token registry shape** (Phase 1, likely env-var-loaded JSON on the ALS side; move to a `service_tokens` table when there are more than a couple of tokens):

```json
{
  "fleet-2026": {
    "customer_id": "FLEET",
    "vin_set": null,
    "strip_profile": "fleet_at_cost"
  }
}
```

**Strip profiles** are named + reusable. `fleet_at_cost` drops `jobs.{estimated_cost, actual_cost, parts_total, labor_total, job_total, discount_percent}`, `job_parts.{unit_price, amount}`, `job_labor_items.{rate, amount}` — the retail side — and keeps `job_parts.unit_cost` + `job_labor_items.hourly_cost` (the at-cost side per the Fleet contract). Different peers can share the same profile if their confidentiality boundary is identical.

**Anti-pattern to avoid:** "trust the peer to filter." Every time we've relied on that in any system, someone eventually shipped a peer version that forgot the filter and leaked data. Middleware enforcement removes the failure mode.

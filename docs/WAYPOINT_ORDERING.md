# Design note — waypoint ordering moves to the middleware

**Status:** implemented 2026-08-01 (activates on the next ALS regen; see Deploy).
**Context:** item 3 of the "client-calculated values → LogicBank" audit
([`TODO.md`](TODO.md)). Doctrine: [`LOGICBANK_RULES.md`](LOGICBANK_RULES.md).

## The problem

A trip's stops live in `waypoint (trip_id, seq, …)` with `UNIQUE (trip_id, seq)`.
The **mobile** `TripWaypointsPage.tsx` maintained `seq` procedurally, and it was
the ugliest calculated-value in either client:

- **Add** computed the new `seq` (insert *before the destination*) and then bumped
  the destination and everything after it down a slot — **back-to-front, one PATCH
  per row**, to dodge the unique constraint mid-shuffle.
- **Reorder** wrote every stop **twice** (`seq = 1000+i`, then `seq = i+1`) — a
  two-phase offset dance, again only to avoid a transient duplicate `seq`.
- **Delete** left a **gap** in the sequence (1, 2, 4, …).

Three things are wrong with that living in the client:

1. **Races.** Each PATCH is its own HTTP request = its own DB transaction. Two
   people adding a stop to the same trip both compute the same `seq` and both try
   to bump — a `UNIQUE (trip_id, seq)` collision; one write fails, or the order
   corrupts. The litmus test says out loud: *if the desktop grew a waypoint editor
   and reimplemented this, two clients editing one trip would collide.*
2. **Non-atomic.** A multi-PATCH shuffle that fails halfway leaves the route
   half-shifted — there's no transaction around it.
3. **Chatty + duplicated.** Reorder is `2N` round-trips; add is up to `N` PATCHes
   + a POST. And the *same* fragile dance already exists a second time in the
   geospatial optimizer (`recompute.py::_persist_order`).

## The invariant (what belongs in the middleware)

> Within a trip, `seq` is a **contiguous, gap-free 1..N** ordering, and every
> reshuffle is **atomic and collision-free**. Clients express **intent** (add one
> stop / move one stop / delete one stop) — never raw `seq` arithmetic against the
> unique constraint.

## Keystone: a **deferrable** unique constraint

```sql
UNIQUE (trip_id, seq) DEFERRABLE INITIALLY DEFERRED
```

An immediate unique constraint is checked after **each row** — which is the whole
reason the offset/back-to-front dances exist. `INITIALLY DEFERRED` moves the check
to **COMMIT**, so any number of rows can be reshuffled inside one transaction with
transient duplicate `seq` values, as long as the *end state* is unique. This one
change is what lets the reshuffle move server-side cleanly. It benefits three
writers at once:

| Writer | Before | After |
| --- | --- | --- |
| LogicBank (API writes) | — | reshuffles siblings freely in the ALS commit txn |
| Mobile client | multi-PATCH offset dances | one PATCH per gesture (intent) |
| Geospatial optimizer (direct SQL, one txn) | two-phase `1000+i` then `i+1` | single-phase `i+1` |

## Division of responsibility

- **Client → intent, one write per gesture.** Add posts one waypoint (a trivial
  `max(seq)+1` append hint — no bump loop, no "find the destination"). Reorder
  PATCHes **only the moved** waypoint to its target `seq`. Delete just deletes.
- **LogicBank → the invariant**, for every write that funnels through the ALS
  commit (`route_governance.py`):
  1. **Place-on-insert** — an intermediate stop (fuel/lunch/load/…) is repositioned
     to just **before the destination** and siblings shift down; origin/destination
     (or a trip with no destination yet) append at the end. The client's hint is
     normalized here, so any client — mobile, a future desktop editor, a raw API
     call — gets the same placement.
  2. **Shift-on-move** — when a waypoint's `seq` changes `old → new`, the stops in
     the crossed range shift by one so the order stays dense 1..N. This is what
     turns a single-row "move to slot K" PATCH into a correct reorder.
  3. **Densify-on-delete** — after a stop is removed, the remaining stops renumber
     to 1..N (no gaps).
- **Optimizer → trusted server-side batch.** `recompute.py` writes `seq` straight
  to Postgres (by design — spatial recompute bypasses ALS so it emits no Kafka
  feedback loop; see [`SPATIAL_GIS_DATA_CONSIDERATIONS.md`](SPATIAL_GIS_DATA_CONSIDERATIONS.md)).
  It does its whole reorder in **one transaction**, so the deferrable constraint
  lets it drop its own two-phase dance. It does **not** trip the LogicBank move
  rule (those fire on ALS writes, not direct SQL) — no feedback loop, no conflict.

## Why not enforce "origin first / destination last" as a hard constraint?

The *integrity* invariant (unique, dense, atomic) is what would corrupt data if a
second client got it wrong — that's LogicBank's job. "Intermediate stops sit before
the destination" is a **placement policy**: nice, and now applied server-side on
insert so it's consistent, but a stop dragged past the destination is odd UX, not
corruption. We keep it a placement default rather than a rejection so a driver's
manual drag is always honored. (A guard rejecting reorders that move a stop past
the destination is a possible follow-up — noted, not built.)

## Deploy & caveats

- **Activation.** Like the other audit items, the rules go live only after an ALS
  **regen from the updated schema** + `make als-extensions` on the Linux box.
  Until then `route_governance.py` isn't loaded; the client's simplified add still
  works (a plain append), but **reorder needs the rule** (a single-row move PATCH
  relies on the server shift) — so regen before relying on drag-reorder. This is
  the same "active after regen" gap noted for `unread_count`.
- **LogicBank version-sensitivity (flagged inline, like our Wt spots).** The event
  hooks used — assigning the inserted row's own `seq` before flush, reading
  `old_row.seq` on update, and adjusting sibling rows within the same commit — vary
  by LogicBank version (`early_row_event` / `row_event` / `after_flush_row_event`
  availability and whether sibling mutations re-flush). **Validate on the Linux
  box.** The rule file documents the expected hook per operation.
- **Verified** (`/verify-db`, throwaway PG16): schema + seed apply cleanly with the
  deferrable constraint; existing seed waypoints (already unique per trip) are
  unaffected.

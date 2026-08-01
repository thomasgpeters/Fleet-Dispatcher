"""Fleet Dispatcher — route/waypoint ordering governance (LogicBank logic).

Owns the ONE invariant for a trip's stops: `waypoint.seq` is a contiguous,
gap-free 1..N ordering per trip, and every reshuffle is atomic + collision-free.
Clients express intent (add one stop / move one stop / delete one stop); the seq
arithmetic that used to live in the mobile TripWaypointsPage — insert-before-
destination, back-to-front bump loops, the two-phase reorder dance — lives here
now, governed once at the ALS commit for every client. Full rationale + the
deferrable-constraint keystone: docs/WAYPOINT_ORDERING.md.

Sibling of fleet_governance.py / comms_governance.py; same install path
(als-extensions/install.sh copies logic_discovery/*.py; re-run `make
als-extensions` after every ALS rebuild).

Requires `UNIQUE (trip_id, seq) DEFERRABLE INITIALLY DEFERRED` on `waypoint` (the
check is deferred to COMMIT, so we can reshuffle siblings with transient duplicate
seq values inside the one commit transaction).

NOTE (LogicBank/ALS version-sensitive — validate on the Linux box, like our Wt
spots): model class name is `waypoint` -> Waypoint. The three hooks below need,
respectively, to (1) set the *inserted* row's own seq before flush, (2) read
`old_row.seq` on update, and (3) adjust *sibling* rows within the same commit and
have them persist. The exact event API (`early_row_event` / `row_event` /
`after_flush_row_event`) and whether sibling mutations re-flush vary by LogicBank
version — the registration section documents the intended hook per operation and
is the first place to adjust if the generated project differs.
"""

from __future__ import annotations

import logging

from logic_bank.exec_row_logic.logic_row import LogicRow
from logic_bank.logic_bank import Rule

from database import models

log = logging.getLogger(__name__)

# stop_type ids — must match database/seed_data.sql.
STOP_ORIGIN = 1
STOP_DESTINATION = 2


def _siblings(session, trip_id, exclude_id):
    """All other waypoints on the trip (the row being written is handled separately)."""
    return (session.query(models.Waypoint)
            .filter(models.Waypoint.trip_id == trip_id,
                    models.Waypoint.id != exclude_id)
            .all())


# --- 1. place-on-insert ------------------------------------------------------

def _waypoint_place(row, old_row, logic_row: LogicRow) -> None:
    """Assign the inserted stop's seq server-side (the client sends only an append
    hint). Intermediate stops (fuel/lunch/load/…) are placed just BEFORE the
    destination, shifting it (and anything after) down one; origin/destination — or
    any stop on a trip with no destination yet — append at the end. Deferrable
    unique lets the shift run with transient dup seqs; COMMIT sees a clean 1..N."""
    if not logic_row.is_inserted():
        return
    sibs = _siblings(logic_row.session, row.trip_id, row.id)
    dest = next((s for s in sibs if s.stop_type_id == STOP_DESTINATION), None)

    if row.stop_type_id not in (STOP_ORIGIN, STOP_DESTINATION) and dest is not None:
        target = dest.seq
        for s in sibs:
            if s.seq >= target:
                s.seq += 1
        row.seq = target
    else:
        row.seq = max((s.seq for s in sibs), default=0) + 1


# --- 2. shift-on-move (reorder) ---------------------------------------------

def _waypoint_reorder(row, old_row, logic_row: LogicRow) -> None:
    """Turn a single-row 'move to slot K' PATCH into a correct reorder: when a
    waypoint's seq changes old->new, shift the stops in the crossed range by one so
    the ordering stays dense 1..N. (Only the moved row is PATCHed by the client;
    this opens/closes the slot.)"""
    if not logic_row.is_updated() or old_row is None:
        return
    old_s, new_s = old_row.seq, row.seq
    if new_s == old_s:
        return
    sibs = _siblings(logic_row.session, row.trip_id, row.id)
    if new_s < old_s:                      # moved up the list: [new, old-1] shift +1
        for s in sibs:
            if new_s <= s.seq < old_s:
                s.seq += 1
    else:                                  # moved down: [old+1, new] shift -1
        for s in sibs:
            if old_s < s.seq <= new_s:
                s.seq -= 1


# --- 3. densify-on-delete ----------------------------------------------------

def _waypoint_densify(row, old_row, logic_row: LogicRow) -> None:
    """After a stop is removed, renumber the trip's remaining stops to 1..N so the
    sequence stays gap-free (a delete would otherwise leave 1, 2, 4, …)."""
    if not logic_row.is_deleted():
        return
    sibs = sorted(_siblings(logic_row.session, row.trip_id, row.id),
                  key=lambda s: s.seq)
    for i, s in enumerate(sibs, start=1):
        if s.seq != i:
            s.seq = i


# --- registration ------------------------------------------------------------

def declare_logic() -> None:
    """Registered by ALS logic discovery on server start.

    Hook choice is the version-sensitive part (see module NOTE):
      * place-on-insert must set row.seq BEFORE the row is flushed -> an EARLY
        row event (falls back to row_event if early isn't available);
      * shift-on-move needs old_row.seq -> a row event on update;
      * densify-on-delete acts after the row is gone -> an after-flush event.
    All three mutate sibling rows in the same commit; the deferrable unique
    constraint makes that safe.
    """
    # Defensive registration: if a given event API isn't present in this
    # LogicBank version, skip that one rule with a warning rather than raising
    # (an AttributeError here would abort LogicBank activation for EVERY module
    # and crash-loop the API). Positional on_class matches the working
    # after_flush_row_event call in comms_governance.py.
    def _register(event_name, calling, *, fallback=None):
        hook = getattr(Rule, event_name, None)
        if hook is None and fallback:
            hook = getattr(Rule, fallback, None)
            if hook is not None:
                log.warning("route governance: Rule.%s unavailable — using "
                            "Rule.%s for %s", event_name, fallback, calling.__name__)
        if hook is None:
            log.warning("route governance: no event hook for %s (tried %s%s) — "
                        "waypoint rule skipped", calling.__name__, event_name,
                        f"/{fallback}" if fallback else "")
            return
        hook(models.Waypoint, calling=calling)

    # place-on-insert needs to set the row's own seq before flush (early); fall
    # back to a plain row event if early isn't available in this version.
    _register("early_row_event", _waypoint_place, fallback="row_event")
    _register("row_event", _waypoint_reorder)          # shift-on-move (needs old_row)
    _register("after_flush_row_event", _waypoint_densify)  # densify-on-delete

    log.info("Fleet Dispatcher route governance registered "
             "(waypoint seq: place-on-insert + shift-on-move + densify-on-delete)")

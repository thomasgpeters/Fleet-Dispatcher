"""Fleet Dispatcher — coerce empty-string foreign keys to NULL (auto-discovered).

**Why this exists.** A JSON:API client that omits (or sends `null` for) a nullable
foreign key can reach the ORM as an empty string `''` rather than `None` (safrs
deserialization). LogicBank then loads that parent with `SELECT … WHERE id = ''`,
Postgres rejects `''::uuid` (`invalid input syntax for type uuid: ""`), the
**transaction aborts**, and the write 500s — every later statement just echoes
`InFailedSqlTransaction`.

The first place it bites is posting a normal (non-reply) **Message**:
`message.reply_to_id` is a *self-referential* FK, so LogicBank loads a `message`
parent with an empty key before the row is inserted.

**The fix.** A SQLAlchemy `before_flush` listener scrubs `''` → `None` on every
nullable FK column of the pending (new/dirty) rows. Registered with `insert=True`
so it runs **ahead of** LogicBank's own `before_flush` (which does the
parent-load), and it reads the row's actual state at flush time — so it works no
matter how safrs set the value (an attribute-`set` hook did NOT catch it, which
is why this uses before_flush instead).

Auto-discovered by ApiLogicServer (module under `logic/logic_discovery/`);
listeners register at import. Reinstalled after a regen via `make als-extensions`.
See docs/DEPLOYMENT.md (redeploy troubleshooting).

NOTE (ALS/SQLAlchemy version-sensitive): uses `sqlalchemy.inspect(cls)` +
`mapper.column_attrs` and a class-level `Session` before_flush with `insert=True`;
if your generated models/session differ, adjust.
"""

from __future__ import annotations

import inspect as _pyinspect
import logging

from sqlalchemy import event
from sqlalchemy import inspect as _sa_inspect
from sqlalchemy.orm import Session

from database import models

log = logging.getLogger(__name__)

# class -> [nullable FK column attribute keys], computed once at import.
_FK_KEYS: dict[type, list[str]] = {}


def _build_fk_map() -> int:
    total = 0
    for _name, cls in _pyinspect.getmembers(models, _pyinspect.isclass):
        try:
            mapper = _sa_inspect(cls)
        except Exception:
            continue  # not a mapped class
        if not hasattr(mapper, "column_attrs"):
            continue
        keys: list[str] = []
        for col_attr in mapper.column_attrs:
            col = col_attr.columns[0]
            if col.foreign_keys and col.nullable:
                keys.append(col_attr.key)
        if keys:
            _FK_KEYS[cls] = keys
            total += len(keys)
    return total


def _scrub_obj(obj) -> None:
    keys = _FK_KEYS.get(type(obj))
    if not keys:
        return
    for key in keys:
        if getattr(obj, key, None) == "":
            setattr(obj, key, None)


def _on_transient_to_pending(session, instance):
    """Fires the moment an object is added to the session — before ANY flush, so
    it beats LogicBank's before_flush parent-load regardless of listener order.
    This is the one that actually catches safrs's '' before it can be queried."""
    _scrub_obj(instance)


def _scrub_before_flush(session, flush_context, instances):
    """Backup: scrub pending rows at flush time too (covers values set after add)."""
    for obj in list(session.new) + list(session.dirty):
        _scrub_obj(obj)


_count = _build_fk_map()
# Session-level hooks (belt-and-suspenders; lost the ordering race on their own).
event.listen(Session, "transient_to_pending", _on_transient_to_pending)
event.listen(Session, "before_flush", _scrub_before_flush, insert=True)

# THE fix: wrap LogicBank's own parent-loader so empty-string FKs on the row are
# nulled *immediately before* it loads parents — guaranteed timing, inside
# LogicBank, so it can't lose a listener-order race. safrs materializes the '' at
# flush time (after every pre-flush hook), so this is the only point that reliably
# sees it before `session.query(Parent).get('')` runs and blows up the transaction.
# NOTE (LogicBank version-sensitive): patches
# logic_bank.exec_row_logic.logic_row.LogicRow._load_parents_on_insert — if that
# private name changes, this no-ops with a warning (guarded) and the write still
# fails; revisit against the new LogicBank.
_patched_load = False
_patched_getparent = False
try:
    from logic_bank.exec_row_logic.logic_row import LogicRow

    # (1) Scrub any literal '' FK on the row right before the parent-load.
    _orig_load_parents = LogicRow._load_parents_on_insert

    def _load_parents_scrubbed(self, *args, **kwargs):
        row = self.row
        for key in _FK_KEYS.get(type(row), ()):
            if getattr(row, key, None) == "":
                setattr(row, key, None)
        return _orig_load_parents(self, *args, **kwargs)

    LogicRow._load_parents_on_insert = _load_parents_scrubbed
    _patched_load = True

    # (2) The real fix: skip loading a parent whose FK is null/empty. A null FK
    #     has no parent, so there is nothing to load — this prevents LogicBank
    #     from running `session.query(Parent).get('')` (which errors on ''::uuid
    #     and aborts the txn) for a self-referential relationship like
    #     message.reply_to (derived as '' even when reply_to_id is None).
    _orig_get_parent = LogicRow._get_parent_logic_row

    def _get_parent_guarded(self, parent_role_name, *args, **kwargs):
        try:
            rel = _sa_inspect(type(self.row)).relationships.get(parent_role_name)
            if rel is not None:
                for col in rel.local_columns:
                    if getattr(self.row, col.key, None) in (None, ""):
                        return None  # null/empty FK -> no parent to load
        except Exception:
            pass  # fall through to original on any introspection hiccup
        return _orig_get_parent(self, parent_role_name, *args, **kwargs)

    LogicRow._get_parent_logic_row = _get_parent_guarded
    _patched_getparent = True
except Exception as exc:  # pragma: no cover - defensive
    log.warning("empty-FK: could not wrap LogicRow parent-load methods: %s", exc)

log.info(
    "Fleet Dispatcher: empty-FK->NULL coercion active on %d nullable FK column(s) "
    "across %d model(s) (load_parents_wrapped=%s, get_parent_guarded=%s)",
    _count,
    len(_FK_KEYS),
    _patched_load,
    _patched_getparent,
)


def declare_logic() -> None:
    """ALS calls this on startup. The SQLAlchemy before_flush listener above does
    the work, so there is nothing to register with LogicBank here."""
    return None

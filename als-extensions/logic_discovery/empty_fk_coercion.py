"""Fleet Dispatcher — coerce empty-string foreign keys to NULL (auto-discovered).

A JSON:API client that omits (or sends null for) a nullable foreign key can reach
the ORM as an empty string `''` rather than `None` (safrs deserialization). For a
plain FK that's harmless-ish, but for anything that then gets looked up by that
key it becomes `... WHERE id = ''::uuid` → `invalid input syntax for type uuid` →
the transaction aborts and the write 500s.

The worst offender — the self-referential `message.reply_to_id` — has been fixed
structurally by **dropping that FK constraint** (see database/schema.sql), which
stops ALS generating the self-relationship that triggered a lazy empty-key load.
This module is the lightweight, defense-in-depth complement: it normalizes `''` →
`None` on **nullable FK columns** of pending rows via ordinary SQLAlchemy session
events (no patching of SQLAlchemy internals), so a stray empty FK from any client
is stored as a proper NULL.

Auto-discovered by ApiLogicServer (module under `logic/logic_discovery/`);
listeners register at import. Reinstalled after a regen via `make als-extensions`.

NOTE (SQLAlchemy version-sensitive): uses `sqlalchemy.inspect(cls)` +
`mapper.column_attrs`; adjust if your generated models differ.
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
            continue
        if not hasattr(mapper, "column_attrs"):
            continue
        keys = [ca.key for ca in mapper.column_attrs
                if ca.columns[0].foreign_keys and ca.columns[0].nullable]
        if keys:
            _FK_KEYS[cls] = keys
            total += len(keys)
    return total


def _scrub_obj(obj) -> None:
    for key in _FK_KEYS.get(type(obj), ()):
        if getattr(obj, key, None) == "":
            setattr(obj, key, None)


def _on_transient_to_pending(session, instance):
    _scrub_obj(instance)


def _scrub_before_flush(session, flush_context, instances):
    for obj in list(session.new) + list(session.dirty):
        _scrub_obj(obj)


_count = _build_fk_map()
event.listen(Session, "transient_to_pending", _on_transient_to_pending)
event.listen(Session, "before_flush", _scrub_before_flush, insert=True)
log.info(
    "Fleet Dispatcher: nullable-FK ''->NULL scrub active on %d column(s) across "
    "%d model(s)",
    _count,
    len(_FK_KEYS),
)


def declare_logic() -> None:
    """ALS calls this on startup; the session listeners above do the work."""
    return None

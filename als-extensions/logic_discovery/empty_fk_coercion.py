"""Fleet Dispatcher — coerce empty-string foreign keys to NULL (auto-discovered).

**Why this exists.** A JSON:API client that omits (or even sends `null` for) a
nullable foreign key can reach the ORM as an empty string `''` rather than `None`
(safrs deserialization). LogicBank then tries to load that parent with
`SELECT … WHERE id = ''`, Postgres rejects `''::uuid`
(`invalid input syntax for type uuid: ""`), the **transaction aborts**, and the
write fails with a 500 — every later statement just echoes
`InFailedSqlTransaction`.

The first place it bites is posting a normal (non-reply) **Message**:
`message.reply_to_id` is a *self-referential* FK, so LogicBank loads a `message`
parent with an empty key before the row is even inserted.

**The fix.** Register a SQLAlchemy attribute `set` listener on every **nullable
FK column** across the model that rewrites `''` → `None` at assignment time —
well before flush / parent-load — so the value is a proper SQL NULL. Real FK
values (UUID strings) pass through untouched.

Auto-discovered by ApiLogicServer (module under `logic/logic_discovery/`); the
listeners are registered at import. Reinstalled after a regen via
`make als-extensions`. See docs/DEPLOYMENT.md (redeploy troubleshooting).

NOTE (ALS/SQLAlchemy version-sensitive): uses `sqlalchemy.inspect(cls)` +
`mapper.column_attrs`; if your generated models differ, adjust the sweep.
"""

from __future__ import annotations

import inspect as _pyinspect
import logging

from sqlalchemy import event
from sqlalchemy import inspect as _sa_inspect

from database import models

log = logging.getLogger(__name__)


def _empty_to_none(target, value, oldvalue, initiator):  # SQLAlchemy 'set' hook
    """Return None for an empty string; pass every other value through."""
    return None if value == "" else value


def _register_empty_fk_coercion() -> int:
    """Attach the coercion to every nullable FK column; return the count."""
    registered = 0
    for _name, cls in _pyinspect.getmembers(models, _pyinspect.isclass):
        try:
            mapper = _sa_inspect(cls)
        except Exception:
            continue  # not a mapped class
        if not hasattr(mapper, "column_attrs"):
            continue
        for col_attr in mapper.column_attrs:
            col = col_attr.columns[0]
            if col.foreign_keys and col.nullable:
                event.listen(
                    getattr(cls, col_attr.key), "set", _empty_to_none, retval=True
                )
                registered += 1
    return registered


# Register once, at discovery/import time (before any request flush).
_count = _register_empty_fk_coercion()
log.info(
    "Fleet Dispatcher: empty-FK->NULL coercion registered on %d nullable FK column(s)",
    _count,
)


def declare_logic() -> None:
    """ALS calls this on startup. The SQLAlchemy listeners above do the work, so
    there is nothing to register with LogicBank here."""
    return None

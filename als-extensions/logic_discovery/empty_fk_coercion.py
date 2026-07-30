"""Fleet Dispatcher — neutralize empty-string primary-key lookups (auto-discovered).

**The bug.** Posting a normal (non-reply) Message 500'd. LogicBank, loading the
new row's parents, ends up running `session.query(Message).get('')` for the
self-referential `reply_to` relationship (it derives an empty `''` key even though
`message.reply_to_id` is `None`). Postgres rejects `''::uuid`
(`invalid input syntax for type uuid: ""`), the **transaction aborts**, and the
write fails with a 500 — every later statement just echoes `InFailedSqlTransaction`.

We tried nulling the FK at every pre-flush stage (attribute `set`,
`transient_to_pending`, `before_flush`, and wrapping LogicBank's parent-load).
None worked: at parent-load time the FK is already `None`, and LogicBank derives
the `''` key from its *own* role metadata, under a role name that isn't a
SQLAlchemy relationship key — so a name-based guard can't catch it.

**The fix.** Attack the actual failing call. An **empty-string identity can never
match a UUID (or integer) primary key**, so `session.query(X).get('')` /
`session.get(X, '')` should simply return "not found" (`None`) instead of issuing
a doomed `''::uuid` query. We patch `Query.get` and `Session.get` to short-circuit
empty-string identities. This is safe (no table here uses `''` as a valid PK),
universal, and independent of how the empty key was derived.

We also keep light `''`→`None` FK scrubs on the session as defense-in-depth.

Auto-discovered by ApiLogicServer (module under `logic/logic_discovery/`);
patches apply at import. Reinstalled after a regen via `make als-extensions`.
See docs/DEPLOYMENT.md (redeploy troubleshooting).

NOTE (SQLAlchemy version-sensitive): patches `sqlalchemy.orm.Query.get` and
`sqlalchemy.orm.Session.get`; guarded with try/except so a signature change
no-ops with a warning rather than breaking startup.
"""

from __future__ import annotations

import inspect as _pyinspect
import logging

from sqlalchemy import event
from sqlalchemy import inspect as _sa_inspect
from sqlalchemy.orm import Query, Session

from database import models

log = logging.getLogger(__name__)

# class -> [nullable FK column keys], for the defense-in-depth scrubs.
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


def _ident_has_empty(ident) -> bool:
    """True if a primary-key identity is (or contains) an empty string."""
    if ident == "":
        return True
    if isinstance(ident, (tuple, list)):
        return any(x == "" for x in ident)
    if isinstance(ident, dict):
        return any(v == "" for v in ident.values())
    return False


_count = _build_fk_map()

# Defense-in-depth: scrub '' -> None on nullable FK columns of pending rows.
event.listen(Session, "transient_to_pending", _on_transient_to_pending)
event.listen(Session, "before_flush", _scrub_before_flush, insert=True)

# THE fix: an empty-string identity can't match a UUID/int PK — return None
# ("not found") instead of issuing `... WHERE id = ''::uuid`, which errors and
# aborts the transaction.
_patched = []
try:
    _orig_query_get = Query.get

    def _query_get_safe(self, ident, *args, **kwargs):
        log.warning("FKGET ident=%r type=%s empty=%s",  # TEMP diagnostic
                    ident, type(ident).__name__, _ident_has_empty(ident))
        if _ident_has_empty(ident):
            return None
        return _orig_query_get(self, ident, *args, **kwargs)

    Query.get = _query_get_safe
    _patched.append("Query.get")
except Exception as exc:  # pragma: no cover - defensive
    log.warning("empty-FK: could not patch Query.get: %s", exc)

try:
    _orig_session_get = Session.get

    def _session_get_safe(self, entity, ident, *args, **kwargs):
        if _ident_has_empty(ident):
            return None
        return _orig_session_get(self, entity, ident, *args, **kwargs)

    Session.get = _session_get_safe
    _patched.append("Session.get")
except Exception as exc:  # pragma: no cover - defensive
    log.warning("empty-FK: could not patch Session.get: %s", exc)

log.info(
    "Fleet Dispatcher: empty-PK lookups neutralized (%s); FK scrubs on %d column(s)",
    ", ".join(_patched) or "none",
    _count,
)


def declare_logic() -> None:
    """ALS calls this on startup. The patches above do the work; nothing to
    register with LogicBank here."""
    return None

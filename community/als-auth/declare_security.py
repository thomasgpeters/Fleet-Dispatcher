"""
ApiLogicServer — declarative role grants to pair with the app_user auth provider.

Drop in at:  security/declare_security.py

Dev-permissive baseline: grants every app role READ on every mapped entity, so an
authenticated user can load data. Tighten to per-role row/column grants for
production (e.g. drivers see only their own rows).

NOTE: ALS's server_setup reads a module-level `declare_security_message` string
at startup and logs it — the generated file defines it, so a replacement MUST
define it too, or startup throws AttributeError.

MIT License. Copyright (c) 2026 Thomas G. Peters.
"""

import inspect as _pyinspect
import logging

from sqlalchemy.orm import class_mapper as _class_mapper
from security.system.authorization import Grant
import database.models as models

logger = logging.getLogger(__name__)

# App roles (match your <role>.code values).
ROLES = ("dispatcher", "driver", "updater")

_granted = 0
for _name, _cls in _pyinspect.getmembers(models, _pyinspect.isclass):
    try:
        _class_mapper(_cls)  # mapped entities only
    except Exception:
        continue
    for _role in ROLES:
        Grant(on_entity=_cls, to_role=_role)
    _granted += 1

logger.info("Security: read grants on %d entities for roles %s", _granted, ROLES)

# REQUIRED: ALS server_setup reads + logs this at startup.
declare_security_message = (
    "Security active — app_user JWT auth + read grants (dev-permissive baseline)"
)

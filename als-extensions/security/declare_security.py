"""Fleet Dispatcher — declarative security grants (installed by als-extensions).

Dev-permissive baseline: grants all app roles READ on every mapped entity, so an
authenticated user can load data. Tighten to per-role row/column grants for
production (e.g. drivers see only their own loads/trips). See
docs/AUTHENTICATION.md.

Installed over security/declare_security.py by als-extensions/install.sh.
"""

import inspect as _pyinspect
import logging

from sqlalchemy.orm import class_mapper as _class_mapper
from security.system.authorization import Grant
import database.models as models

logger = logging.getLogger(__name__)

# App roles (match fleet.app_role.code).
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

logger.info("Fleet Dispatcher security: read grants on %d entities for roles %s",
            _granted, ROLES)

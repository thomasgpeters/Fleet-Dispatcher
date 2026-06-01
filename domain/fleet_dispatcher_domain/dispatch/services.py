"""Dispatch domain service.

Coordinates the rules that span more than one aggregate: the weekly load cap
(spans many ``Load`` instances) and equipment/commodity compatibility (spans
``Equipment`` and ``Commodity``).
"""

from __future__ import annotations

from ..fleet.driver import Driver
from ..fleet.equipment import Equipment
from ..shared.enums import LoadStatus
from ..shared.errors import CompatibilityError, SchedulingError
from ..shared.policies import MAX_LOADS_PER_WEEK
from .commodity import Commodity
from .load import Load
from .repositories import LoadRepository


class DispatchService:
    """Assigns loads to drivers/equipment, enforcing cross-aggregate rules."""

    def __init__(self, loads: LoadRepository) -> None:
        self._loads = loads

    def assign(self, load: Load, driver: Driver, equipment: Equipment,
               commodity: Commodity) -> None:
        """Assign ``load`` to ``driver`` on ``equipment``.

        Enforces:
          * the assigned equipment can haul the commodity's category, and
          * the driver holds fewer than ``MAX_LOADS_PER_WEEK`` active loads in
            this dispatch week.
        Then performs the load-local DRAFT->DISPATCHED transition.
        """
        if not equipment.in_service:
            raise SchedulingError(f"Equipment {equipment.unit_number} is out of service")

        if not equipment.can_haul_commodity_category(commodity.category):
            raise CompatibilityError(
                f"{equipment.configuration.describe()} cannot haul "
                f"'{commodity.category}'"
            )

        if load.dispatch_week_id is None:
            raise SchedulingError("Load has no dispatch week")

        existing = self._loads.loads_for_driver_in_week(driver.id, load.dispatch_week_id)
        active = [l for l in existing
                  if l.status != LoadStatus.CANCELLED and l.id != load.id]
        if len(active) >= MAX_LOADS_PER_WEEK:
            raise SchedulingError(
                f"Driver {driver.name} already holds {len(active)} loads this "
                f"week (max {MAX_LOADS_PER_WEEK})"
            )

        load.assign_to(driver.id, equipment.id)

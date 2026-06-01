"""Equipment aggregate root (Fleet context)."""

from __future__ import annotations

from dataclasses import dataclass

from ..shared.enums import PowerUnit, TrailerType
from ..shared.model import AggregateRoot
from ..shared.value_objects import EquipmentConfiguration


@dataclass(eq=False)
class Equipment(AggregateRoot):
    """A truck/trailer rig identified by a unit number, described by its
    :class:`EquipmentConfiguration`."""

    unit_number: str = ""
    configuration: EquipmentConfiguration | None = None
    in_service: bool = True

    @classmethod
    def register(cls, unit_number: str, configuration: EquipmentConfiguration) -> "Equipment":
        return cls(unit_number=unit_number, configuration=configuration, in_service=True)

    def can_haul_commodity_category(self, category: str) -> bool:
        """Whether this rig's configuration is suitable for a commodity category.

        Baseline compatibility matrix; refine as the catalog of commodities and
        rigs grows. ``category`` values come from ``Commodity.category``.
        """
        if self.configuration is None:
            return False
        trailer = self.configuration.trailer
        rules: dict[str, set[TrailerType]] = {
            "vehicles": {TrailerType.CAR_CARRIER, TrailerType.STEP_DECK_52, TrailerType.FLATBED_52},
            "heavy_equipment": {TrailerType.RGN_LOWBOY, TrailerType.STEP_DECK_52},
            "farm_equipment": {TrailerType.RGN_LOWBOY, TrailerType.STEP_DECK_52, TrailerType.FLATBED_52},
            "generators": {TrailerType.FLATBED_52, TrailerType.STEP_DECK_52, TrailerType.RGN_LOWBOY},
            "lifts": {TrailerType.FLATBED_52, TrailerType.STEP_DECK_52, TrailerType.RGN_LOWBOY},
        }
        allowed = rules.get(category)
        # Unknown category: defer to a flatbed/step-deck as a general carrier.
        if allowed is None:
            return trailer in {TrailerType.FLATBED_52, TrailerType.STEP_DECK_52}
        return trailer in allowed

    def take_out_of_service(self) -> None:
        self.in_service = False

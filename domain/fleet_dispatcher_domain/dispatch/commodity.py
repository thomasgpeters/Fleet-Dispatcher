"""Commodity entity (Dispatch context)."""

from __future__ import annotations

from dataclasses import dataclass

from ..shared.model import Entity


@dataclass(eq=False)
class Commodity(Entity):
    """What is being hauled.

    ``category`` is the coarse class used for equipment compatibility checks
    (see ``Equipment.can_haul_commodity_category``): e.g. ``"vehicles"``,
    ``"heavy_equipment"``, ``"farm_equipment"``, ``"generators"``, ``"lifts"``.
    """

    description: str = ""
    category: str = ""
    weight_lbs: int | None = None

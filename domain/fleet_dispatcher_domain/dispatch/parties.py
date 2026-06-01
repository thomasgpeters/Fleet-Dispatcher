"""Shipper and Receiver entities (Dispatch context)."""

from __future__ import annotations

from dataclasses import dataclass

from ..shared.model import Entity
from ..shared.value_objects import Location


@dataclass(eq=False)
class Shipper(Entity):
    """The party releasing freight at the pickup."""

    name: str = ""
    location: Location | None = None
    contact: str = ""


@dataclass(eq=False)
class Receiver(Entity):
    """The party accepting freight at the drop-off."""

    name: str = ""
    location: Location | None = None
    contact: str = ""

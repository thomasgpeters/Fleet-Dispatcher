"""Repository ports for the Fleet context.

These are *abstract* — the concrete adapters (SQLAlchemy / ApiLogicServer) live
in the middleware. The domain depends only on these interfaces.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from uuid import UUID

from .driver import Driver
from .equipment import Equipment


class DriverRepository(ABC):
    @abstractmethod
    def get(self, driver_id: UUID) -> Driver | None: ...

    @abstractmethod
    def add(self, driver: Driver) -> None: ...

    @abstractmethod
    def list_active(self) -> list[Driver]: ...


class EquipmentRepository(ABC):
    @abstractmethod
    def get(self, equipment_id: UUID) -> Equipment | None: ...

    @abstractmethod
    def add(self, equipment: Equipment) -> None: ...

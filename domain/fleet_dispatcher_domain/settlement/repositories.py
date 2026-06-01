"""Repository ports for the Settlement context."""

from __future__ import annotations

from abc import ABC, abstractmethod
from uuid import UUID

from .settlement import Settlement


class SettlementRepository(ABC):
    @abstractmethod
    def get(self, settlement_id: UUID) -> Settlement | None: ...

    @abstractmethod
    def add(self, settlement: Settlement) -> None: ...

    @abstractmethod
    def for_driver(self, driver_id: UUID) -> list[Settlement]: ...

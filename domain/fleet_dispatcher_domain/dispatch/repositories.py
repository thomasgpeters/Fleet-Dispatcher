"""Repository ports for the Dispatch context."""

from __future__ import annotations

from abc import ABC, abstractmethod
from uuid import UUID

from .load import Load


class LoadRepository(ABC):
    @abstractmethod
    def get(self, load_id: UUID) -> Load | None: ...

    @abstractmethod
    def add(self, load: Load) -> None: ...

    @abstractmethod
    def loads_for_driver_in_week(self, driver_id: UUID, dispatch_week_id: UUID) -> list[Load]:
        """All non-cancelled loads a driver holds in a given dispatch week.

        Used by ``DispatchService`` to enforce the weekly load cap.
        """
        ...

"""Driver aggregate root (Fleet context)."""

from __future__ import annotations

from dataclasses import dataclass, field
from uuid import UUID

from ..shared.enums import DriverType
from ..shared.model import AggregateRoot
from ..shared.value_objects import Percentage


@dataclass(eq=False)
class Driver(AggregateRoot):
    """A person who hauls loads, under a Company or Owner-Operator contract.

    The contract percentage is *derived* from ``driver_type`` and is never set
    directly — see :meth:`contract_percentage`.
    """

    name: str = ""
    driver_type: DriverType = DriverType.COMPANY
    phone: str = ""
    email: str = ""
    home_base: str = ""
    active: bool = True
    # Equipment this driver is qualified/assigned to operate (by id).
    equipment_ids: set[UUID] = field(default_factory=set)

    @classmethod
    def hire(cls, name: str, driver_type: DriverType, *, phone: str = "",
             email: str = "", home_base: str = "") -> "Driver":
        """Factory guaranteeing a valid, active new driver."""
        return cls(name=name, driver_type=driver_type, phone=phone,
                   email=email, home_base=home_base, active=True)

    @property
    def contract_percentage(self) -> Percentage:
        """The settlement share for this driver (Company 30%, Owner-Op 70%)."""
        return Percentage.for_driver_type(self.driver_type)

    @property
    def bears_operating_costs(self) -> bool:
        """True when the *driver/owner* bears fuel, tolls, maintenance, repairs.

        Owner-Operators bear their own costs; for Company Drivers the company
        does.
        """
        return self.driver_type == DriverType.OWNER_OPERATOR

    def assign_equipment(self, equipment_id: UUID) -> None:
        self.equipment_ids.add(equipment_id)

    def unassign_equipment(self, equipment_id: UUID) -> None:
        self.equipment_ids.discard(equipment_id)

    def deactivate(self) -> None:
        self.active = False

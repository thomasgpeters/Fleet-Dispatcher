"""Settlement aggregate root (Settlement context)."""

from __future__ import annotations

from dataclasses import dataclass
from uuid import UUID

from ..shared.enums import DriverType
from ..shared.errors import InvariantViolation
from ..shared.model import AggregateRoot
from ..shared.value_objects import Money, Percentage


@dataclass(eq=False)
class Settlement(AggregateRoot):
    """A driver's pay for a single load.

    ``driver_pay`` must always equal ``gross_rate × contract_percentage``; the
    invariant is checked on construction and re-checked after any change. Cost
    responsibility (``costs_borne_by_owner``) is recorded for reporting but is
    not deducted from the split in this baseline.
    """

    driver_id: UUID | None = None
    load_id: UUID | None = None
    gross_rate: Money = None
    contract_percentage: Percentage = None
    driver_pay: Money = None
    costs_borne_by_owner: bool = False

    def __post_init__(self) -> None:
        if self.gross_rate is None or self.contract_percentage is None:
            raise InvariantViolation("Settlement requires a gross rate and percentage")
        expected = self.gross_rate * self.contract_percentage
        if self.driver_pay is None:
            self.driver_pay = expected
        elif self.driver_pay != expected:
            raise InvariantViolation(
                f"driver_pay {self.driver_pay} != rate × % ({expected})"
            )

    @property
    def company_pay(self) -> Money:
        """The remainder retained by the company (gross − driver pay)."""
        return Money(self.gross_rate.amount - self.driver_pay.amount,
                     self.gross_rate.currency)

    @classmethod
    def cost_responsibility(cls, driver_type: DriverType) -> bool:
        """True when the owner/driver bears operating costs."""
        return driver_type == DriverType.OWNER_OPERATOR

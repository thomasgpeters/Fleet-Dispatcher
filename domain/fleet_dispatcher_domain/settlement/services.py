"""Settlement domain service."""

from __future__ import annotations

from ..fleet.driver import Driver
from ..dispatch.load import Load
from .settlement import Settlement


class SettlementCalculator:
    """Computes a :class:`Settlement` from a delivered load and its driver.

    Driver pay = load rate × the driver's contract percentage. The percentage
    comes from the driver's type (Company 30%, Owner-Operator 70%), so the rule
    lives in exactly one place (``Percentage.for_driver_type``).
    """

    def settle(self, load: Load, driver: Driver) -> Settlement:
        return Settlement(
            driver_id=driver.id,
            load_id=load.id,
            gross_rate=load.rate,
            contract_percentage=driver.contract_percentage,
            costs_borne_by_owner=driver.bears_operating_costs,
        )

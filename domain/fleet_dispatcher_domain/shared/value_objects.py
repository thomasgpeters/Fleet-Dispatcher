"""Value objects — immutable, equality-by-value, self-validating.

A value object has no identity: two ``Money(100, "USD")`` are interchangeable.
Each validates its own invariants on construction, so an invalid value object
can never exist.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import date, datetime, time, timedelta
from decimal import Decimal, ROUND_HALF_UP

from .enums import DriverType, PowerUnit, TrailerType
from .errors import InvariantViolation
from .policies import COMPANY_DRIVER_PERCENT, OWNER_OPERATOR_PERCENT

_CENTS = Decimal("0.01")


@dataclass(frozen=True)
class Money:
    """A non-negative monetary amount in a single currency (default USD)."""

    amount: Decimal
    currency: str = "USD"

    def __post_init__(self) -> None:
        # Allow ints/strs/floats at the boundary, normalize to Decimal cents.
        object.__setattr__(self, "amount", Decimal(str(self.amount)).quantize(_CENTS, ROUND_HALF_UP))
        if self.amount < 0:
            raise InvariantViolation(f"Money cannot be negative: {self.amount}")
        if not self.currency:
            raise InvariantViolation("Money requires a currency")

    def __mul__(self, factor: "Percentage | Decimal | int | float") -> "Money":
        if isinstance(factor, Percentage):
            factor = factor.as_fraction
        result = (self.amount * Decimal(str(factor))).quantize(_CENTS, ROUND_HALF_UP)
        return Money(result, self.currency)

    __rmul__ = __mul__

    def __str__(self) -> str:  # pragma: no cover - cosmetic
        return f"{self.amount} {self.currency}"

    @classmethod
    def zero(cls, currency: str = "USD") -> "Money":
        return cls(Decimal("0.00"), currency)


@dataclass(frozen=True)
class Percentage:
    """A percentage in the inclusive range 0–100."""

    value: Decimal

    def __post_init__(self) -> None:
        object.__setattr__(self, "value", Decimal(str(self.value)))
        if not (Decimal(0) <= self.value <= Decimal(100)):
            raise InvariantViolation(f"Percentage must be 0–100: {self.value}")

    @property
    def as_fraction(self) -> Decimal:
        return self.value / Decimal(100)

    @classmethod
    def for_driver_type(cls, driver_type: DriverType) -> "Percentage":
        """The single source of truth for the contract split (30 / 70)."""
        mapping = {
            DriverType.COMPANY: COMPANY_DRIVER_PERCENT,
            DriverType.OWNER_OPERATOR: OWNER_OPERATOR_PERCENT,
        }
        return cls(Decimal(mapping[driver_type]))

    def __str__(self) -> str:  # pragma: no cover - cosmetic
        return f"{self.value}%"


@dataclass(frozen=True)
class Distance:
    """A non-negative distance in miles (dead-head or loaded)."""

    miles: Decimal

    def __post_init__(self) -> None:
        object.__setattr__(self, "miles", Decimal(str(self.miles)))
        if self.miles < 0:
            raise InvariantViolation(f"Distance cannot be negative: {self.miles}")

    def __add__(self, other: "Distance") -> "Distance":
        return Distance(self.miles + other.miles)

    @classmethod
    def zero(cls) -> "Distance":
        return cls(Decimal(0))


@dataclass(frozen=True)
class Address:
    """A postal address used as a pickup or drop-off Location."""

    line1: str
    city: str
    state: str
    postal_code: str
    line2: str = ""
    country: str = "USA"

    def __post_init__(self) -> None:
        for label, val in (("line1", self.line1), ("city", self.city),
                           ("state", self.state), ("postal_code", self.postal_code)):
            if not val or not val.strip():
                raise InvariantViolation(f"Address requires {label}")


# Location is an Address used in a dispatch role; alias keeps the language clear.
Location = Address


@dataclass(frozen=True)
class DateRange:
    """A half-open date/time interval ``[start, end)``."""

    start: datetime
    end: datetime

    def __post_init__(self) -> None:
        if self.end <= self.start:
            raise InvariantViolation("DateRange end must be after start")

    def contains(self, moment: datetime) -> bool:
        return self.start <= moment < self.end

    @classmethod
    def dispatch_week(cls, any_day: date) -> "DateRange":
        """The Monday->Monday dispatch week containing ``any_day``.

        Weeks run Monday 00:00 (inclusive) to the following Monday 00:00
        (exclusive).
        """
        monday = any_day - timedelta(days=any_day.weekday())
        start = datetime.combine(monday, time.min)
        return cls(start, start + timedelta(days=7))


@dataclass(frozen=True)
class EquipmentConfiguration:
    """A valid power-unit / trailer combination with options.

    Encodes which combinations are legitimate rigs (e.g. a RAM with a car
    carrier, a tractor with a 52' step-deck and ramps).
    """

    power_unit: PowerUnit
    trailer: TrailerType
    has_ramps: bool = False
    deck_length_ft: int | None = None
    has_duals: bool = False

    def __post_init__(self) -> None:
        allowed = self._allowed_for(self.power_unit)
        if self.trailer not in allowed:
            raise InvariantViolation(
                f"{self.power_unit.value} cannot pull {self.trailer.value}"
            )
        if self.deck_length_ft is not None and self.deck_length_ft <= 0:
            raise InvariantViolation("deck_length_ft must be positive")

    @staticmethod
    def _allowed_for(power_unit: PowerUnit) -> frozenset[TrailerType]:
        if power_unit == PowerUnit.TRACTOR:
            return frozenset({
                TrailerType.STEP_DECK_52,
                TrailerType.RGN_LOWBOY,
                TrailerType.FLATBED_52,
                TrailerType.NONE,
            })
        # RAM 3500 / 4500 — straight-truck style hauling (e.g. car carrier).
        return frozenset({TrailerType.CAR_CARRIER, TrailerType.FLATBED_52, TrailerType.NONE})

    def describe(self) -> str:
        bits = [self.power_unit.value, self.trailer.value]
        if self.has_ramps:
            bits.append("ramps")
        if self.deck_length_ft:
            bits.append(f"{self.deck_length_ft}ft")
        if self.has_duals:
            bits.append("duals")
        return " + ".join(bits)

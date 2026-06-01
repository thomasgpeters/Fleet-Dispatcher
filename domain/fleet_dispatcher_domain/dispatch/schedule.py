"""DispatchWeek aggregate root (Dispatch context).

A dispatch week runs Monday 00:00 -> the following Monday 00:00. Its identity is
derived from the opening Monday so the same week is referenced consistently.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import date, datetime

from ..shared.model import AggregateRoot
from ..shared.value_objects import DateRange


@dataclass(eq=False)
class DispatchWeek(AggregateRoot):
    """The Monday->Monday scheduling window that loads are booked into."""

    range: DateRange | None = None

    @classmethod
    def containing(cls, any_day: date) -> "DispatchWeek":
        """The dispatch week that contains ``any_day``."""
        return cls(range=DateRange.dispatch_week(any_day))

    @property
    def monday(self) -> datetime:
        assert self.range is not None
        return self.range.start

    def includes(self, moment: datetime) -> bool:
        assert self.range is not None
        return self.range.contains(moment)

    @property
    def label(self) -> str:
        """Human label, e.g. 'Week of 2026-06-01'."""
        return f"Week of {self.monday.date().isoformat()}"

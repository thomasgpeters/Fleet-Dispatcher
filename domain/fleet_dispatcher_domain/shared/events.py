"""Domain events.

A seam for the future: aggregates record these via ``record_event`` and the
application layer can publish them after a successful commit. No event bus is
wired up in this baseline — the types simply make the intent explicit.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime
from uuid import UUID


@dataclass(frozen=True)
class DomainEvent:
    occurred_at: datetime = field(default_factory=datetime.utcnow)


@dataclass(frozen=True)
class LoadDispatched(DomainEvent):
    load_id: UUID = None
    driver_id: UUID = None
    dispatch_week_id: UUID = None


@dataclass(frozen=True)
class LoadDelivered(DomainEvent):
    load_id: UUID = None


@dataclass(frozen=True)
class SettlementCreated(DomainEvent):
    settlement_id: UUID = None
    driver_id: UUID = None
    load_id: UUID = None

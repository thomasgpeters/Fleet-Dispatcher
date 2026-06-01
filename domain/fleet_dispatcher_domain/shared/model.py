"""Base classes for the tactical DDD building blocks.

``Entity``        — has identity and a lifecycle; equality by id.
``AggregateRoot`` — the only entity referenced from outside its aggregate;
                    collects domain events to be published after persistence.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any
from uuid import UUID, uuid4


def new_id() -> UUID:
    """Generate a fresh aggregate/entity identity."""
    return uuid4()


@dataclass(eq=False)
class Entity:
    """An object defined by identity rather than its attributes.

    Two entities are equal iff they share the same type and id, regardless of
    their other attribute values.
    """

    id: UUID = field(default_factory=new_id)

    def __eq__(self, other: Any) -> bool:
        if other.__class__ is not self.__class__:
            return NotImplemented
        return self.id == other.id

    def __hash__(self) -> int:
        return hash((self.__class__, self.id))


@dataclass(eq=False)
class AggregateRoot(Entity):
    """The entry point to an aggregate and its consistency boundary.

    Cross-aggregate references are held by **id**, never by object reference.
    Domain events raised while handling a command are buffered here and drained
    by the application layer once the transaction commits.
    """

    _events: list[Any] = field(default_factory=list, init=False, repr=False)

    def record_event(self, event: Any) -> None:
        self._events.append(event)

    def pull_events(self) -> list[Any]:
        """Return and clear buffered domain events."""
        events, self._events = self._events, []
        return events

"""Shared kernel: DDD building blocks and cross-context value objects."""

from .enums import (
    DriverType,
    LoadStatus,
    PowerUnit,
    Role,
    RunType,
    TrailerType,
)
from .errors import DomainError, InvariantViolation
from .model import AggregateRoot, Entity
from .policies import MAX_LOADS_PER_WEEK, TARGET_LOADS_PER_WEEK
from .value_objects import (
    Address,
    DateRange,
    Distance,
    EquipmentConfiguration,
    Money,
    Percentage,
)

__all__ = [
    # building blocks
    "Entity",
    "AggregateRoot",
    "DomainError",
    "InvariantViolation",
    # value objects
    "Money",
    "Percentage",
    "Distance",
    "Address",
    "DateRange",
    "EquipmentConfiguration",
    # enums
    "DriverType",
    "RunType",
    "PowerUnit",
    "TrailerType",
    "LoadStatus",
    "Role",
    # policies
    "MAX_LOADS_PER_WEEK",
    "TARGET_LOADS_PER_WEEK",
]

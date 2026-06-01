"""Enumerations shared across bounded contexts.

These mirror the enumerations documented in ``docs/domain-model.md`` and the
PostgreSQL ``CREATE TYPE`` definitions in ``database/schema.sql``.
"""

from __future__ import annotations

from enum import Enum


class DriverType(str, Enum):
    """A driver's contract type. Determines the settlement percentage and who
    bears operating costs (fuel, tolls, maintenance, repairs)."""

    COMPANY = "company"            # company driver — 30%; company bears costs
    OWNER_OPERATOR = "owner_operator"  # owner-operator — 70%; owner bears costs


class RunType(str, Enum):
    LONG_HAUL = "long_haul"
    REGIONAL = "regional"


class PowerUnit(str, Enum):
    """The power unit of a rig."""

    TRACTOR = "tractor"        # semi tractor
    RAM_3500 = "ram_3500"      # Dodge RAM 3500 (often duals)
    RAM_4500 = "ram_4500"      # Dodge RAM 4500


class TrailerType(str, Enum):
    """The towed / mounted unit."""

    STEP_DECK_52 = "step_deck_52"   # 52' step-deck (commonly with ramps)
    RGN_LOWBOY = "rgn_lowboy"       # Removable Gooseneck / low-boy
    FLATBED_52 = "flatbed_52"       # 52' flatbed
    CAR_CARRIER = "car_carrier"     # car carrier (e.g. behind a RAM)
    NONE = "none"                   # power unit only


class LoadStatus(str, Enum):
    DRAFT = "draft"
    DISPATCHED = "dispatched"
    IN_TRANSIT = "in_transit"
    DELIVERED = "delivered"
    SETTLED = "settled"
    CANCELLED = "cancelled"


class Role(str, Enum):
    """Application roles."""

    DISPATCHER = "dispatcher"   # desktop portal — receives & dispatches loads
    DRIVER = "driver"           # mobile / ELD — hauls loads
    UPDATER = "updater"         # mobile — driver/dispatcher communication

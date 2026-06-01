"""Domain-level errors.

These are raised when a domain invariant would be violated. They are
intentionally framework-agnostic; the middleware translates them into HTTP /
JSON:API error responses.
"""

from __future__ import annotations


class DomainError(Exception):
    """Base class for all errors originating in the domain layer."""


class InvariantViolation(DomainError):
    """Raised when an operation would leave an aggregate in an invalid state."""


class CompatibilityError(InvariantViolation):
    """Raised when equipment is not compatible with a commodity (or similar)."""


class SchedulingError(InvariantViolation):
    """Raised when a scheduling rule (e.g. weekly load cap) would be broken."""

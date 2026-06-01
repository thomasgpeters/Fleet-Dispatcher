"""Fleet Dispatcher — pure-Python Domain-Driven Design model.

This package is the authoritative, framework-free expression of the Fleet
Dispatcher ubiquitous language. It has **no dependency** on ApiLogicServer,
SQLAlchemy, Flask, or PostgreSQL; it can be unit-tested in isolation.

Bounded contexts:
    * fleet       — drivers and equipment
    * dispatch    — loads, the weekly board, parties and places
    * settlement  — driver pay
    * (identity)  — users and roles live in ``shared.enums``/``shared.model``

See ``docs/domain-model.md`` and ``docs/ddd-patterns.md`` at the repo root.
"""

__all__ = ["shared", "fleet", "dispatch", "settlement"]

__version__ = "0.1.0"

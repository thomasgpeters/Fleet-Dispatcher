"""Load aggregate root (Dispatch context) — the central unit of work."""

from __future__ import annotations

from dataclasses import dataclass
from uuid import UUID

from ..shared.enums import LoadStatus, RunType
from ..shared.errors import InvariantViolation
from ..shared.events import LoadDelivered, LoadDispatched
from ..shared.model import AggregateRoot
from ..shared.value_objects import Distance, Location, Money


@dataclass(eq=False)
class Load(AggregateRoot):
    """A shipment to haul. References other aggregates by id only.

    Lifecycle: DRAFT -> DISPATCHED -> IN_TRANSIT -> DELIVERED -> SETTLED
    (or CANCELLED).
    """

    # Cross-aggregate references (by id, per DDD).
    dispatch_week_id: UUID | None = None
    driver_id: UUID | None = None
    equipment_id: UUID | None = None
    shipper_id: UUID | None = None
    receiver_id: UUID | None = None
    commodity_id: UUID | None = None

    pickup: Location | None = None
    dropoff: Location | None = None
    run_type: RunType = RunType.REGIONAL
    deadhead: Distance = None
    loaded: Distance = None
    rate: Money = None  # post-broker rate
    status: LoadStatus = LoadStatus.DRAFT

    def __post_init__(self) -> None:
        if self.deadhead is None:
            self.deadhead = Distance.zero()
        if self.loaded is None:
            self.loaded = Distance.zero()
        if self.rate is None:
            self.rate = Money.zero()
        self._check_invariants()

    @classmethod
    def draft(cls, *, dispatch_week_id: UUID, shipper_id: UUID, receiver_id: UUID,
              commodity_id: UUID, pickup: Location, dropoff: Location,
              rate: Money, run_type: RunType,
              deadhead: Distance | None = None,
              loaded: Distance | None = None) -> "Load":
        """Create an unassigned DRAFT load in a guaranteed-valid state."""
        return cls(
            dispatch_week_id=dispatch_week_id,
            shipper_id=shipper_id,
            receiver_id=receiver_id,
            commodity_id=commodity_id,
            pickup=pickup,
            dropoff=dropoff,
            rate=rate,
            run_type=run_type,
            deadhead=deadhead or Distance.zero(),
            loaded=loaded or Distance.zero(),
            status=LoadStatus.DRAFT,
        )

    @property
    def total_miles(self) -> Distance:
        return self.deadhead + self.loaded

    def _check_invariants(self) -> None:
        if self.pickup is not None and self.dropoff is not None and self.pickup == self.dropoff:
            raise InvariantViolation("Pickup and drop-off locations must differ")
        # Money/Distance value objects already guarantee non-negativity.

    # --- lifecycle -------------------------------------------------------

    def assign_to(self, driver_id: UUID, equipment_id: UUID) -> None:
        """Attach a driver + equipment and move the load to DISPATCHED.

        Compatibility and weekly-cap checks are the responsibility of
        ``DispatchService`` (they span aggregates); this method enforces only
        the load-local state transition.
        """
        if self.status not in (LoadStatus.DRAFT, LoadStatus.DISPATCHED):
            raise InvariantViolation(f"Cannot assign a {self.status.value} load")
        self.driver_id = driver_id
        self.equipment_id = equipment_id
        self.status = LoadStatus.DISPATCHED
        self.record_event(LoadDispatched(
            load_id=self.id, driver_id=driver_id,
            dispatch_week_id=self.dispatch_week_id,
        ))

    def start_transit(self) -> None:
        self._require(LoadStatus.DISPATCHED)
        self.status = LoadStatus.IN_TRANSIT

    def mark_delivered(self) -> None:
        self._require(LoadStatus.IN_TRANSIT)
        self.status = LoadStatus.DELIVERED
        self.record_event(LoadDelivered(load_id=self.id))

    def mark_settled(self) -> None:
        self._require(LoadStatus.DELIVERED)
        self.status = LoadStatus.SETTLED

    def cancel(self) -> None:
        if self.status in (LoadStatus.SETTLED, LoadStatus.CANCELLED):
            raise InvariantViolation(f"Cannot cancel a {self.status.value} load")
        self.status = LoadStatus.CANCELLED

    def _require(self, expected: LoadStatus) -> None:
        if self.status != expected:
            raise InvariantViolation(
                f"Load must be {expected.value}, but is {self.status.value}"
            )

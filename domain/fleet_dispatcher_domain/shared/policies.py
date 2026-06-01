"""Named domain policy constants.

Keeping these here (rather than as magic numbers) means a business-rule change
is a one-line edit with a single source of truth, mirrored by a LogicBank rule
in the middleware.
"""

from __future__ import annotations

# A dispatch week runs Monday -> Monday. A driver takes up to 3-4 loads/week.
TARGET_LOADS_PER_WEEK: int = 3   # soft target
MAX_LOADS_PER_WEEK: int = 4      # hard cap (enforced)

# Contract percentages by driver type (see Percentage.for_driver_type).
COMPANY_DRIVER_PERCENT: int = 30
OWNER_OPERATOR_PERCENT: int = 70

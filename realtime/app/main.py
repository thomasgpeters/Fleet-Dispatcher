"""Entry point for the realtime WebSocket bridge.

    python -m app.main      # from the realtime/ directory
"""

from __future__ import annotations

import asyncio
import logging

from .bridge import run


def main() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

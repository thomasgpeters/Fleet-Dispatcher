"""Fleet Dispatcher — comms governance (auto-discovered LogicBank logic).

Server-side enforcement for the team-comms model (Telegram-style; see
docs/TODO.md "Feature 4"). Two rules, both on Message insert:

  P1  Broadcast posting lock — in a BROADCAST channel only the owner/admins may
      post; members are read-only.
  P2  Mute / ban — a member whose standing is 'muted' or 'banned' may not post
      while the restriction is active (channel_member.restricted_until NULL =
      indefinite; a past timestamp means the restriction has expired).

Why a server rule (not just client UI): clients hide the composer as UX, but the
authoritative check must live where every write funnels through — LogicBank, so
mobile, desktop, and any API caller are all governed identically.

Install/regen: ApiLogicServer auto-discovers modules under
`logic/logic_discovery/` and calls `declare_logic()`. This file requires the
P1/P3 schema (channel_member.member_role_id / member_status_id / restricted_until;
channel.channel_type_id), so **regenerate ALS from the updated schema first**,
then re-run `als-extensions/install.sh` (or `make als-extensions`).

NOTE (ALS/SQLAlchemy version-sensitive): model class names come from ALS
generation (`channel` -> Channel, `channel_member` -> ChannelMember,
`message` -> Message). The LogicRow.session query API and Rule.constraint
`calling` signature can vary by version — adjust if your generated project
differs. We query with filter_by(...).one_or_none() to stay 1.x/2.0 compatible.
"""

from __future__ import annotations

import logging
from datetime import datetime, timezone

from logic_bank.exec_row_logic.logic_row import LogicRow
from logic_bank.rule_bank.rule_bank import Rule

from database import models

log = logging.getLogger(__name__)

# Lookup ids — must match database/seed_data.sql.
CHANNEL_TYPE_BROADCAST = 3
ROLE_OWNER = 1
ROLE_ADMIN = 3
STATUS_MUTED = 2
STATUS_BANNED = 3


def _restriction_active(member) -> bool:
    """True if the member's mute/ban is currently in force."""
    if member.member_status_id not in (STATUS_MUTED, STATUS_BANNED):
        return False
    until = member.restricted_until
    if until is None:
        return True  # indefinite
    # restricted_until is TIMESTAMPTZ (tz-aware); compare in UTC.
    return until > datetime.now(timezone.utc)


def _can_post(row, old_row, logic_row: LogicRow) -> bool:
    """Constraint: may this message be posted? (True = allowed.)"""
    if not logic_row.is_inserted():
        return True  # edits/reads are governed elsewhere; gate new posts only

    session = logic_row.session
    channel = (
        session.query(models.Channel)
        .filter_by(id=row.channel_id)
        .one_or_none()
    )
    member = (
        session.query(models.ChannelMember)
        .filter_by(channel_id=row.channel_id, user_id=row.author_id)
        .one_or_none()
    )

    # Broadcast: only owner/admins may post.
    if channel is not None and channel.channel_type_id == CHANNEL_TYPE_BROADCAST:
        if member is None or member.member_role_id not in (ROLE_OWNER, ROLE_ADMIN):
            return False

    # Mute / ban: blocked while the restriction is active.
    if member is not None and _restriction_active(member):
        return False

    return True


def declare_logic() -> None:
    """Registered by ALS logic discovery on server start."""
    Rule.constraint(
        validate=models.Message,
        calling=_can_post,
        error_msg="You don't have permission to post in this channel.",
    )
    log.info("Fleet Dispatcher comms governance registered (broadcast lock + mute/ban)")

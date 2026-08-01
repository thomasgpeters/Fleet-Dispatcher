"""Fleet Dispatcher — comms governance (auto-discovered LogicBank logic).

Server-side enforcement for the team-comms model (Telegram-style; see
docs/TODO.md "Feature 4"). Message-posting constraints:

  P1  Broadcast posting lock — in a BROADCAST channel only the owner/admins may
      post; members are read-only.
  P2  Mute / ban — a member whose standing is 'muted' or 'banned' may not post
      while the restriction is active (channel_member.restricted_until NULL =
      indefinite; a past timestamp means the restriction has expired).

Plus the per-member **unread_count** derivation (TODO "Client-calculated values
-> LogicBank", item 1): the unread badge was computed independently in the mobile
board and the desktop CommPanel, and they drifted (a read on the phone didn't
clear the desktop badge). It's now derived once here:
  * a formula recomputes channel_member.unread_count whenever the member row is
    written (mark-as-read stamps last_read_at -> the count recomputes, ~0);
  * an after-flush event bumps every other member's count when a message posts.
Increment-on-post is cheap; recompute-on-read is the self-healing reconcile.

Why a server rule (not just client UI): clients hide the composer as UX, but the
authoritative check must live where every write funnels through — LogicBank, so
mobile, desktop, and any API caller are all governed identically. Same for the
unread count: one authoritative value every client reads, none recompute.

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
from logic_bank.logic_bank import Rule  # Rule lives here (matches ALS declare_logic.py)

from database import models

log = logging.getLogger(__name__)

# Lookup ids — must match database/seed_data.sql.
CHANNEL_TYPE_BROADCAST = 3
ROLE_OWNER = 1
ROLE_ADMIN = 3
STATUS_MUTED = 2
STATUS_BANNED = 3
APP_ROLE_DISPATCHER = 1


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


# --- unread_count derivation ------------------------------------------------

def _count_unread(session, channel_id, user_id, last_read_at) -> int:
    """Authoritative unread count for one member: the channel's messages newer
    than last_read_at that the member didn't author. NULL last_read_at = the
    member has never opened the channel, so everything counts."""
    q = (session.query(models.Message)
         .filter(models.Message.channel_id == channel_id,
                 models.Message.author_id != user_id))
    if last_read_at is not None:
        q = q.filter(models.Message.posted_at > last_read_at)
    return q.count()


def _member_unread_count(row, old_row, logic_row: LogicRow):
    """Formula: recompute unread_count from this member's own last_read_at.

    Fires whenever the channel_member row is written — most importantly on
    mark-as-read (a PATCH to last_read_at), which recomputes the count to ~0.
    This is the self-healing reconcile that keeps the increment path honest."""
    return _count_unread(logic_row.session, row.channel_id, row.user_id,
                         row.last_read_at)


def _bump_unread_on_message(row, old_row, logic_row: LogicRow) -> None:
    """After a message is inserted, bump unread_count for every OTHER member of
    the channel who hasn't read past it. The formula above only re-fires when a
    member row itself changes; a new message doesn't touch those rows, so this
    event carries the increment. (Cheap +1 per member; the formula reconciles the
    exact value on the reader's next mark-as-read.)"""
    if not logic_row.is_inserted():
        return
    session = logic_row.session
    members = (session.query(models.ChannelMember)
               .filter(models.ChannelMember.channel_id == row.channel_id,
                       models.ChannelMember.user_id != row.author_id)
               .all())
    for m in members:
        if m.last_read_at is None or m.last_read_at < row.posted_at:
            m.unread_count = (m.unread_count or 0) + 1


def _can_create_topic(row, old_row, logic_row: LogicRow) -> bool:
    """Constraint: only admins/dispatchers may create channel topics.

    Allowed when the creator is the channel owner/admin, OR has the dispatcher
    app-role. Regular members (drivers/updaters) don't create their own topics.
    """
    if not logic_row.is_inserted():
        return True

    session = logic_row.session
    user = (
        session.query(models.AppUser)
        .filter_by(id=row.created_by)
        .one_or_none()
    )
    if user is not None and user.app_role_id == APP_ROLE_DISPATCHER:
        return True

    member = (
        session.query(models.ChannelMember)
        .filter_by(channel_id=row.channel_id, user_id=row.created_by)
        .one_or_none()
    )
    return member is not None and member.member_role_id in (ROLE_OWNER, ROLE_ADMIN)


def declare_logic() -> None:
    """Registered by ALS logic discovery on server start."""
    Rule.constraint(
        validate=models.Message,
        calling=_can_post,
        error_msg="You don't have permission to post in this channel.",
    )
    Rule.constraint(
        validate=models.ChannelTopic,
        calling=_can_create_topic,
        error_msg="Only admins and dispatchers can create topics.",
    )

    # unread_count: derived once, read by every client (no more client compute).
    Rule.formula(derive=models.ChannelMember.unread_count,
                 calling=_member_unread_count)
    Rule.after_flush_row_event(models.Message, calling=_bump_unread_on_message)

    log.info(
        "Fleet Dispatcher comms governance registered "
        "(broadcast lock + mute/ban + topic-create + unread_count)"
    )

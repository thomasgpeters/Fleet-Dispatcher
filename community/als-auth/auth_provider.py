"""
ApiLogicServer — SQL auth provider that authenticates against your REAL domain
user table (with werkzeug password hashes), instead of the default separate
auth store.

Drop in at:  security/authentication_provider/sql/auth_provider.py
(after `ApiLogicServer add-auth --provider-type=sql`).

Adapt to your schema: this example uses an `AppUser` model (table `app_user`)
with columns `username`, `full_name`, `email`, `active`, `password_hash`,
`app_role_id`, and an `AppRole` lookup with a `code`. Rename to match yours.

Two things worth knowing (both learned the hard way):
  * Newer ALS validates the login by calling get_user(id, password) and treating
    a None return as failure — so we verify the werkzeug hash INSIDE get_user.
  * ALS reuses get_user for BOTH login (2nd arg = password str) AND JWT identity
    resolution on every authenticated request (2nd arg = the JWT claims DICT).
    Guard the password check with isinstance(password, str), or you'll call
    check_password_hash(hash, <dict>) -> dict.encode() -> 500 on the first
    authenticated call after a successful login.

MIT License. Copyright (c) 2026 Thomas G. Peters. Shared as a pattern for the
ApiLogicServer community.
"""

from security.authentication_provider.abstract_authentication_provider import Abstract_Authentication_Provider
from flask import Flask
import safrs
from safrs.errors import JsonapiError
from dotmap import DotMap
from http import HTTPStatus
import logging
from werkzeug.security import check_password_hash

db = None
session = None
logger = logging.getLogger(__name__)


class ALSError(JsonapiError):
    def __init__(self, message, status_code=HTTPStatus.BAD_REQUEST):
        super().__init__()
        self.message = message
        self.status_code = status_code


class DotMapX(DotMap):
    """DotMap with a hash-aware check_password (kept for callers that use it)."""
    def check_password(self, password=None):
        return bool(self.password_hash) and check_password_hash(self.password_hash, password)


class Authentication_Provider(Abstract_Authentication_Provider):

    @staticmethod
    def configure_auth(flask_app: Flask):
        return

    @staticmethod
    def get_user(id: str, password: str = "") -> object:
        """Look up the login in your user table (username = login id).

        Returns None on any failure (unknown user / bad password) so ALS reports
        a clean auth failure rather than a 500. When `password` is a real login
        string we verify it here; when it's a JWT claims dict (identity
        resolution on later requests) we skip the check and just resolve the user.
        """
        from database import models  # lazy: avoid import cycles at module load
        global db, session
        if db is None:
            db = safrs.DB
            session = db.session
        user = session.query(models.AppUser).filter(models.AppUser.username == id).one_or_none()
        if user is None:
            logger.info("auth: no user for '%s'", id)
            return None  # generic failure (don't reveal whether the user exists)
        if not user.active:
            logger.info("auth: inactive account '%s'", id)
            raise ALSError("This account is no longer active.", HTTPStatus.FORBIDDEN)
        # Login: 2nd arg is the password (str). JWT verify: it's the claims (dict).
        if isinstance(password, str) and password and not (
                user.password_hash and check_password_hash(user.password_hash, password)):
            logger.info("auth: bad password for '%s'", id)
            return None  # generic failure (same message as unknown user)

        rtn_user = DotMapX()
        rtn_user.id = user.username           # JWT identity / login id
        rtn_user.name = user.full_name
        rtn_user.email = user.email
        rtn_user.password_hash = user.password_hash
        rtn_user.UserRoleList = []
        role = session.get(models.AppRole, user.app_role_id)
        if role is not None:
            ur = DotMapX()
            ur.user_id = user.username
            ur.role_name = role.code
            ur.name = role.code
            rtn_user.UserRoleList.append(ur)
        return rtn_user

    @staticmethod
    def check_password(user: object, password: str = "") -> bool:
        if user is None or not getattr(user, "password_hash", None):
            return False
        return check_password_hash(user.password_hash, password)

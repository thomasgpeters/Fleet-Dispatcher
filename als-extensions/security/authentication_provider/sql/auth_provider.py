"""Fleet Dispatcher — SQL auth provider (installed by als-extensions).

Overrides ALS's default sql provider so authentication runs against our
`app_user` table (fleet DB) and verifies werkzeug pbkdf2 password hashes, instead
of the default separate sqlite auth DB with plaintext passwords.

Installed over security/authentication_provider/sql/auth_provider.py after
`ApiLogicServer add-auth` by `als-extensions/install.sh` (run on every fresh
build). See docs/AUTHENTICATION.md.
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
        """Look up the login in fleet.app_user (username = login id).

        Returns None on any failure (unknown user / inactive / bad password) so
        ALS reports a clean auth failure rather than a 500. When `password` is
        supplied (login) we verify it HERE — newer ALS validates via get_user and
        treats None as "incorrect username or password". An empty password means
        identity resolution from a JWT on later requests, so we skip the check
        then. `check_password` below is kept for ALS versions that call it.
        """
        from database import models  # lazy: avoid import cycles at module load
        global db, session
        if db is None:
            db = safrs.DB
            session = db.session
        user = session.query(models.AppUser).filter(models.AppUser.username == id).one_or_none()
        if user is None:
            logger.info("auth: no app_user for '%s'", id)
            return None  # generic failure (don't reveal whether the user exists)
        if not user.active:
            logger.info("auth: inactive account '%s'", id)
            raise ALSError(
                "This account is no longer active. Contact your dispatcher.",
                HTTPStatus.FORBIDDEN)
        if password and not (user.password_hash and
                             check_password_hash(user.password_hash, password)):
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
            ur.role_name = role.code          # dispatcher | driver | updater
            ur.name = role.code
            rtn_user.UserRoleList.append(ur)
        return rtn_user

    @staticmethod
    def check_password(user: object, password: str = "") -> bool:
        if user is None or not getattr(user, "password_hash", None):
            return False
        return check_password_hash(user.password_hash, password)

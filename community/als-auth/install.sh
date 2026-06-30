#!/usr/bin/env bash
#
# Reinstall custom ApiLogicServer security over a freshly generated project.
#
# Why: a fresh `ApiLogicServer create` regenerates (overwrites) the project,
# including security/. Keep your customizations in source control and re-apply
# them with one command after every (re)generate — generated + custom, reconciled.
#
# Usage:
#   ./install.sh /path/to/your-als-project
#
# Run AFTER:  ApiLogicServer add-auth --provider-type=sql
# (add-auth creates security/, which this script then overwrites.)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ALS_PROJECT="${1:?usage: install.sh /path/to/als-project}"

if [[ ! -d "$ALS_PROJECT/security" ]]; then
    echo "error: $ALS_PROJECT/security not found — run 'ApiLogicServer add-auth --provider-type=sql' first." >&2
    exit 1
fi

cp -v "$HERE/auth_provider.py" \
      "$ALS_PROJECT/security/authentication_provider/sql/auth_provider.py"
cp -v "$HERE/declare_security.py" \
      "$ALS_PROJECT/security/declare_security.py"

echo
echo "Installed. Run ALS with security ON (it defaults OFF) + a real secret:"
echo "  export SECURITY_ENABLED=true"
echo "  export SECRET_KEY=\$(openssl rand -hex 32)"
echo "  python api_logic_server_run.py 0.0.0.0 5656"

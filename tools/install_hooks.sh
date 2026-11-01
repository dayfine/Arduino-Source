#!/bin/sh
# install_hooks.sh: set up the local code-health gate for every worktree of this clone.
#   sh <(git show origin/agents:tools/install_hooks.sh)
# Creates the code-health venv (~/.venvs/code-health) and installs a pre-push stub in the clone's
# shared hooks folder. The stub runs tools/hooks/pre-push from origin/agents, so updates need no reinstall.
set -e
V=$HOME/.venvs/code-health
if [ ! -x "$V/bin/python" ]; then
    PY=$(command -v python3.12 || command -v python3.11 || command -v python3)
    "$PY" -m venv "$V"
fi
git show origin/main:devtools/code_health/requirements.txt > "${TMPDIR:-/tmp}/code-health-req.txt"
"$V/bin/pip" install -q -r "${TMPDIR:-/tmp}/code-health-req.txt"
H=$(git rev-parse --path-format=absolute --git-common-dir)/hooks
mkdir -p "$H"
cat > "$H/pre-push" <<'STUB'
#!/bin/sh
# Stub from tools/install_hooks.sh (agents branch): run the shared pre-push gate.
s=$(git show origin/agents:tools/hooks/pre-push 2>/dev/null) || { echo "pre-push: cannot read origin/agents:tools/hooks/pre-push; run git fetch origin agents" >&2; exit 1; }
printf '%s\n' "$s" > "${TMPDIR:-/tmp}/pa-pre-push.$$"
sh "${TMPDIR:-/tmp}/pa-pre-push.$$" "$@"; rc=$?
rm -f "${TMPDIR:-/tmp}/pa-pre-push.$$"
exit $rc
STUB
chmod +x "$H/pre-push"
echo "installed: $H/pre-push (runs origin/agents:tools/hooks/pre-push), venv $V"

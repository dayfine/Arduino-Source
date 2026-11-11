#!/bin/sh
# harness: reusable
# wait_for.sh -- block until ONE condition holds, then exit.
#
# Meant to run under the harness's background mode (Bash run_in_background),
# so the main session gets one turn back when the condition fires instead of
# paying a full-context re-read for every foreground poll. Only the main
# session waits; subagents push, report and stop (AGENTS.md "Dispatching
# subagents", .agents/rules/long-chain-ownership.md).
#
# Usage:
#   wait_for.sh [opts] log <file> <ERE>      a line matching <ERE> is in <file>
#                                            (e.g. log <run.log> 'Tests Finished')
#   wait_for.sh [opts] checks <PR>           every check run on <PR>'s head has
#                                            completed; failed count printed
#   wait_for.sh [opts] reviews <PR> <N>      <PR> has at least <N> reviews
#   wait_for.sh [opts] proc <ERE>            no live process's args match <ERE>
#
# Options:
#   --max-minutes N   give up after N minutes (default 30)
#   --max-seconds N   same, in seconds (overrides --max-minutes)
#   --interval S      seconds between checks (default 30)
#
# Exit: 0 the condition holds; 2 timed out; 1 usage error; 3 no GitHub backend
# (checks/reviews only, checked at startup). Every fired or
# timed-out exit prints one line with the UTC wall-clock time and the elapsed
# seconds, so a resumed session can tell "fired late" from "never fired".
# `checks` firing does not mean green: read fail= in its line.
#
# `proc` ignores zombie (Z) processes: a killed process can leave defunct
# entries whose names still match the pattern.
#
# The PR conditions use `gh api` (REST), not `gh pr checks` / `gh pr view`,
# which go through GraphQL and are refused in Claude cloud sessions.
#
# Backend for checks/reviews: `gh` when it is on PATH; otherwise curl against
# https://api.github.com with $GH_TOKEN, parsed with jq. If neither is usable
# (no gh and no GH_TOKEN, or no curl or jq), checks/reviews exit 3 immediately
# with a message on stderr -- they never wait silently on a backend that
# cannot answer.
#
# Seams (tests): WAIT_FOR_GH (default gh), WAIT_FOR_CURL (default curl), WAIT_FOR_PS (default
# 'ps -eo stat=,args='), WAIT_FOR_REPO (default dayfine/Arduino-Source).

set -u

GH="${WAIT_FOR_GH:-gh}"
CURL="${WAIT_FOR_CURL:-curl}"
EXIT_NO_BACKEND=3
backend=""
PS_CMD="${WAIT_FOR_PS:-ps -eo stat=,args=}"
REPO="${WAIT_FOR_REPO:-dayfine/Arduino-Source}"
SECONDS_PER_MINUTE=60
DEFAULT_MAX_MINUTES=30
DEFAULT_INTERVAL=30

max_seconds=$((DEFAULT_MAX_MINUTES * SECONDS_PER_MINUTE))
interval=$DEFAULT_INTERVAL
start=$(date +%s)

_usage() {
  echo "usage: wait_for.sh [--max-minutes N|--max-seconds N] [--interval S] log <file> <ERE> | checks <PR> | reviews <PR> <N> | proc <ERE>" >&2
  exit 1
}

_is_uint() {
  case "$1" in '' | *[!0-9]*) return 1 ;; *) return 0 ;; esac
}

_report() {
  # $1 = outcome word, $2 = detail.
  now=$(date +%s)
  echo "wait_for: $1 at $(date -u +%Y-%m-%dT%H:%M:%SZ) after $((now - start))s -- $2"
}

_cond_log() {
  [ -f "$1" ] && grep -Eq -- "$2" "$1"
}

# _api <rest-path> <jq-filter>: print the filtered REST response using the
# selected backend; non-zero when the call or the parse fails.
_api() {
  if [ "$backend" = gh ]; then
    $GH api "$1" --jq "$2" 2>/dev/null
  else
    body=$($CURL -fsSL -H "Authorization: Bearer ${GH_TOKEN:-}" \
      -H "Accept: application/vnd.github+json" \
      "https://api.github.com/$1" 2>/dev/null) || return 1
    printf "%s" "$body" | jq -r "$2" 2>/dev/null
  fi
}

# Pick the backend for checks/reviews, or exit 3.
_select_backend() {
  if command -v "$GH" >/dev/null 2>&1; then
    backend=gh
  elif [ -n "${GH_TOKEN:-}" ] && command -v "$CURL" >/dev/null 2>&1 &&
    command -v jq >/dev/null 2>&1; then
    backend=curl
  else
    echo "wait_for: no GitHub backend for '$kind': need gh on PATH, or GH_TOKEN with curl and jq" >&2
    exit "$EXIT_NO_BACKEND"
  fi
}

# Check runs on the PR head (REST, so it also works where GraphQL is blocked);
# fires when there is at least one and every one has completed. Detail names
# the failed count so the caller knows which way it went.
_cond_checks() {
  sha=$(_api "repos/$REPO/pulls/$1" .head.sha) || return 1
  [ -n "$sha" ] || return 1
  runs=$(_api "repos/$REPO/commits/$sha/check-runs?per_page=100" \
    '.check_runs[] | "\(.status) \(.conclusion)"') || return 1
  [ -n "$runs" ] || return 1
  if printf '%s\n' "$runs" | grep -qv '^completed '; then return 1; fi
  fails=$(printf '%s\n' "$runs" |
    grep -cE ' (failure|timed_out|cancelled|action_required|startup_failure)$' ||
    true)
  detail="all checks completed on PR $1 at $sha (fail=$fails)"
  return 0
}

_cond_reviews() {
  n=$(_api "repos/$REPO/pulls/$1/reviews?per_page=100" length) || return 1
  _is_uint "$n" && [ "$n" -ge "$2" ]
}

# Live (non-zombie) processes whose args match the ERE; fires when none is left.
_cond_proc() {
  # Snapshot first: a filter in the same pipeline as `ps` is itself a live
  # process whose args hold the pattern, so it would always match.
  snap=$($PS_CMD 2>/dev/null) || true
  live=$(printf '%s\n' "$snap" | awk '$1 !~ /^Z/' | grep -E -- "$1" |
    grep -v 'wait_for' || true)
  [ -z "$live" ]
}

while [ $# -gt 0 ]; do
  case "$1" in
    --max-minutes)
      _is_uint "${2:-}" || _usage
      max_seconds=$(($2 * SECONDS_PER_MINUTE))
      shift 2
      ;;
    --max-seconds)
      _is_uint "${2:-}" || _usage
      max_seconds=$2
      shift 2
      ;;
    --interval)
      _is_uint "${2:-}" || _usage
      interval=$2
      shift 2
      ;;
    *) break ;;
  esac
done

kind="${1:-}"
[ -n "$kind" ] && shift
case "$kind" in
  log) [ $# -eq 2 ] || _usage; detail="'$2' in $1" ;;
  checks) [ $# -eq 1 ] && _is_uint "$1" || _usage; detail="checks on PR $1" ;;
  reviews)
    [ $# -eq 2 ] && _is_uint "$1" && _is_uint "$2" || _usage
    detail="PR $1 has >= $2 reviews"
    ;;
  proc) [ $# -eq 1 ] || _usage; detail="no live process matches '$1'" ;;
  *) _usage ;;
esac
case "$kind" in checks | reviews) _select_backend ;; esac

while :; do
  if "_cond_$kind" "$@"; then
    _report fired "$detail"
    exit 0
  fi
  if [ $(($(date +%s) - start)) -ge "$max_seconds" ]; then
    _report timeout "$detail (limit ${max_seconds}s)"
    exit 2
  fi
  sleep "$interval"
done

#!/usr/bin/env bash
# Launch this checkout as a named remote station (or provision it for Codex).
#
# usage: launch_remote_station.sh <role> [options]
#   role: win | linux | mac | main
#   --machine <suffix>   append a machine token to the session name (mandatory
#                        when the same role exists on more than one machine)
#   --read-only          provision with authority read-only-proof
#   --continue           reattach to the last remote-control session here
#   --no-server          write .remote-station.json only (Codex-orchestrated
#                        use); prints the lease-wrapped launch to run instead
#
# The station contract this implements lives in
# .agents/skills/bpr-implement-rfc/references/remote-routing.md.

set -euo pipefail

usage() { sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'; }

[ $# -ge 1 ] || { usage >&2; exit 2; }
ROLE=$1; shift

case "$ROLE" in
  win)   BRANCH=windows-lane ;;
  linux) BRANCH=linux-lane ;;
  mac)   BRANCH=mac-lane ;;
  main)  BRANCH=main ;;
  *) echo "unknown role: $ROLE (expected win, linux, mac, or main)" >&2; exit 2 ;;
esac

MACHINE=""
AUTHORITY=lane-commit
CONTINUE=""
NO_SERVER=0
while [ $# -gt 0 ]; do
  case "$1" in
    --machine) MACHINE=${2:?--machine needs a value}; shift 2 ;;
    --read-only) AUTHORITY=read-only-proof; shift ;;
    --continue|-c) CONTINUE="--continue"; shift ;;
    --no-server) NO_SERVER=1; shift ;;
    *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

ROOT=$(git rev-parse --show-toplevel 2>/dev/null) || { echo "not inside a git checkout" >&2; exit 2; }
ORIGIN=$(git -C "$ROOT" remote get-url origin 2>/dev/null || true)
case "$ORIGIN" in
  *BetterPullReview*) ;;
  *) echo "this checkout's origin ($ORIGIN) is not the BetterPullReview repository" >&2; exit 2 ;;
esac

CURRENT=$(git -C "$ROOT" branch --show-current)
[ "$CURRENT" = "$BRANCH" ] || {
  echo "checkout is on '$CURRENT' but role '$ROLE' requires '$BRANCH'" >&2
  echo "check out the right station worktree instead of switching branches here" >&2
  exit 2
}

case "$(uname -s)" in
  Darwin) PLATFORM=mac; ENVIRONMENT=macos ;;
  Linux)
    if grep -qi microsoft /proc/version 2>/dev/null; then
      PLATFORM=linux; ENVIRONMENT=wsl2
    else
      PLATFORM=linux; ENVIRONMENT=linux-native
    fi ;;
  MINGW*|MSYS*) PLATFORM=win; ENVIRONMENT=windows-native ;;
  *) echo "unsupported platform: $(uname -s)" >&2; exit 2 ;;
esac

if [ "$ROLE" != "main" ] && [ "$ROLE" != "$PLATFORM" ]; then
  echo "role '$ROLE' cannot run on a $ENVIRONMENT host (WSL2 is a Linux station, native Windows a Windows station)" >&2
  exit 2
fi

NAME="bpr-$ROLE${MACHINE:+-$MACHINE}"
[ "$ROLE" = "main" ] && NAME="bpr-main-$PLATFORM${MACHINE:+-$MACHINE}"
STATION_FILE="$ROOT/.remote-station.json"

# Station ids are durable: keep the existing one across relaunches.
STATION_ID=""
if [ -f "$STATION_FILE" ]; then
  STATION_ID=$(sed -n 's/.*"station_id"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$STATION_FILE" | head -1)
fi
if [ -z "$STATION_ID" ]; then
  STATION_ID="stn-$(od -An -N4 -tx4 /dev/urandom | tr -d ' \n')"
fi

cat > "$STATION_FILE" <<EOF
{
  "schema": "betterpullreview.implement-rfc.remote-station.v1",
  "station_id": "$STATION_ID",
  "station": "$NAME",
  "project": "betterpullreview",
  "role": "$ROLE",
  "environment": "$ENVIRONMENT",
  "branch": "$BRANCH",
  "authority": "$AUTHORITY"
}
EOF

LEASE="$ROOT/.agents/skills/bpr-implement-rfc/scripts/station_lease.py"
LEASE_ROLE=station-writer
[ "$ROLE" = "main" ] && LEASE_ROLE=project-integrator

if [ "$NO_SERVER" = 1 ]; then
  echo "station file written: $STATION_FILE ($NAME, $ENVIRONMENT, $AUTHORITY)"
  echo "launch the Codex session under the writer lease from $ROOT:"
  echo "  python3 $LEASE run --role $LEASE_ROLE --station $NAME -- codex"
  exit 0
fi

cd "$ROOT"
exec python3 "$LEASE" run --role "$LEASE_ROLE" --station "$NAME" -- \
  claude remote-control --name "$NAME" --spawn session $CONTINUE

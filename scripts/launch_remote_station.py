#!/usr/bin/env python3
"""Launch, inspect or take over this checkout's remote station.

Forwards to the remote-station skill's entry script so the documented command
keeps working from the repository root:

    python3 scripts/launch_remote_station.py [options]   # macOS, Linux, WSL2
    py -3 scripts/launch_remote_station.py [options]     # native Windows
    python3 scripts/launch_remote_station.py status --json
    python3 scripts/launch_remote_station.py takeover --json
    python3 scripts/launch_remote_station.py hooks status

The skill owns every behavior, including takeover's default
`--mode local --stop-previous`. This file only binds the command to this
checkout with `--repo-root` and runs the skill in this process with the same
arguments, stdio and exit status. Workstreams and their branches are declared
in .mask/skill-config/remote-station/station.json; the station contract lives
in .agents/skills/remote-station/references/station-contract.md.
"""

from __future__ import annotations

import pathlib
import runpy
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
TARGET = ROOT / ".agents" / "skills" / "remote-station" / "scripts" / "launch_remote_station.py"


def main(argv=None):
    arguments = list(sys.argv[1:] if argv is None else argv)
    if not TARGET.is_file():
        print(f"launch_remote_station: {ROOT} has no remote-station skill at "
              ".agents/skills/remote-station; run: mask install remote-station", file=sys.stderr)
        return 4
    if not any(arg == "--repo-root" or arg.startswith("--repo-root=") for arg in arguments):
        arguments += ["--repo-root", str(ROOT)]
    sys.argv = [str(TARGET), *arguments]
    try:
        runpy.run_path(str(TARGET), run_name="__main__")
    except SystemExit as exit_request:
        return exit_request.code
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Launch this checkout as a named remote station.

Forwards to the portable launcher in the implement-rfc skill so the documented
command keeps working from the repository root:

    python3 scripts/launch_remote_station.py [options]   # macOS, Linux, WSL2
    py -3 scripts/launch_remote_station.py [options]     # native Windows
    python3 scripts/launch_remote_station.py status --json
    python3 scripts/launch_remote_station.py takeover --json

The workstream and any machine suffix are inferred from the station file or
the checkout's branch; pass them only to be explicit. `--fresh-station` forgets
this checkout's recorded station and starts over under the workstream's
canonical name. Workstreams and their branches are declared in
.mask/skill-config/implement-rfc/project.json; the station contract lives in
.agents/skills/implement-rfc/references/remote-routing.md.
"""

from __future__ import annotations

import pathlib
import runpy
import sys

TARGET = (
    pathlib.Path(__file__).resolve().parents[1]
    / ".agents"
    / "skills"
    / "implement-rfc"
    / "scripts"
    / "launch_remote_station.py"
)

def main(argv=None):
    arguments = list(sys.argv[1:] if argv is None else argv)
    sys.path.insert(0, str(TARGET.parent))
    if arguments and arguments[0] in ("status", "takeover"):
        if not any(arg == "--repo-root" or arg.startswith("--repo-root=") for arg in arguments):
            arguments += ["--repo-root", str(pathlib.Path(__file__).resolve().parents[1])]
        if arguments[0] == "takeover":
            if not any(arg == "--mode" or arg.startswith("--mode=") for arg in arguments):
                arguments += ["--mode", "local"]
            if "--stop-previous" not in arguments:
                arguments += ["--stop-previous"]
        helper = runpy.run_path(str(TARGET.with_name("station_control.py")))
        return helper["main"](arguments)
    if "--help" in arguments or "--help-all" in arguments:
        print("Station management: status [--json] [--observe-runtime], takeover [--json] [--confirm-runtime-stopped].")
        print("Station hooks have been removed. Takeover is only for transferring ownership; it may stop the previous writer.")
    sys.argv = [str(TARGET), *arguments]
    runpy.run_path(str(TARGET), run_name="__main__")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Launch this checkout as a named remote station.

Forwards to the portable launcher in the implement-rfc skill so the documented
command keeps working from the repository root:

    python3 scripts/launch_remote_station.py [options]   # macOS, Linux, WSL2
    py -3 scripts/launch_remote_station.py [options]     # native Windows

The workstream is inferred from the station file or the checkout's branch;
pass one only to be explicit. Workstreams and their branches are declared in
.agents/skills/implement-rfc/config/project.json; the station contract lives in
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

if __name__ == "__main__":
    sys.argv[0] = str(TARGET)
    runpy.run_path(str(TARGET), run_name="__main__")

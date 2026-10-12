---
schema: implement-rfc.card.v1
id: BPR-1386
title: Remove obstructive station hooks and expose management in the launcher
status: "Done"
rfcs: []
depends_on: []
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"739411f14","evidence":{"kind":"observation","detail":"WSL station_hooks preview found exactly 3 station guard commands; removal returned changed true, subsequent config hooks object is empty. No station/MASK internal state mutation or skill rollout occurred; the existing transferring lifecycle is retained."}},{"criterion":"AC2","outcome":"pass","revision":"739411f14","evidence":{"kind":"command","detail":"WSL python3 scripts/launch_remote_station.py --help and status --json exit 0 through existing pinned helper. Status reports actual transferring mode, free lease and unknown runtime; no live takeover attempted. Two disposable wrapper checks pass: default root/mode/stop arguments, explicit flags and ordinary-launch forwarding."}},{"criterion":"AC3","outcome":"pass","revision":"739411f14","evidence":{"kind":"command","detail":"WSL python3 scripts/verify_documentation.py --root . returns DOCUMENTATION_OK; git diff --check clean. Existing helper successfully loads; no runtime-stop or host-reload observation is claimed. Owner must reload project hooks if their current host retains cached configuration."}}]
---

# Remove obstructive station hooks and expose management in the launcher

Owner mandate (2026-10-11): work directly; station hooks blocked MASK recovery in
this WSL consumer. Remove automatic station hooks and expose existing status and
takeover through `scripts/launch_remote_station.py`.

Owned scope: `.codex/hooks.json`, project station wrapper and directly relevant
station documentation. Shared policy/helpers belong to AgentSkills ASK-0105.
No skill upgrade, live station stop/takeover, permissions, global host settings,
push or product code. Removing hooks neither stops nor reconciles runtime writers.
Baseline: b67d720dd72323e126d9b1db0073fde2bfb257a4.

Acceptance:
- AC1: Project hook configuration contains no implement-rfc station commands;
  unrelated hooks/settings survive. No station or MASK internal state is edited.
- AC2: The public project launcher exposes status and takeover using this
  consumer root and existing installed helper, without requiring hidden paths or
  manually spelling root/mode/stop options. Ordinary launch arguments still
  forward unchanged; no implicit runtime stop or launch happens during help/status.
- AC3: Direct WSL help/status and hook inspection plus focused wrapper fixtures
  and the documentation gate pass. Installed helper limitations are reported
  truthfully; no runtime-stop success is inferred from a fixture or lease metadata.

Execution: direct primary; no agents. Preserve unrelated product work.

---
schema: "implement-rfc.card.v1"
id: "BPR-0787"
title: "Qualify the CLI connector on target-native platforms"
status: "Backlog"
rfcs: ["RFC-BPR-0015"]
depends_on: ["BPR-0786"]
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
---

# Qualify the CLI connector on target-native platforms

- [ ] Mac tests-disabled read and fixture write show the selected installed CLI, actual account warning, process cleanup and terminal behavior.
- [ ] Redirect, Unix socket/config, CA, account-switch, executable and descendant/PID risks are probed and described without a false containment claim.
- [ ] Windows and Linux remain explicitly unavailable until each platform has its own native credential, process, read/write and UI evidence.
- [ ] Documentation and verification retain actual versions, limitations and safe-test boundaries without credentials or private repository content.

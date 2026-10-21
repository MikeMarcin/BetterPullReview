---
schema: "implement-rfc.card.v1"
id: "BPR-1694"
title: "Prepare and qualify the coordinated v0.3.2 release"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate":"release-v032-owner-request-20261021","baseline":"111235987772e808f4fae26383925759985b3041","execution_strategy":"direct","external_state":"/Users/jaeger/Documents/Codex/2026-10-21/release-v032"}
---

# Prepare and qualify v0.3.2

The owner requested `$publish-release v0.3.2 if there are fixes to be made make us an implementer`.
This card owns source preparation, reproduced blocker repairs and exact-candidate
qualification for the existing full macOS universal, Windows x64 and Ubuntu
26.04 WSL producer route. Broad changes since v0.3.1 require comprehensive
native Release and Debug. Release preparation supplies no inferred legal,
security, native installation or trust acceptance.

## Acceptance

- AC1: Prepare grounded user-facing notes and version 0.3.2; commit one clean common candidate and preserve unrelated work.
- AC2: Reconcile native jobs before dispatch; qualify the exact committed candidate with comprehensive native Release and Debug, retaining failures and configured skips. Repair reproduced blockers in explicit committed slices without weakening gates.
- AC3: Prepare and audit complete binary, corresponding-source, SPDX, checksum, provenance and native-signature sets through the existing release tools and approved producer configuration. Keep required human/security/legal decisions explicit and use short attended review or wizard handoffs when needed.
- AC4: Publish only a qualified, authorized exact candidate through the existing publisher; verify remote state and downloaded bytes/native trust. Preserve unfinished obligations and external continuation state if prerequisites prevent publication.

## Initial reconciliation

Clean main at the baseline above. CMake still selects 0.3.1 and no v0.3.2
notes exist. Early composition inventory and documentation checks passed in
the assessing chat; neither is native/artifact qualification. BPR-1653 is Done
under its separately approved 240 MiB RSS and 216 MiB live-heap criteria;
v0.3.1's exception is not extended. BPR-1633's foreground native-menu course,
BPR-1371's resource investigation and older BPR-1551/BPR-1404 qualification
frontiers require current evidence reconciliation, not an assumption of a
current product failure. External phase ledger preserves request timing,
producer observations and subsequent results.

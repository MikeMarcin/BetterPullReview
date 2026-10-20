---
schema: "implement-rfc.card.v1"
id: "BPR-1663"
title: "Publish v0.3.1 and shorten the release procedure"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate":"Owner requested publication of v0.3.1 and a faster procedure; confirmed full macOS, Windows and Ubuntu 26.04 packages","baseline":"5aff89be6"}
---

# BPR-1663: Publish v0.3.1 and shorten the release procedure

## Acceptance

- Prepare one clean committed 0.3.1 source with accurate public release notes.
- Run native normal gates, package and sign the three selected platforms, audit actual runtime/source/SPDX and final native trust, retaining failures and skips.
- Publish through the existing guarded public publisher only after selected artifacts and required dispositions are ready; verify downloaded bytes, source binding, native trust and release/tag state.
- Record measured time and safe procedure improvements, reusing unchanged evidence and producer caches without weakening gates.

The owner explicitly extended the existing Ubuntu RSS-growth exception to
v0.3.1 on 2026-10-19, requesting an implementer pursue its repair after publication.
Only the unchanged 216 MiB process RSS predicate is excepted; its result stays failed. RFC-BPR-0038's
remaining native/security/source obligations remain explicit.

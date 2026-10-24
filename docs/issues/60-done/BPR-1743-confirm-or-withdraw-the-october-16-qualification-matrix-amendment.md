---
schema: "implement-rfc.card.v1"
id: "BPR-1743"
title: "Confirm or withdraw the 2026-10-16 qualification-matrix amendment"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P2"
platforms: ["win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1742--BPR-1743--04a7cd1de032.json","found_by":"RFC-0038 security-audit port, 2026-10-22","found_at":"4e318b625","related_cards":["BPR-0960","BPR-0961","BPR-0962","BPR-0973"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e73261468ac38fa9bafbf7d378cd61f752d8ff8a","evidence":{"kind":"observation","detail":"The owner answered the one question in the coordinating session on 2026-10-23 UTC, relayed verbatim: 'Windows 11 and ubuntu wsl2 are tested. If it works on other things is unproven, but not disallowed'. The amendment stands. Recorded in docs/issues/_checkpoints/RFC-BPR-0038/owner-matrix-confirmation-20261023.json beside the unchanged 2026-10-16 record. No attended-review session was opened; the answer came directly from the owner."}},{"criterion":"AC2","outcome":"pass","revision":"e73261468ac38fa9bafbf7d378cd61f752d8ff8a","evidence":{"kind":"artifact","detail":"RFC-BPR-0038 matrix and 2026-10-16 amendment section, BPR-0960 (AC1 environment), BPR-0961 (amendment section, blocker label), BPR-0962 (pending-decision bullet), BPR-0973 (tested environments), the package guide and its maintainer reference, the corporate security overview, the board README and the RFC-0038 ledger name Windows 11 and Ubuntu 26.04 WSL2/WSLg on JaegAW as tested and other environments as untested but not refused. README says 'Tested on Windows 11 and Ubuntu 26.04 (WSL2). Other Windows and Linux versions may work but are untested.' The RFC-0014 plan gets dated amendment r5 (amendments/r5/amendment.json); milestone-plan.json and MILESTONES.md are unchanged. Shipped release notes are unchanged."}},{"criterion":"AC3","outcome":"pass","revision":"e73261468ac38fa9bafbf7d378cd61f752d8ff8a","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . prints DOCUMENTATION_OK (exit 0); python3 tests/scripts/verify_documentation_tests.py runs 67 tests, OK. Both run with Python 3.14.7 on the closing tree."}}]
---

# BPR-1743: Confirm or withdraw the 2026-10-16 qualification-matrix amendment

## Finding

The [owner matrix record](../_checkpoints/RFC-BPR-0038/owner-matrix-amendment-20261016.json)
says the owner amended RFC-BPR-0038 on 2026-10-16:

- Windows qualification moves to JaegAW Windows 11, and Windows 10 is
  removed.
- Linux qualification moves to JaegAW Ubuntu 26.04 under WSL2/WSLg, and
  Linux claims are limited to that environment.

That amendment stayed on the unmerged branch `codex/rfc0038-security-audit-20261016`.
Main still names Windows 10/11 and a clean Ubuntu desktop in RFC-BPR-0038,
BPR-0960, BPR-0961 and the RFC-0014 plan's m8 exit criteria. README offers
the v0.3.1 MSI for "Windows 10/11", and the package guide lists Windows
10/11 x64. The record does not quote the owner's words. The 0.3.x releases
shipped afterwards without the amendment.

## Acceptance

- AC1: The owner answers one Accept/Reject question in a short attended
  session: does the 2026-10-16 matrix amendment stand?
- AC2: If it stands, RFC-BPR-0038's matrix, BPR-0960, BPR-0961, BPR-0973,
  the package guide and the security overview agree with it. The RFC-0014
  plan changes only through its amendment process. README's support claim
  then either matches the qualified matrix or says plainly what is
  unqualified. If it is withdrawn, the ledger records that and the record
  stays as history.
- AC3: The documentation gate passes.

## Boundary

Wording and scope only. No platform is newly qualified by this card.

## Outcome

The owner answered on 2026-10-23 UTC, in the coordinating session:

> Windows 11 and ubuntu wsl2 are tested. If it works on other things is
> unproven, but not disallowed

The amendment stands. The tested Windows and Linux environments are Windows 11
x64 and Ubuntu 26.04 x86_64 under WSL2/WSLg, both on JaegAW. macOS is
unchanged. Windows 10, other Linux distributions and native Linux desktops are
unproven but not disallowed. Nothing refuses them and nothing claims them. The
[confirmation record](../_checkpoints/RFC-BPR-0038/owner-matrix-confirmation-20261023.json)
sits beside the 2026-10-16 record, which stays byte-identical.

AC1 asked for a short attended session. None was opened: the owner gave the
answer directly, which settles the one question it would have asked.

## Verification

- AC1: the owner's words above, recorded in the confirmation record.
- AC2: [RFC-BPR-0038](../../rfcs/RFC-BPR-0038-native-package-release-security-qualification.md#owner-amendment-2026-10-16--tested-windows-and-linux-environments),
  [BPR-0960](../50-blocked/BPR-0960-qualify-windows-x64-package.md),
  [BPR-0961](../50-blocked/BPR-0961-qualify-linux-x64-packages.md),
  [BPR-0962](../50-blocked/BPR-0962-verify-release-trust-and-security-boundaries.md),
  [BPR-0973](../10-backlog/BPR-0973-close-rfc-0038.md), the
  [package guide](../../development/release-packages.md), its
  [maintainer reference](../../development/release-packaging-maintainers.md), the
  [corporate security overview](../../security/corporate-security-overview.md),
  the [board README](../README.md), the [ledger](../../verification/RFC-BPR-0038.md)
  and [README](../../../README.md) agree. The RFC-0014 plan takes dated
  [amendment r5](../../rfcs/plans/rfc-bpr-0014-release-readiness/amendments/r5/amendment.json)
  instead of a rewrite of its M8 exit. Shipped release notes keep their wording.
- AC3: the documentation gate prints `DOCUMENTATION_OK`; its 67 tests pass.

No environment is newly qualified. The M8/M9 native, vault, crash-privacy and
vulnerability gaps stay open. BPR-0958 to BPR-0962 stay Blocked, BPR-0973 stays
Backlog, and all nine RFC-BPR-0038 criteria remain open.

Left as written: `packaging/package_runtime.py` records `osFloor: "Windows 10"`
for the MSI, which matches "not disallowed". `packaging/README.md`, the
`publish-release` skill's Windows reference and dated build, verification and
handoff records still say Windows 10/11 as history or build-host notes.

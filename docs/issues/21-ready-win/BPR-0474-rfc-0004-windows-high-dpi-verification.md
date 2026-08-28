---
id: BPR-0474
title: Run RFC-BPR-0004 Windows x64 target-native high-DPI verification
status: Ready
rfc: RFC-BPR-0004
depends_on: [BPR-0053]
---

# Run RFC-BPR-0004 Windows x64 target-native high-DPI verification

On the designated real Windows 10/11 x64 host, run the target-native high-DPI
application and rendering procedure for the retained diff-workbench surface:
supported Windows scaling must render attached readable ribbons and panes
with no platform-specific geometry or dependency failure, through the real
application at the scales the RFC-BPR-0004 ledger's platform plan names.

Host state first, recorded 2026-08-28: the `C:\code\git\bpr-windows`
worktree's administrative registration was removed by a repository-wide
`git worktree prune` run from the WSL2 side (the entry was `prunable` from
Linux because its Windows path does not resolve there). The worktree's
files and the `windows-lane` branch at `17c8950d` are intact and no commit
was lost, but git commands inside that directory fail with `not a git
repository` until repaired. Before any work on this card, run
`git worktree repair` from Windows git — inside `C:\code\git\bpr-windows`
or from `C:\code\git\BetterPullReview` — and then delete this paragraph.
Do not run the repair from WSL2: it would write `/mnt/c/...` paths into
the registration and re-break it for Windows.

This card was split out on 2026-08-28 when [[BPR-0053]] closed with the
RFC-BPR-0003 closeout: BPR-0053's procedure section
([Run RFC-BPR-0003 Windows x64 target-native verification](../60-done/BPR-0053-rfc-0003-windows-verification.md#run-rfc-bpr-0003-windows-x64-target-native-verification))
remains the anchored host/toolchain/matrix procedure this card inherits, and
the RFC-BPR-0004 high-DPI closeout row in [[BPR-0083]] now blocks on this
card instead of the closed one. The complete Windows matrix-10/11 record
already runs the diff suites at 100% scale; what this card owns is the
explicit high-DPI (1.25x/2.0x) rendered verification of the retained diff
surface on the real machine, which no existing row restates.

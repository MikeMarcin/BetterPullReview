---
id: BPR-0474
title: Run RFC-BPR-0004 Windows x64 target-native high-DPI verification
status: Done
rfc: RFC-BPR-0004
depends_on: [BPR-0053]
---

# Run RFC-BPR-0004 Windows x64 target-native high-DPI verification

On the designated real Windows 10/11 x64 host, run the target-native high-DPI
application and rendering procedure for the retained diff-workbench surface:
supported Windows scaling must render attached readable ribbons and panes
with no platform-specific geometry or dependency failure, through the real
application at the scales the RFC-BPR-0004 ledger's platform plan names.

Host state, resolved 2026-08-28: a repository-wide `git worktree prune`
from the WSL2 side had deleted the `C:\code\git\bpr-windows` administrative
registration outright (the entry was `prunable` from Linux because its
Windows path does not resolve there). The Windows-side session recreated
the admin files by hand, ran `git worktree repair C:/code/git/bpr-windows`,
rebuilt the index with a plain `git reset` touching no working file, and
verified a clean `windows-lane` worktree before this card's run. No commit
was lost. Worktree repair on this shared checkout belongs to Windows git
only; a WSL2-side repair would write `/mnt/c/...` paths and re-break it.

This card was split out on 2026-08-28 when [[BPR-0053]] closed with the
RFC-BPR-0003 closeout: BPR-0053's procedure section
([Run RFC-BPR-0003 Windows x64 target-native verification](../60-done/BPR-0053-rfc-0003-windows-verification.md#run-rfc-bpr-0003-windows-x64-target-native-verification))
remains the anchored host/toolchain/matrix procedure this card inherits, and
the RFC-BPR-0004 high-DPI closeout row in [[BPR-0083]] now blocks on this
card instead of the closed one. The complete Windows matrix-10/11 record
already runs the diff suites at 100% scale; what this card owns is the
explicit high-DPI (1.25x/2.0x) rendered verification of the retained diff
surface on the real machine, which no existing row restates.

## Run on the designated host — 2026-08-28

The course ran on the designated host (Windows 11 Home 10.0.26200 x64,
single 4K 3840x2160 panel, recommended 150%) with the `windows-dev` preset
(Visual Studio 18 2026, x64 Debug) and Qt 6.10.3 msvc2022_64 from the
project CPM cache, in the repaired `bpr-windows` worktree fast-forwarded
`17c8950d -> 7160c283` (ff-only; the incremental build was green and
`better_pull_review.exe` did not relink — linked 2026-08-27 — so no Smart
App Control reputation stall arose). Scaling was real per-monitor DPI
override, the same mechanism as the Settings slider, verified from a fresh
process by virtualized desktop bounds: 125% -> 3072x1728, 200% ->
1920x1080. No `QT_SCALE_FACTOR` or other synthetic scale was set anywhere.
The run was executed by the Windows-side session under this integrator
session's direction, with no product-code, tracker, commit, or push writes;
both `C:\code\git\bpr-windows` and `C:\code\git\BetterPullReview` finished
clean.

**Verdict: the acceptance criterion is satisfied at real 1.25x and 2.0x
through the real application.** Per scale, native `windows` QPA, shipped
`better_pull_review.exe`, github-com fixture with `--diff-workbench-probe`:

- **125% and 200%, `QT_STYLE_OVERRIDE=fusion`**: `PUBLIC_PATH_OK`,
  `DIFF_WORKBENCH_OK`, `DIFF_INTERACTION_EVIDENCE_OK`,
  `CONVERSATION_ANCHOR_EVIDENCE_OK`, `RENDER_OK`, with 19 rendered frames
  plus a 19-line `interaction-trace.jsonl` at each scale.
- **125% and 200%, default `QWindows11Style`**: the full-window render
  passes (`RENDER_OK` at both scales when run without the interaction
  probe) and the pane body paints the applied profile exactly — base
  `#0d1117` at 83.3%/83.8% of the unchanged-code crop with readable
  `#e6edf3` text — but the probe's viewport-grab palette-identity check
  fails `DIFF_INTERACTION_FAILURE:appearance-raster-palette`. Controls
  prove this is style paint polish, not a high-DPI or geometry failure: the
  same marker fails at 150% and at 1.0x offscreen with
  `QT_STYLE_OVERRIDE=windows11` (no desktop at all), while native
  `windowsvista` passes the full course at 125% and offscreen default
  (QFusionStyle) passes. [[BPR-0475]] now owns that divergence under
  RFC-BPR-0005's presentation-polish scope.
- **Component executables natively with no style override and no forced
  scale**, `BPR_EXPECTED_QT_SCALE` asserting `devicePixelRatioF()` against
  the live display: 125% -> `qt_diff_workbench` pass (1350x875) and
  `qt_diff_link_map` pass (1550x650); 200% -> both pass (2160x1400,
  2480x1040), dimensions identical to the macOS BPR-0080 record. App
  rasters scale exactly: the 1280x1000-logical course window grabs
  1600x1250 at 125%, 1920x1500 at 150%, and 2560x2000 at 200%.
- Direct inspection of the 125%/200% fusion renders and the 200% link-map
  raster: continuous filled ribbons meeting the changed-line backgrounds at
  both pane edges, deletion attached left and insertion right,
  selected-line highlight in both panes, populated detail pane, no
  clipping, seams, geometry failure, or dependency failure (the only DLL
  note is the expected BPR-0430 Qt-bin PATH prepend, as the harness itself
  performs).

Artifacts are on the host only, gitignored and not checked in, under
`C:\code\git\bpr-windows\build\logs\bpr-0474\` — `EVIDENCE.md` plus
per-scale `default-style`, `fusion`, and `components` directories, the
150% sanity and diagnostic runs, and a stdout log for every run. The
display was restored to the recommended 150% and verified
(fresh-process bounds 2560x1440, per-monitor DPI override cleared).

Like closed [[BPR-0053]] before it, this card's work completed and the card
stayed open only until [[BPR-0083]]'s closing slice consumed this record —
the 2026-08-28 close that moved both cards to `60-done/` and flipped
RFC-BPR-0004 to Auto-Verified.

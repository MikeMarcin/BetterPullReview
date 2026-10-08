---
schema: "implement-rfc.card.v1"
id: "BPR-1324"
title: "Windows: the title row's owner returns at a narrower width, and the whole name no longer fits at 1280, after the fourth modality"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win, BPR-1293's Windows station run, 2026-10-07","reservation":"docs/issues/_reservations/BPR-1324--BPR-1325--7765882b27a6.json","related_cards":["BPR-1293","BPR-1251","BPR-1129","BPR-0653"],"caused_by":"7778886c9"}
---

# BPR-1324: Windows: the title row's owner returns at a narrower width, and the whole name no longer fits at 1280, after the fourth modality

## Observation

[[BPR-1293]]'s implementation commit `7778886c9` added Conversations as a
fourth modality to the title row. On Windows, `qt_workbench_window_scale_100`,
`_125` and `_200` fail on both the dev and the optimized trees. macOS and Linux
pass.

1. **The owner returns.** The narrowing test stops at "the owner yields once and
   does not return at a narrower width". A probe gives the same widths at every
   scale:

   ```
   owner-gone width=1104 compact=0 text=[terminal  #20607]
   owner-returned width=1099 compact=1 title=0 text=[microsoft/terminal  #20607] minW=902
   ```

   The owner yields at 1104 px beside the whole modality. At 1099 px the
   modality drops its shortcuts (`available < 1'100`), and the width that frees
   brings the owner back. [[BPR-1251]] fixed the same failure for the title by
   deciding it from the room beside the whole modality; the owner had no such
   guard.

2. **The whole name no longer fits at 1280.** Past the first failure, the
   review-bar test stops at "at 1280 the title row names the pull request whole
   …": the name reads `EpicGames/UnrealEngine  #14366 Hair strand lighting fi…`,
   about 80 px short. On Windows the title row also holds the menu control and
   the caption buttons ([[BPR-0653]]). The four modalities with their shortcuts
   now take the room the whole name needs.

Bisected in a separate worktree: both pass at `5d5de1c02`, the parent of
`7778886c9`, which already holds every other source change since
`d8f4a67b7`; both fail at `7778886c9`.

## Outcome

- The owner is decided as the title is: it also needs the room it would have
  beside the whole modality. That room only shrinks as the window narrows, so
  the owner yields once and never returns. The yield order stays title, owner,
  then the modality's shortcuts.
- On Windows only, the modality drops its shortcut hints below 1300 px instead
  of 1100 px, so the whole name fits at 1280 as it does on every platform. This
  is the coordinator's decision of 2026-10-08 (Accept, over relaxing the 1280
  check on Windows). Between 1100 and 1299 px on Windows the segments show their
  names without "Ctrl+N". The shortcuts still work, and the View menu lists
  them.

## Acceptance

- `qt_workbench_window_scale_100`, `_125` and `_200` pass on Windows, in the dev
  and the optimized trees.
- The title-row and composed rows that read the title row still pass on Windows.
- macOS and Linux are unchanged: the threshold change is Windows-only, and the
  owner rule cannot show text there that the old rule hid.

## Implementation (bpr-main-win, 2026-10-08)

**What changed** (`src/adapters/qt`):

- **The owner rule.** `update_strip_identity` shows `owner/repository #N` only
  when it fits the current room *and* the room beside the whole modality
  (`IdentityTitleBudget`, which [[BPR-1251]] already used for the title). That
  room only shrinks as the window narrows, so the owner yields once and never
  returns. The yield order is title, owner, then the modality's shortcuts.
- **The Windows threshold.** `apply_strip_width` drops the modality's shortcut
  hints below 1300 px on Windows (`_WIN32`) and below 1100 px elsewhere, as
  before. The whole name fits at 1280 on Windows again. This is the
  coordinator's Accept of 2026-10-08.
- **Every segment's tooltip names its key.** Overview "(Ctrl+1)", Review
  "(Ctrl+2)" after its first sentence, and Discuss "(Ctrl+3)", matching
  Conversations' "(Ctrl+4, Ctrl+J)", on all platforms. A compact segment
  therefore still offers its key. The accessible description is the same text.
  This was the coordinator's addition after the Accept.
- `docs/ux/workbench-specification.md` records the tooltips and both
  thresholds.

The same commit pins five Windows rows that other cards registered uncontained
without a pin. Each was measured contained for the purpose:

- `application_overview_summary_1`, `_1_25` and `_2`, and
  `application_change_arrows_across_files` ([[BPR-1328]]), stop at "FAILED:
  private composed course root";
- `application_published_file_switch_contract` ([[BPR-1326]]) refuses startup
  at the registry.

All five are pinned `dos-volume-name`, so `test_network_isolation_manifest`
passes on Windows.

**Evidence** (bpr-main-win, Windows 11, MSVC 19.51, Qt 6.11.2, offscreen, at
`e3163bbbe` plus this change). Both trees rebuilt with no `warning C` line.

- `build/windows-dev-tour-cli` (Debug, `ctest -j4`): 25 of 26 pass. These are:
  - `qt_workbench_window_scale_100`, `_125` and `_200` (177, 184 and 199 s);
  - `qt_workbench_title_row` ×3, `qt_workbench_focus_order` ×3,
    `qt_workbench_strip_menu`;
  - `qt_composed_github` ×3, `qt_composed_inline_drawer` ×3,
    `qt_conversation_finder` ×3;
  - the three `application_overview_summary` rows (uncontained);
  - `application_review_round_trip_composition_contract`,
    `architecture_boundaries` and `test_network_isolation_structure_contract`.

  The one failure was the manifest, on the two rows pinned above. With those
  pins it passes, together with both rows (4 of 4).
- `build/bpr0967-release` (MSVC `/O2`, tests on, `ctest -j1`):
  `qt_workbench_window` ×3 and `qt_workbench_title_row` ×3 pass, 6 of 6.

**Failing-before controls.** Each was built and run in the bisect worktree,
on the full change minus one part, `qt_workbench_window_scale_100`:

| Control | Result |
|---|---|
| None (the full change) | passes, 26.9 s |
| No Windows threshold | fails at "at 1280 the title row names the pull request whole …" |
| No key in Overview's tooltip | fails at "each modality segment's tooltip names its key" |
| No owner rule | **passes** |

With the 1300 px threshold, the modality goes compact before the owner would
yield, so nothing on Windows exercises the owner rule today. Its
failing-before evidence is at 1100 px:

- `7778886c9` without the rule fails at "the owner yields once and does not
  return at a narrower width";
- the same tree with the rule passes that check and fails only later, at
  1280.

The rule is kept as the invariant's guard at 1100 px off Windows and against
future width changes.

**Limits.**

- macOS and Linux were not run here. The threshold is `_WIN32`-only. The owner
  rule can only hide the owner where the old rule showed it and the whole
  modality would not fit it: the order the design names.
- The tooltip text changes on every platform.
- `application_windows_title_row`, which shows a real window, waits for the
  owner's desktop clearance. It is not run.

**On the merged tree** (`5964dd6f1`, after rebasing over 13 upstream commits that
changed `src/adapters/qt`, `workbench_window.cpp` among them). Both trees
rebuilt with no `warning C` line.

- `qt_workbench_window` ×3 (185 to 206 s in Debug), `qt_workbench_title_row`
  ×3, focus order ×3, strip menu, composed GitHub ×3, inline drawer ×3,
  finder ×3, the overview summary rows, the round-trip pins,
  `architecture_boundaries` and the structure contract all pass.
- The optimized tree passes 6 of 6.
- The manifest named two newer unpinned rows:
  `application_discuss_top_public_path` ([[BPR-1337]]) and
  `application_accepted_file_in_overview` ([[BPR-1339]]). Measured contained,
  they stop at "FAILED: private discuss-top course root" and "FAILED: private
  composed course root". Both are now pinned `dos-volume-name`, and the
  manifest and both rows pass 4 of 4.

**After a second rebase** (onto BPR-1338's title-row order and the modality
renamed from Conversations to Threads). The conflicts in `workbench_strip.cpp`,
the window test and the spec were resolved by keeping upstream's names with
this change's tooltip keys.

The first rebuild failed on a second C4458, BPR-1336's local `data` in
`appearance_settings_panel::code_zoom()`, so that run's rows came from stale
binaries and were discarded. `47f6d5882` renames that local.

On the rebuilt tree (`47f6d5882`), with no `warning C` line in either tree:

- every row above passes again in the dev tree, `qt_workbench_window` ×3
  among them (184 to 210 s);
- the optimized tree passes 6 of 6;
- the manifest passes once four more rows other cards left unpinned are pinned
  (measured contained, `dos-volume-name`).

The threshold control repeated on this tree, the change at `47f6d5882` with
the Windows threshold back at 1100 px: it still fails at "at 1280 the title
row names the pull request whole…". The shorter "Threads" label does not make
the 1300 px threshold unnecessary. The full change passes there (42.1 s).

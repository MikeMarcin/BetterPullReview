---
schema: "implement-rfc.card.v1"
id: "BPR-1324"
title: "Windows: the title row's owner returns at a narrower width, and the whole name no longer fits at 1280, after the fourth modality"
status: "Ready"
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

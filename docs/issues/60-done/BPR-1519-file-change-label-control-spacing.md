---
schema: "implement-rfc.card.v1"
id: "BPR-1519"
title: "Bring File and Change labels closer to their controls"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"The controls for file and change are far away from their label's text. Probably need to shift those labels right","dispatch":"Dedicated BPR-1519 implementer /root/bpr1519 completed local rendered and combined public verification","milestone_plan":{"id":"bpr-1519-navigation-label-spacing","path":"docs/rfcs/plans/bpr-1519-navigation-label-spacing/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0c23b902aa98ea4f1106a117ac6177f89f8cf263","evidence":{"kind":"observation","detail":"Actual QLabel raster text ends8.8–10logical pixels before its first arrow at1280/900; old left alignment measures95–96and fails the12pixel bound. Full-window renders inspected.","refs":["docs/issues/_checkpoints/BPR-1519/checkpoint.json","build/bpr1519-evidence/qt-scale-100.log","build/bpr1519-evidence/qt-scale-125.log","build/bpr1519-evidence/qt-scale-200.log","/tmp/bpr-coordinator-hotkey-layout-tests.log"]}},{"criterion":"AC2","outcome":"pass","revision":"0c23b902aa98ea4f1106a117ac6177f89f8cf263","evidence":{"kind":"observation","detail":"Empty and selected File/Changes counters pass at1280/900and device scales1/1.25/2; existing large-font/floor/yield/digit-boundary geometry tests pass without overlap, clipping or changed hit targets.","refs":["docs/issues/_checkpoints/BPR-1519/checkpoint.json","build/bpr1519-evidence/qt-scale-100.log","build/bpr1519-evidence/qt-scale-125.log","build/bpr1519-evidence/qt-scale-200.log","/tmp/bpr-coordinator-hotkey-layout-tests.log"]}},{"criterion":"AC3","outcome":"pass","revision":"0c23b902aa98ea4f1106a117ac6177f89f8cf263","evidence":{"kind":"observation","detail":"Existing semantic navigation actions, exact order, accessible identities/descriptions, unique shortcuts and walked focus order pass at all3scales. Combined public navigation also passes.","refs":["docs/issues/_checkpoints/BPR-1519/checkpoint.json","build/bpr1519-evidence/qt-scale-100.log","build/bpr1519-evidence/qt-scale-125.log","build/bpr1519-evidence/qt-scale-200.log","/tmp/bpr-coordinator-hotkey-layout-tests.log"]}},{"criterion":"AC4","outcome":"pass","revision":"0c23b902aa98ea4f1106a117ac6177f89f8cf263","evidence":{"kind":"observation","detail":"Six local title-row/focus rows pass on exact source0c23b902a; full-window offscreen renders inspected. Coordinator actual Git/cache composition at integrated mainfce3377fb: application_review_navigation_entry32.28s, application_diff_workbench_public_path4.44s, integrated title-row3.85s; combined6/6passed.","refs":["docs/issues/_checkpoints/BPR-1519/checkpoint.json","build/bpr1519-evidence/qt-scale-100.log","build/bpr1519-evidence/qt-scale-125.log","build/bpr1519-evidence/qt-scale-200.log","/tmp/bpr-coordinator-hotkey-layout-tests.log"]}}]
---

# BPR-1519: Bring File and Change labels closer to their controls

Reduce the excessive visual gap between the File and Change navigation labels and their associated controls. Shift the labels right toward the controls, as suggested by the owner, using the existing layout system.

## Acceptance

- AC1: File and Change labels sit close enough to their corresponding navigation controls to read as clear groups, without the current excessive gap.
- AC2: Both groups retain consistent spacing and alignment at normal and narrow supported window widths and supported display scaling, without overlap or clipped text.
- AC3: Navigation, focus order, accessible label relationships and existing control hit targets remain correct.
- AC4: Verify the actual offscreen rendered navigation area at representative widths/scales and exercise its existing controls.

## Coordination

BPR-1517 owns stalled file navigation; BPR-1518 owns cursor-relative change navigation. Keep this visual spacing correction separate and coordinate shared navigation-layout files. Load frontend-design:frontend-design before UX edits per AGENTS.md. Never drive or capture the owner’s live desktop. Any required human judgement uses a short attended session and exact frozen candidate.

## Implementation and evidence

Source candidate: `0c23b902aa98ea4f1106a117ac6177f89f8cf263`, based on
`583eb468974b81d050574841f22491c7462da999`. The File and Changes labels now
right-align text within their existing reserved counter slots. The arrow
rectangles, toolbar spacing, rails, order and native palette/font stay the same.
Loaded `frontend-design:frontend-design` before UX edits and reviewed the small
layout correction against the owner's explicit suggestion to move the labels right.

- AC1: The actual QLabel rasters show 8.8–10 logical pixels between the last
  strong text pixel and the first arrow. Restoring the old left alignment as a
  negative control produces 95–96 pixels and fails the twelve-pixel bound.
- AC2: Empty and selected counters render whole without overlap at 1280 and
  900 logical pixels, at 1.0×, 1.25× and 2.0×. Existing digit-boundary, large-font,
  floor, counter-yield and stable-hit-target checks also pass.
- AC3: Existing navigation translation, exact action order, accessible names and
  descriptions, shortcut uniqueness and walked focus order pass at all three scales.
- AC4: Inspected full-window offscreen renders at normal and narrow widths,
  including 1.25× and 2.0×. The coordinator integrated this source into main
  `fce3377fb` and the actual Git/cache `application_review_navigation_entry`
  course passed in 32.28 seconds. `application_diff_workbench_public_path`
  passed in 4.44 seconds and the integrated title-row check passed in 3.85 seconds;
  the six-row combined batch passed in 52.67 seconds.

Six focused tests passed through
`ctest --preset dev --output-on-failure -V -R '^qt_workbench_(title_row|focus_order)_scale_(100|125|200)$'`
(run serially per scale with separate render prefixes). Context: macOS 26.6.2,
arm64, Qt 6.11.2, offscreen platform, inherited application font/palette,
132-line synthetic comparison with eleven separated groups. The six test rows
took 21.9 seconds in total; this is layout/interaction evidence, not a timing budget.

`clang-format` 22.1.8 dry-run, `git diff --check` and the documentation gate pass.
Local synthetic logs and renders remain under `build/bpr1519-evidence/`, with
digests and command state in `docs/issues/_checkpoints/BPR-1519/checkpoint.json`.

Documentation impact: no semantic API, authority, provider, persistence or
navigation-policy change. The existing workbench contract already places each
label before its arrows; only text alignment within its reserved Qt slot changes.

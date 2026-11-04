---
schema: "implement-rfc.card.v1"
id: "BPR-1856"
title: "Add repository above the clone folders in Settings › Repositories"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"ux","found_by":"owner request, 2026-11-04: \"In the settings menu for repositories the add repository section should be up too ahead of the clone folders\"","reservation":"docs/issues/_reservations/BPR-1856--BPR-1856--39ff953239dd.json","related_cards":["BPR-1308","BPR-1309","BPR-1311","BPR-1750","BPR-1753"],"frontend_design":"loaded: frontend-design:frontend-design for the section order and the spacing above the Clone folders heading"}
acceptance: []
---

# BPR-1856: Add repository above the clone folders in Settings › Repositories

## Request

The owner, 2026-11-04: "In the settings menu for repositories the add
repository section should be up too ahead of the clone folders".

Before this card Settings › Repositories drew Clone folders and Linked clones
first and the account's repository card with Add repository last, because
`settings_window::adopt_panel` puts each panel it adopts at the top of the page
and the clone folders panel is adopted after the repositories editor. Tab and
the accessibility tree already followed adoption order (repositories first), so
the drawn order also disagreed with the keyboard order.

## Decisions

- `settings_window::adopt_panel_below(page, panel, above)` places a panel
  directly under one the page already holds; `adopt_panel` keeps its meaning
  (other pages are unchanged). The clone folders panel is adopted under the
  repositories editor, so drawn, Tab and accessibility order agree: lede,
  saved repositories card (list, Remove), the New repository card when open,
  Add repository, the confirmation, then Clone folders and Linked clones.
- The clone folders panel has a 16 px top margin, so its heading starts a new
  section under Add repository instead of sitting on the button's row; it
  matches the 16 px rhythm inside the repositories section.
- `docs/ux/workbench-specification.md` (Add repository) states the order.

## Acceptance

- **AC1** Settings › Repositories draws the saved repositories and Add
  repository above Clone folders; Tab from Add repository moves into the clone
  folders; the accessibility tree presents the repositories editor before the
  clone folders panel. Automated at 100, 125 and 200 % in
  `test_add_repository_comes_before_the_clone_folders`
  (`qt_workbench_home_discovery_scale_*`).
- **AC2** Adjacent Settings, agent-setup and composed courses still pass;
  format and documentation gates pass.
- **AC3** (human) The owner accepts the new order and spacing on a frozen
  build.

## Evidence

Recorded at closeout.

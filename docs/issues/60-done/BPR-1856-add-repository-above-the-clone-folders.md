---
schema: "implement-rfc.card.v1"
id: "BPR-1856"
title: "Add repository above the clone folders in Settings › Repositories"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"ux","found_by":"owner request, 2026-11-04: \"In the settings menu for repositories the add repository section should be up too ahead of the clone folders\"","reservation":"docs/issues/_reservations/BPR-1856--BPR-1856--39ff953239dd.json","related_cards":["BPR-1308","BPR-1309","BPR-1311","BPR-1750","BPR-1753"],"frontend_design":"loaded: frontend-design:frontend-design for the section order and the spacing above the Clone folders heading"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "644cd4e41", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_workbench_home_discovery_scale_(100|125|200)$'", "detail": "test_add_repository_comes_before_the_clone_folders: the repositories editor precedes cloneFoldersPanel in the page column; Add repository's bottom is above the Clone folders top with at most 64 px between; the focus chain reaches the list and Add repository before any clone-folder control and no editor control after it; Tab from Add repository lands in the clone folders; the page's accessible children list the editor before the panel. Before the change the same course failed with 'the saved repositories come before the clone folders in the page's column'. The full workbench run (qt_workbench_window_scale_100) also runs the case."}}, {"criterion": "AC2", "outcome": "pass", "revision": "644cd4e41", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(qt_settings_window.*|qt_agent_setup_view|qt_composed_github_scale_100|qt_release_maintenance_scale_100|application_personal_discovery.*public_path|qt_workbench_window_scale_100|coding_format_check|architecture_boundaries)$'; python3 scripts/verify_documentation.py --root .", "detail": "All pass except architecture_boundaries, which fails on main too: the QToolTip::showText at workbench_window.cpp:5014 from BPR-1850's 8d448697b (tooltip placement, BPR-1354), not touched here. coding_format_check passes (clang-format 22.1.8); DOCUMENTATION_OK."}}, {"criterion": "AC3", "outcome": "pass", "revision": "644cd4e41", "evidence": {"kind": "observation", "detail": "Owner Accept, no note, 2026-11-04T21:28:52Z: attended session fc783354a054253fa795bb26df12a37b (state /Users/jaeger/.codex/attended-review/bpr1856-v001-Om6pSP/state), candidate BPR-1856-repositories-order, fingerprint 0aeb46e267b7751c9a78e95fb3bac9644646b4b978a612333dbb9dcb8073f01d, decision 09ff3219-93fa-4c08-a4f5-fac41e5816b1, handed back at cursor 6. Frozen build BPR-1856-v001 from 644cd4e41 (executable sha256 05a637ff); host stopped and frozen root removed after the hand-back."}}]
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

Offscreen renders of the course (before at main, after at 100, 125 and 200 %)
were inspected and are embedded in the attended session; none are committed.
AC1 and AC2 pass at `644cd4e41`. AC3: the owner accepted the order and spacing
on frozen build `BPR-1856-v001` (from `644cd4e41`) in session
`fc783354a054253fa795bb26df12a37b`, with no note, on 2026-11-04.

---
schema: "implement-rfc.card.v1"
id: "BPR-1587"
title: "Shared drawer parts: header, card, ownership marks and empty state"
status: "Done"
rfcs: ["RFC-BPR-0085"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "drawer-foundation", "criteria": ["AC01"], "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}, "requested_skill": "frontend-design"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"6464b4fc3","evidence":{"kind":"command","detail":"qt_drawer_parts_scale_{100,125,200}: the shared parts render in Light, Dark and both high-contrast themes with no principle violation; one negative control per rule (nested tab, stacked buttons, style sheet, empty-value rows) fails the audit; the Comments drawer's courses (qt_workbench_window_scale_*, qt_composed_github_scale_*, qt_composed_long_thread_scale_*) pass with the extracted header. Full dev suite on the tree merged with main (6464b4fc3's parent dc692295b, -j 4, load 60-76): 773 of 777 pass; qt_composed_long_thread_scale_{125,200} and designated_live_public_observer_fixture pass rerun alone; live_mutation_isolation times out at 300 s and application_large_review_history misses its epoch-refresh budget under load 35-40, both load-bound (BPR-1598 owns load-robust large-review timing; the change adds no network, mutation or per-refresh work)."}}]
---
# BPR-1587: Shared drawer parts: header, card, ownership marks and empty state

Build the RFC-BPR-0085 principles as one set of Qt adapter components, so every
later milestone draws from them rather than stock widgets.

## Outcome

- **Drawer header.** The tab row with the accepted Comments tab look (34
  pixels, semibold labels, 2-pixel signal underline, count pill) and tools as
  icons at its right; extracted from today's tray header so the Comments
  drawer is unchanged.
- **Card.** The closed note and the open card (panel, shadow, 3-pixel edge and
  notch, in amber for reading and blue for writing), painted from palette
  roles, with the ownership variants: solid, dashed, dashed with a lock,
  agent square avatar, numbered tour circle.
- **Empty state.** Glyph, one sentence, one action. Waiting and failure use
  [[BPR-1576]]'s pattern when it lands; this card adds no loading look of its
  own.
- **Rules as tests.** A Qt course renders each part in the four themes at 100,
  125 and 200 percent and fails on a nested tab, a stacked button column or a
  colour outside the palette roles.

## Acceptance

- RFC-BPR-0085 AC01 for the parts: the components exist, the Comments drawer
  uses the extracted header unchanged (its existing courses pass), and the
  render course with its negative controls passes.

## Boundaries

Qt adapter only. No surface changes look except through BPR-1588 in the same
milestone. `frontend-design:frontend-design` is loaded first.

## Execution record (2026-10-17)

`frontend-design:frontend-design` was loaded first; the brief pins the
accepted Comments drawer's look, so no token was added.

**What changed.** `bpr/qt/drawer_parts.hpp` holds `drawer_count_pill` and
`drawer_header` (the Comments tray's pill and tab row, moved byte for byte),
`configure_drawer_tabs`, `make_drawer_tool`, `drawer_icon` (tour, hide, page),
`paint_drawer_card`/`drawer_card` (closed, reading, writing; published,
unpublished, private, agent; the amber or blue edge and notch) and
`drawer_empty_state`, all from palette roles with no style sheet. The Comments
drawer now builds its header from them, unchanged.
`drawer_principle_violations` checks a widget tree for nested tabs, style
sheets, stacked button columns and empty-value rows; later milestones' courses
run it.

**Verification** (macOS 26.6.2 arm64, Qt 6.11.2, `build/dev` Debug):

| Check | Result |
|---|---|
| `qt_drawer_parts_scale_{100,125,200}` (new) | Pass: the parts in Light, Dark and both high-contrast themes (open card is Base, reading edge amber, pill, one-sentence empty state with one action, no violations); header wraps its tools at 150 px; one negative control per rule (nested tab, three stacked buttons, a style sheet, "not available", "—", "0") fails the audit |
| Comments drawer unchanged | `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_{100,125,200}`, `qt_composed_long_thread_scale_{100,125,200}` pass |
| Full `dev` suite at `70d7d2de1` (775 tests, `-j 4`, load average 60 to 76 from six other agents' builds) | 763 pass. Of the 12: `coding_format_check` was one unformatted line (fixed); `documentation_structure`, `documentation_issue_graph_contract`, `issue_id_allocation_contract` and `rfc0016_production_identity_security` were the worktree's missing or linked `.agents`; `application_composition_contract` was the pins (resealed); `application_diff_host`, `qt_composed_long_thread_scale_{100,125,200}` and `application_profile_multi_window_public_path` were load (all pass rerun alone; the long-thread course also passes on the base revision and twice on this one); `application_large_review_history` misses its epoch-refresh budget under this load and is retried when the machine is quiet |
| Merged with `main` at `e5182fd8e` | 25 affected courses pass with `-j 3`; `DOCUMENTATION_OK`; clang-format 22 clean; clang-tidy finds nothing on changed lines |

Renders (uncommitted): `drawer-parts-{light,dark,high_contrast_light,high_contrast_dark}.png`
and `status-drawer.png` from the course's `BPR_DRAWER_RENDER_ROOT`.

---
schema: "implement-rfc.card.v1"
id: "BPR-1134"
title: "The right tray's tabs"
status: "Done"
rfcs: ["RFC-BPR-0052"]
depends_on: []
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-09"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
acceptance: [{"criterion":"Each tab shows only its own content and tools.","outcome":"pass","revision":"2770ebb1","evidence":{"kind":"command","detail":"macOS 26.6.2 arm64, build/dev Debug at 2770ebb1: qt_workbench_window_scale_{100,125,200} pass test_the_right_tray_holds_comments_and_findings_tabs: Comments shows the conversation and Next new comment beneath the tabs and nothing of Findings; Findings shows the agent panel, side chat, finding queue and defect controls and not the conversation; each set is its tab's descendant. Control A (the finding queue back in the Comments column) fails 'the Comments tab shows the conversation and Next new beneath the tabs, and nothing of Findings'; control B (the tab not switching the page) fails the 200-pixel reachability course. Renders at 100/125/200 percent (workbench-window-*-tray-{comments,findings}.png). Windows and Linux runs owed with the command on the card."}},{"criterion":"Switching tabs keeps each tab's scroll and selection.","outcome":"pass","revision":"2770ebb1","evidence":{"kind":"command","detail":"Same test at 2770ebb1: the selected conversation survives a trip to Findings and back; the Findings column's scroll value survives a trip to Comments and back (each tab is its own QScrollArea in a stacked page); qt_composed_github_scale_{100,125,200} scroll the Findings column through the composed window."}},{"criterion":"Keyboard and accessibility reach every tab.","outcome":"pass","revision":"2770ebb1","evidence":{"kind":"command","detail":"Same test at 2770ebb1: the tab bar and Next new comment are declared focus stops (qt_workbench_focus_order_scale_{100,125,200} pass with the tray's stops after the drawer's rail); Left/Right arrow move between the tabs; QAccessible names each tab ('Comments, 2 threads with new comments', 'Findings'); the AI tools action and the tab follow each other. A native screen-reader listen (VoiceOver/Narrator) is an owner ask recorded on the card for BPR-1135's attended look."}},{"criterion":"The Comments tab keeps the aligned cards and their behaviour exactly.","outcome":"pass","revision":"2770ebb1","evidence":{"kind":"command","detail":"At 2770ebb1: qt_workbench_conversation_reveal, qt_github_conversation_panel, qt_conversation_cards_view, qt_composed_resident_drawer_filters, qt_shared_snapshot_navigation, qt_workbench_window_scale_* (the card, reveal and read-state courses) and application_workbench_composition_contract pass unchanged; the conversation panel is the same widget moved into the Comments tab's column. The agent rows pass 24 of 25; the one failure is BPR-0966's known application_agent_finding_dif_context_contract, unchanged."}}]
---

# BPR-1134: The right tray's tabs

## Outcome

The conversations drawer's header becomes tabs: Comments and Findings, and
Tour when RFC-BPR-0039 provides it. Each tab's tools sit beneath the tabs.

- **Comments** keeps its scope and filters, and the new-comment count.
- **Findings** takes the AI tools, the finding queue and the defect controls,
  which today stack under the threads.

The drawer's toggle, its width and its modality presets are unchanged.

## Acceptance

- Each tab shows only its own content and tools.
- Switching tabs keeps each tab's scroll and selection.
- Keyboard and accessibility reach every tab.
- The Comments tab keeps the aligned cards and their behaviour exactly.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-09 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.

## Execution record

What was built, how it was verified, and what is owed elsewhere. The
acceptance above is unchanged by it.

## Done on macOS, 2026-09-25 (main, `2770ebb1`)

The owner activated RFC-BPR-0052 on 2026-09-25 ahead of canonical order (see
the RFC's status section), so this slice started without waiting for
RFC-BPR-0028 to RFC-BPR-0038.

### What changed

- **The tray's header is its tabs.** The conversations drawer holds a
  `QTabBar` (`workbenchTrayTabs`, named "Right tray") with **Comments**, then
  **Findings**, over a stacked page per tab. It opens on Comments. Tour is
  RFC-BPR-0039's to add as a third tab.
- **Comments** (`workbenchCommentsTab`) holds the conversation panel, with
  its scope, filters and cards exactly as before, and **Next new comment**
  (`workbenchNextNewCommentButton`, the window's existing action) beneath the
  tabs. The tab carries the count of threads with new comments as an accent
  pill painted from the palette (`workbenchTrayCommentsCount`), and its
  accessible name says it: "Comments, 2 threads with new comments", "Comments,
  one thread with new comments", or "Comments". The count is the same one the
  title row's comments toggle carries ([[BPR-1129]]).
- **Findings** (`workbenchFindingsTab`) holds the agent launch panel, the side
  chat and its actions, the finding queue and its actions, the defect
  navigation, card and verdicts, in their previous order. They no longer stack
  under the threads. Dif's decorative slot sits at the tab's top right.
- **One choice, not two.** RFC-BPR-0031's explicit AI selector was a separate
  "AI tools and Findings" button above the column. It is gone. The Findings
  tab is the selector now: the `showWorkbenchAiToolsAction` action (text
  "Findings") and the tab follow each other. Choosing Findings permits Dif's
  slot and plays the reveal cue as the button did. It dispatches no agent or
  review action and never opens a collapsed drawer.
- **Each tab keeps its place.** Each tab's column is its own scroll area
  (`workbenchCommentsScroll`, `workbenchFindingsScroll`), with the previous
  column's rules: no minimum width of its own, and horizontal scrolling
  beneath its content's minimum. So switching tabs keeps each tab's scroll
  position and the selected conversation. The old button scrolled the queue
  into view when the AI tools were chosen; the tab does not, because that
  would move the Findings tab's own scroll.
- **Keyboard.** The tab bar is one focus stop after the drawer's rail, and the
  arrow keys move between the tabs. Next new comment follows it. The Findings
  tab's stops follow in the existing order.
- **Unchanged.** The drawer's toggle, its width and its persistence, its
  modality presets, Discuss borrowing the conversation for the centre (every
  drawer is hidden then, so the tab needs no notice), and the conversation's
  behaviour.
- **Specification.** [workbench-specification.md](../../ux/workbench-specification.md)
  describes the tray under "Conversation drawer", and the layout sketch names
  the tabs.

### Verification (macOS 26.6.2 arm64, Qt 6.11.2, `build/dev` Debug, at `2770ebb1`)

| Check | Result |
| --- | --- |
| `qt_workbench_window_scale_{100,125,200}` | Pass. `test_the_right_tray_holds_comments_and_findings_tabs` (new): Comments then Findings, opening on Comments; the count and its accessible name; each tab shows only its own content, beneath the tabs; the Findings tools are that tab's descendants; a selected conversation survives a trip to Findings and back; the Findings scroll survives a trip to Comments and back; the arrow keys move between the tabs; accessibility names each tab; the AI tools action and the tab follow each other. BPR-0506/0520's reachability course (`every_control_is_reachable`) runs at 200 pixels for both tabs. The Dif course now selects the Findings tab and keeps its placement, sizes and corner rules |
| `qt_workbench_focus_order_scale_{100,125,200}`, `qt_workbench_title_row_scale_{100,125,200}`, `qt_composed_github_scale_{100,125,200}` (Findings selected through the action, its column scrolled; the agent availability sentence on the Comments tab) | Pass |
| `qt_workbench_conversation_reveal`, `qt_github_conversation_panel`, `qt_conversation_cards_view`, `qt_side_chat_panel`, `qt_dif_runtime`, `qt_composed_resident_drawer_filters`, `qt_workbench_overview`, `qt_shared_snapshot_navigation` | Pass: the Comments tab keeps the cards and their behaviour |
| `application_uiforetw_selected_diff_contract`, `application_review_round_trip_composition_contract` (the pins are unchanged), `application_workbench_composition_contract`, `application_diff_workbench_public_path` | Pass |
| The agent rows: `application_agent_{chat,finding,invocation}_*` (24) and `architecture_boundary_contract` | 24 of 25 pass. The one failure is [[BPR-0966]]'s, unchanged: `application_agent_finding_dif_context_contract` fails `dif-comment-ready-after-conversion dif-acknowledge-after-acceptance` because the workshop's composition page hides the drawers. Instrumenting it showed the same path as before this card |
| `test_network_isolation_manifest`, `architecture_boundaries`, `core_dependency_boundary` | Pass |
| Control A, scratch: the finding queue back in the Comments column | Fails: `the Comments tab shows the conversation and Next new beneath the tabs, and nothing of Findings` |
| Control B, scratch: the tab no longer switches the page | Fails: `no Findings tab control is clipped at 200 pixels; the column scrolls` |
| clang-format 23.1.1 (`coding-format-check`) | Formatting check passed (816 files) |
| `_coding-standard/scripts/tidy.py` on `workbench_window.cpp` and both test files | No finding on a changed line. The three reported are existing: `dif_runtime.hpp:1`, `home_view_state.hpp:35`, `workbench_window_tests.cpp:9375` |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

Both controls were scratch edits; the candidate was restored and rebuilt. The
renders are at
`build/dev/tests/adapters_qt/workbench-window-{100,125,200}-tray-{comments,findings}.png`
(1280x560 logical): the tabs sit under the drawer's name, the Comments pill
reads 2, and the Findings tab shows the agent panel from its top.

### Owed elsewhere: the Windows and Linux runs

Qt draws the tab bar in each platform's style, and the pill takes the
palette's highlight. Each station runs, at or after `2770ebb1`:

```sh
ctest --test-dir <build> --output-on-failure -R "^(qt_workbench_window_scale_(100|125|200)|qt_workbench_focus_order_scale_(100|125|200)|qt_composed_github_scale_(100|125|200)|qt_workbench_conversation_reveal|qt_github_conversation_panel|qt_conversation_cards_view|application_agent_finding_composition_contract|application_agent_invocation_composition_contract|test_network_isolation_manifest)$"
```

- **Windows (bpr-main-win): owed.** Expect 15 passes, and the six
  `workbench-window-*-tray-*.png` renders in the build's
  `tests/adapters_qt` directory.
- **Linux (bpr-main-linux): owed.** Expect 15 passes.

A failure on either is this card's defect.

### Owner visual asks (for the attended look BPR-1135 needs; no session prepared)

Each ask is one question about one criterion.

1. **Each tab shows only its own content.** Opening a pull request with
   comments on macOS and on Windows: do the tray's tabs, Comments with its
   count pill and then Findings, read as the drawer's header the way round v8
   draws them?
2. **Keyboard and accessibility reach every tab.** With VoiceOver on macOS (or
   Narrator on Windows), Tab to the tray's tabs: does it announce "Comments, N
   threads with new comments", and does Right Arrow reach "Findings"?

## Owner verdicts — 2026-09-25 (attended session 06ad5734, macOS)

Packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md),
session `06ad5734ed9f7c2fd6d0a62c8328fa0a`, hand-back at sequence 54. The
questions were carried verbatim, with identical fingerprints, from session
`60bd1ed5`, which still lists them unanswered. They bind the frozen m2 app:
source `ba9d2723`, executable `a2e5128a…8a83`. Every decision is `actor:
human`, and its fingerprint equals the prepared one. This card's acceptance
above is unchanged; the work the verdicts ask for is on new cards.

| Ask | Candidate, decision | Verdict, verbatim | Disposition |
|---|---|---|---|
| 1. The tray header | `BPR-1134-tray`, `6c6d2c52`, seq 52 | **reject**: "I have no idea what the hell findings is supposed to be." | Header not accepted; [[BPR-1204]] |
| 2. Keyboard and VoiceOver | `BPR-1134-voiceover`, `6ee64abe`, seq 53 | "Yes, the announcement and Right Arrow both work" | macOS VoiceOver accepted (native listen by the owner) |

Narrator on Windows is still owed. If [[BPR-1204]] renames the second tab, the
VoiceOver listen is repeated for the new name only.

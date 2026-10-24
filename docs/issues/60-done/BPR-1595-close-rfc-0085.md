---
schema: "implement-rfc.card.v1"
id: "BPR-1595"
title: "Verify and close RFC-BPR-0085"
status: "Done"
rfcs: ["RFC-BPR-0085"]
closeout_for: ["RFC-BPR-0085"]
depends_on: ["BPR-1589", "BPR-1590", "BPR-1593", "BPR-1594", "BPR-1603", "BPR-PENDING-AR"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "Owner decisions in attended session 9b6359b5e66fa4d761e1acb025c4697b (BPR-1574), 2026-10-17", "reservation": "docs/issues/_reservations/BPR-1587--BPR-1595--796d1dde48eb.json", "milestone": "drawer-closeout", "milestone_plan": {"id": "drawer-design-system-v1", "path": "docs/rfcs/plans/drawer-design-system-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC08","outcome":"pass","revision":"270077e655d630ad8afa687046b6bb48bc4f0067","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R '^qt_drawer_surfaces_fit_scale_(100|125|200)$'","detail":"Every rebuilt surface renders without clipping in Light, Dark and both high-contrast themes at 100, 125 and 200 percent, at its narrowest and widest width, after BPR-PENDING-AR's repairs; at 6c998babb the course failed 68 states at each scale.","refs":["docs/issues/60-done/BPR-PENDING-AR-drawer-surfaces-keep-their-words-at-every-width.md"]}}]
---

# BPR-1595: Verify and close RFC-BPR-0085

Close [RFC-BPR-0085](../../rfcs/RFC-BPR-0085-drawer-design-system.md) once
every milestone has landed with its owner verdict.

RFC base: `4317330bad446c4c1577b6d6bf09d458cf8edb9a`.

## Closeout checklist

| Criterion | Direct check / evidence | Result at the closeout attempt | Producing commit |
|---|---|---|---|
| **AC01 Shared parts:** the drawer header, card, ownership marks and empty state are one set of Qt components used by every surface below; Qt courses render each in the four themes at 100, 125 and 200 percent with negative controls (a nested tab, a stacked button column, a non-role colour). | `qt_drawer_parts_scale_{100,125,200}` (the parts in Light, Dark and both high-contrast themes; one negative control per rule); the Comments drawer's courses with the extracted header; [[BPR-1587]] AC01. Rerun at `6c998babb`. | Pass: the parts render in the four themes at the three scales and each negative control fails the audit; the three rows pass again at `6c998babb` | `6464b4fc3` |
| **AC02 Status is a drawer:** the Overview's side column resizes within its bounds, collapses by toggle and key, remembers its width per window across restart, and is reachable by keyboard; there is no Start review button; ⌘2 from a fresh Overview selects the first unseen change's file and Return on Files opens the selected file; the owner answers the promote-label question. | `qt_drawer_parts_scale_*` (260 to 560, collapse, rail, keyboard), `sqlite_workbench_settings_storage`, `application_host`, `application_profile_window_layout_public_path` (restart keeps it), `qt_workbench_window_scale_*` (no Start review; first ⌘2); [[BPR-1588]] AC02; owner session `e471b4b84fdd0009bd645ca916ffe64d` | Pass: the owner accepted the frozen build (decision `6897d336-3add-44da-8226-f4cd057dd5f9`) and chose B, Draft comment (decision `bcb5d520-636d-425f-8705-71c0c8d5012b`); the window rows pass again at `6c998babb` | `6464b4fc3` |
| **AC03 Status content:** the drawer leads with my role and what needs attention as specified, names each failed check in one place, draws no empty row, expands each section to its details, and Go to them opens Threads with Only new (or Discuss for pathless comments); core courses cover every role, including several at once and unobserved team membership. | `core_review_role`, `application_pull_request_overview`, `github_paged_read`, `github_provider`, `qt_pull_request_overview_page`, `qt_workbench_window_scale_100`, `application_status_attention_public_path`; [[BPR-1589]] AC03; owner session `82f52ad4245bb64879f3bf2680814dea` | Pass: the owner accepted v002 (decision `71bc675f-6911-4d51-a034-74a8c97c1a63`) after rejecting v001; `qt_pull_request_overview_page` passes again at `6c998babb` | `88d5f086c` |
| **AC04 Compose review:** Finish review opens the sheet over the current surface with no thread list; a pending row reveals its line behind the sheet; Expand and Restore keep text, caret and scroll; BPR-1541's panel and every submission outcome behave as before. | `qt_review_submission_editor_scale_*`, `qt_workbench_window_scale_*` (sheet, Expand and Restore keep text, caret and scroll), `application_start_review_contract`, the review submission composition public path and its scales; [[BPR-1590]] AC04; owner session `95b80a7a4bd9bf3e575d2338a8986edc`. The submission public path and its two scales aborted on a stale Agent assertion after [[BPR-1594]]; [[BPR-1616]] repaired them and they pass from `7c88aed2f` | Pass: the owner accepted v003 (decision `89f1f2df-f2bd-4cfb-b2ff-3e69cd4a4d5a`), the hover words removed as the note asked; every submission outcome passes again from `7c88aed2f`; the editor and window rows pass at `6c998babb` | `59d2234e6`, `615be6e85`, `7c88aed2f` |
| **AC05 Notes:** notes hang at their lines and open in place; a note written on a line survives restart and a new head (or is shown as outdated); the promote action creates a local draft at the note's line and publishes nothing; Notes shows no agent tools. | `core_note_placement`, `sqlite_review_ledger_storage` (schema 10, restart, new head), `qt_private_notes_panel`, `qt_workbench_window_scale_*` (no agent tools in Notes), `application_review_submission_composition_public_path` (Draft comment at the line, zero GitHub mutation); [[BPR-1591]] and [[BPR-1592]] AC05; owner session `23a47c6b542ce5e92f47a335594b3a42` | Pass: the owner accepted v001 with no note (decision `1d30aced-1d77-4ee1-a30f-3c4f29e90d2c`); `qt_private_notes_panel` passes again at `6c998babb` | `467d325ad`, `c2487b258` |
| **AC06 Tours:** a tour is written, reordered by pointer and keyboard, expanded to the tour page and back, previewed and followed in the route, with code and explanation visible together. | Every tour row, `qt_workbench_window_scale_*` (`test_the_tour_is_a_route_expanding_to_its_page`), the composition contracts; [[BPR-1593]] AC06; owner session `9a9c8a15076be0d2dbf02051bf3e0b6b` | Pass: the owner accepted v001 with no note at sequence 4; the window rows pass again at `6c998babb` | `a1222c7f2`, `eb4990c98` |
| **AC07 Agent:** ⌥⇧A, ⌘I and the context menu reach the Agent tab; the four states render; a finding becomes a local draft only on a person's press; sessions, earlier heads and private runs are reachable from History. | `qt_workbench_window_scale_*` (`test_the_agent_is_a_drawer_tab`), keyboard scales, side chat, agent panels, finding queue and finding public path, both composition contracts; [[BPR-1594]] AC07; owner session `5547b2341ac4b1228c7f89e5ed5110c1` | Pass: the owner accepted v001 with no note at sequence 4; the window rows pass again at `6c998babb` | `21030cfb4`, `cd19a8988` |
| **AC08 Themes and sizes:** every rebuilt surface renders without clipping in the four themes at 100, 125 and 200 percent and at its narrowest and widest width. | `qt_drawer_surfaces_fit_scale_{100,125,200}` (`4bdaddb2d`, first written as `c42b7d1b5`): Status at 260 and 560; Comments with the edge chip and a review summary open in place, Notes, the Tour route and the Agent tab at 280 and 760, restored and dragged; the Compose sheet, its page and the tour page at the 900-pixel window floor and in a 1920-pixel window; each in the four themes, with negative controls; offscreen renders at 100 and 200 percent; [[BPR-PENDING-AR]] AC1 | Pass at `270077e65`: every state at all three scales, after [[BPR-PENDING-AR]] repaired the four defects of the 2026-10-23 attempt (68 states at each scale at `6c998babb`) | `4bdaddb2d` (the course), `270077e65` (the repairs) |
| **AC09 Owner looks:** the owner accepts each milestone's frozen build. | The attended sessions of every milestone: drawer-foundation `e471b4b8…`, overview-status `82f52ad4…`, compose-sheet `95b80a7a…`, notes-margin `23a47c6b…`, tours-route `9a9c8a15…`, agent-tab `5547b234…`, conversation-chips `d28b9f71…` | Pass: each milestone's frozen build was accepted by the owner (`actor: human`), as recorded on [[BPR-1588]], [[BPR-1589]], [[BPR-1590]], [[BPR-1592]], [[BPR-1593]], [[BPR-1594]] and [[BPR-1603]] | `6464b4fc3`, `88d5f086c`, `59d2234e6`, `c2487b258`, `eb4990c98`, `cd19a8988`, `f80960a40` |
| **AC10 Pull-request comments at the edge (amendment):** conversation comments and review summaries on no file wait as a chip at the Comments drawer's top edge, open in place with their unread marks, and offer Show in Discuss; Discuss is unchanged; the owner accepts the design note's mockup and the frozen build. | `qt_conversation_cards_view` (`test_pull_request_comments_live_at_the_edge`: one chip counting live singletons, creation order, mark read, verdict, Show in Discuss, Discuss unchanged), the conversation read public path, staged review session courses, the live BPR-Test PR 108 course; [[BPR-1603]] AC10; mockup direction A chosen in session `537cac7b…` (sequence 4), built v001 in session `d28b9f71ffc1dfc153f9b731595c0dca` | Pass: the owner chose direction A and accepted the built v001 with no note (decision 6); `qt_conversation_cards_view` passes again at `6c998babb` | `9eab5046a`, `f80960a40` |

## Final closeout record, 2026-10-24

Closed: every criterion passes. AC08, the one the attempt below found
unmet, passes at `270077e65`: [[BPR-PENDING-AR]] repaired its four
defects (the tray tab row's stale width, the open summary's header, the
Tour stop's actions and the tour page's columns at the window floor)
and `qt_drawer_surfaces_fit_scale_{100,125,200}` pass in all four
themes. The milestone courses pass again at `270077e65`:
`qt_workbench_window_scale_*`, `qt_drawer_parts_scale_*`,
`qt_review_submission_editor_scale_*`, `qt_private_notes_panel`,
`qt_pull_request_overview_page`, `qt_conversation_cards_view`,
`qt_github_conversation_panel`, `qt_composed_inline_drawer_scale_*` and
the review tour composition, publication and writing rows (34 of 34
with the course; macOS 26 arm64, Qt 6.12, Debug, offscreen). AC01 to
AC07, AC09 and AC10 keep the evidence recorded at the attempt.
RFC-BPR-0085 is set to `Auto-Verified`, which lets [[BPR-1756]] close
RFC-BPR-0092.

Windows and Linux are unmeasured for AC08; the course ran on macOS.

## Closeout attempt, 2026-10-23

Not closed: AC08 is unmet. Every other criterion has its evidence above, and
the milestone courses still pass at `6c998babb` (`qt_drawer_parts_scale_*`,
`qt_workbench_window_scale_*`, `qt_review_submission_editor_scale_*`,
`qt_private_notes_panel`, `qt_pull_request_overview_page`,
`qt_conversation_cards_view`: 12 of 12, macOS 26 arm64, Qt 6.12, Debug,
offscreen).

AC08 was the closeout's own criterion and had no course. The attempt wrote
`tests/adapters_qt/drawer_surfaces_fit_tests.cpp`, registered as
`qt_drawer_surfaces_fit_scale_{100,125,200}`. It is on branch
`bpr-1595-ac08-fit-course` (commit `c42b7d1b5`) and not on `main`, because
it fails. It renders Status at 260 and 560 pixels; Comments with the pull
request's comments opened from their chip, Notes, the Tour route and the
Agent tab at 280 and 760 pixels, both restored from the layout and dragged
by the seam; and the Compose sheet, its page and the tour page in the
900-pixel window floor and a 1920-pixel window. All of these run in the four
themes. A control past its surface, a button or unwrapped label below its
minimum size hint, and a tab past its bar each count as clipping, and each
has a negative control. Offscreen renders confirmed every defect below:

1. **The Comments drawer's tab row.** The tray tab bar keeps 192 pixels,
   four glyph-only tabs, after the faces change. At 760 pixels, restored or
   dragged, and at a restored 280, Tour and Agent sit behind the bar's
   scroll arrows. The window course's tab test does not catch this. It
   starts at 420 pixels with the files drawer collapsed and no theme
   applied, and reaches each width by dragging. Why the dragged 760 here
   differs from that test's is not yet known.
2. **The open review summary at 280 pixels.** The header's "Pull request" and
   "Changes requested" are plain labels with no elision; they are cut to 64
   pixels ("Pull reques", "Changes re"). This is [[BPR-1603]]'s surface.
3. **The Tour stop's actions at 280 pixels.** In the route, "Enhance text"
   (85 of 88 pixels) and "Use selected lines" (84 of 119) are cut. They are
   cut in the tour page at the window floor too, as is "Related source".
4. **The tour page at the 900-pixel window floor.** The On GitHub column
   (`tourPageGithub`, `tourPublicationPanel`) runs to 955 pixels in a
   900-pixel page, so its right edge and its words are off the window.

One more report is unconfirmed: in the 1920-pixel tour page, a
`tourStopCard` is 355 pixels wide in a 266-pixel list viewport, yet the
render shows the outline whole. It may be an item widget that is not
painted.

Next: fix cards for the four defects. Each takes the course on
`bpr-1595-ac08-fit-course` as its acceptance check, and the course lands
with the last of them. Then this card reruns the course at the three scales,
records AC08, and closes RFC-BPR-0085, and [[BPR-1756]] after it.

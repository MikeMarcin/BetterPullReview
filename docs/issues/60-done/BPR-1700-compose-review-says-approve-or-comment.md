---
schema: "implement-rfc.card.v1"
id: "BPR-1700"
title: "Compose review says \"Approve or comment…\" while no review is in progress"
status: "Done"
rfcs: ["RFC-BPR-0052"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1700--BPR-1700--963c73415d69.json","kind":"ux","origin":"owner report 2026-10-21","owner_note":"I think the compose review button is a little too subtle when there's not a active review. It might be difficult for the uninitiated to find out how to approve a review.","related_cards":["BPR-1129","BPR-1345","BPR-1157","BPR-1255","BPR-1578"],"requested_skill":"frontend-design"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a7e7d1126","evidence":{"kind":"command","command":"ctest qt_compose_review_entry_scale_100 and qt_compose_review_entry_scale_200 (--compose-review-entry at 1x and 2x)","detail":"test_compose_review_says_approve_or_comment_with_no_review: with no review in progress the bar's Compose review presenter is TextBesideIcon, text \"Approve or comment…\", icon kept; accessible name \"Compose review: approve or comment\" (holds the shown words), description names Approve, Request changes and Comment; tooltip \"Compose review (⌥R)\\nApprove, request changes or comment on the whole pull request.\"; key Alt+R and menu text \"Compose &review\" unchanged; StrongFocus; whole, nothing folded; edge carries the accent's hue and the face is the button's own. Clicking it opens the review sheet and submits nothing. Failing-before control: with workbench_window.cpp and workbench_strip.cpp at 15bb7f8a4 both selector runs fail at 'with no review in progress Compose review reads \"Approve or comment…\" beside its pencil'."}},{"criterion":"AC2","outcome":"pass","revision":"a7e7d1126","evidence":{"kind":"command","command":"ctest qt_compose_review_entry_scale_100 and _200","detail":"With two pending drafts the chip shows and Compose review is IconOnly, named \"Compose review\", without the accent edge; so while the review is being submitted; with nothing pending the words and edge return. The kept-draft amber mark is unchanged (test_compose_review_marks_a_kept_draft runs in the same selector)."}},{"criterion":"AC3","outcome":"pass","revision":"a7e7d1126","evidence":{"kind":"command","command":"ctest: all 45 macOS rows of better_pull_review_qt_workbench_window_tests (1x, 1.25x, 2x), application_uiforetw_selected_diff_contract, application_start_review_contract, command_correctness_registry_contract","detail":"Narrowing 1600 to the floor in 5 px steps the words go \"Approve or comment…\" to \"Approve…\" to the pencil, each once, never back at a narrower width, the toolbar extension never shows and Comment stays inside the bar. The floor is unchanged (900 offscreen): it is measured with the pencil alone. At 1280 the accepted BPR-1129 layout keeps the comparison's dates, \"File 3 of 40\" and every progress step's name beside \"Approve…\"; at 1460 it says \"Approve or comment…\" with the dates. test_compact_icon_row_preserves_exact_order_and_edges now requires words on Compose review only."}}]
---

# BPR-1700: Compose review says "Approve or comment…" while no review is in progress

## Owner note

The owner, 2026-10-21, verbatim:

> I think the compose review button is a little too subtle when there's not a
> active review. It might be difficult for the uninitiated to find out how to
> approve a review.

## What it looked like

Rendered offscreen at 1x and 2x, light and dark, before the change.

- Compose review was a 22 pixel pencil, the same size, border and weight as
  Comment's balloon beside it. Nothing on it said "review", let alone
  "approve"; only its tooltip did.
- To its left the progress track has words, and its **✓ Accept** is the
  most approve-like thing on the bar, but it accepts one file privately.
  A newcomer looking for Approve is drawn to the wrong control.
- While a review is in progress the "Review · N pending | Finish review" chip
  already leads, so the subtle state is only the one with no review.

## Change

Loaded `frontend-design` first. The direction keeps to the bar's vocabulary:
words in full ink on the ordinary button face, and one quiet cue.

- **Words.** With no review in progress the bar's presenter shows its pencil
  and "Approve or comment…". The ellipsis says it opens something. It names
  two of the review page's three verdicts, so it points at the review page
  and not at a one-click approve, and nothing is submitted until the reviewer
  chooses a verdict there. The menus and the shortcut catalogue keep
  "Compose review"; the words come from the action's icon text, which only the
  bar's presenter shows.
- **Edge.** A `stripLead` strip property draws the button's edge in the
  accent at reduced opacity, on its own face. It is not the checked look (no
  fill), and it is softened so the two-pixel focus ring still reads as focus
  over it ([[BPR-1157]]). At full accent the dark theme's focus ring was
  barely different.
- **In progress.** While drafts are pending in a review, or held by its
  submission, Compose review is the plain icon again and the chip leads.
- **Name, tooltip, key, focus.** The accessible name is "Compose review:
  approve or comment", which keeps the command's name and holds the words it
  shows. The tooltip adds "Approve, request changes or comment on the whole
  pull request." Option-R (Alt+R) and the focus order are unchanged.
- **Width.** Nothing folds ([[BPR-1255]]). As the window narrows the words
  shorten first, to "Approve…", and "Approve…" is the bar's last yield, after
  the dates, the positions and the steps' names ([[BPR-1578]]). The floor is
  measured with the pencil alone, so it does not move. A first attempt let the
  full words yield last, and that cost the accepted 1280 layout its dates;
  the test of [[BPR-1129]]'s 1280 render caught it.

`docs/ux/workbench-specification.md` and RFC-BPR-0052 (the owner report note,
the review bar text, AC-03 and the implementation table) say this.

### Why no core transition

Which words fit and whether a review is in progress are presentation of state
the window already holds (the chip's counts). No intent, selection or
decision changes.

## Shared files

`src/adapters/qt/src/workbench_window.cpp` and its header (Compose review's
presenter and `apply_review_bar_width` only), `workbench_strip.cpp` and its
header (the lead edge), `tests/adapters_qt/workbench_window_tests.cpp` (a new
case, a render sheet, two selectors, the compact-row and 1280 render cases)
and `tests/adapters_qt/CMakeLists.txt`,
`tests/end_to_end/application_uiforetw_selected_diff_contract.cmake` (the gap
bound), `tests/command_correctness_registry.json`,
`tests/workbench_case_inventory.json` and
`tests/scripts/command_correctness_registry_tests.py` (the counts).

## Verification

On `bpr-main-mac` (macOS 26.6.2, Qt 6.12.0, Debug `build/dev` at `-g0`,
offscreen), at candidate `a7e7d1126` (on `4271e2ce2`). The change was then
rebased onto `8341758d1` as `713ad6d76`. Its product diff is byte-identical to
`a7e7d1126`'s, and the list below was run again there. The registry counts
follow main's own new default case.

- `test_compose_review_says_approve_or_comment_with_no_review` (in
  `--compose-review-entry` and the default run) covers AC1 to AC3. Failing-before
  control: with `workbench_window.cpp` and `workbench_strip.cpp` restored to
  their base, both selector rows fail at its first assertion.
- Every macOS row of `better_pull_review_qt_workbench_window_tests` passes (45,
  at 1x, 1.25x and 2x), and so do `application_uiforetw_selected_diff_contract`,
  `application_start_review_contract` and
  `command_correctness_registry_contract`. The default run's 1280 render case
  keeps its pin of the dates and "File 3 of 40", and now also checks
  "Approve…" there and "Approve or comment…" with the dates at 1460.
- The UIforETW contract's 100 pixel gap bound became 8 pixels. At 1280 the real
  application's bar shows the dates, "File 3 of 5", every step and "Approve…",
  35 pixels from private progress. The words take the spacer's room only as far
  as they fit, so the gap is bounded by the toolbar's item spacing.
- `scripts/workbench_case_inventory.py --execute-lists` agrees: 45 invocations
  and 501 ordered case occurrences on macOS at `713ad6d76`. The two new rows are declared in
  `tests/command_correctness_registry.json`, and its pins and the case
  inventory are resealed. The round-trip composition pins are unchanged.
- `coding-format-check` passes (clang-format 22.1.8). clang-tidy is clean on
  `workbench_window.cpp`, `workbench_strip.cpp` and the test.
- The documentation gate refuses only on this card's provisional ID (the
  coordinator allocates it on main). `verify_documentation_tests.py` passes.

The offscreen renders use Fusion. The native macOS look is for the owner to
judge, below.

## Owner look

Two Accept or Reject questions on the frozen build are pending in their own
short attended session. They are recorded here rather than as acceptance
criteria, because they wait on the owner.

- Session `85855153b502837fdfe571ba104a19a1`, opened 2026-10-21 with fresh
  state `/Users/jaeger/.mask/bpr-pending-1-compose-review-v001-20261021/attended-review`.
  Both candidates were ready at event 8, and the loopback host was serving.
  The authenticated URL stays in the external state.
- Candidates, revision `frozen-a7e7d1126-v001`, one question each:
  - `BPR-1700-discoverable-v001` (`d53b97823588…`): with no review in
    progress, a newcomer can see from the review bar how to approve, request
    changes or comment on the whole pull request.
  - `BPR-1700-quiet-v001` (`5e857caa818f…`): "Approve or comment…" and
    its accent edge sit quietly beside the rest of the bar, in light and dark.

  Each carries before and after offscreen renders at 2x, light and dark, kept
  with the 1x ones in
  `/Users/jaeger/.mask/bpr-pending-1-compose-review-v001-20261021/renders`.
- Frozen root `/Users/jaeger/.mask/attended-review/bpr-pending-1-compose-review-v001.q61u6zra`
  from `a7e7d1126` (fixture `inline-drawer-v1`, open file
  `src/ui/conversation_cards_view.cpp`). The executable's SHA-256 is
  `34d8d5c70398…db0f6f`. The launcher's own check printed
  `CHECK_OK (first head): ATTENDED_FILE_SHOWN:src/ui/conversation_cards_view.cpp`.
  Launch with `python3 /Users/jaeger/.mask/attended-review/bpr-pending-1-compose-review-v001.q61u6zra/launch.py`.
  It carries this change exactly. It does not carry main's later work from
  `4271e2ce2` to `8341758d1` (bulk Mark read and the long-file pick reveal),
  which no question asks about.
- Verdicts, 2026-10-21: the owner answered **Accept** in chat after viewing the 2x before and after renders (light and dark), covering both `BPR-1700-discoverable-v001` and `BPR-1700-quiet-v001`, without a note. The session host was stopped and the frozen root removed.

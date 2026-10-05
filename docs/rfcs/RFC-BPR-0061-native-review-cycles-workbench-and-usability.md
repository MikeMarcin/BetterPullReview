---
id: "RFC-BPR-0061"
title: "Native Review Cycles Workbench and Usability"
status: "Proposed"
area: "BPR"
milestone: "Review Cycles"
depends_on: ["RFC-BPR-0053", "RFC-BPR-0054", "RFC-BPR-0055", "RFC-BPR-0056", "RFC-BPR-0057", "RFC-BPR-0052", "RFC-BPR-0037", "RFC-BPR-0050"]
pack_version: "review-cycles-v1"
metadata: {"pack": "bpr-review-cycles-v1", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
---

# RFC-BPR-0061: Native Review Cycles Workbench and Usability

## Outcome and decisions

Present Review Cycles inside BPR as the source-first counterpart to PR review, not a separate product or an AI tools page. Add Open Project and local-cycle navigation with simple defaults. Preserve the owner-selected RFC0052 title/review bar, folding Updates line, document tabs and tray, adapting subject-specific actions without another permanent chrome row.

Full source is the initial and persistent mode. Inline notes, exact suggestions, related references and the ordinary composer build one feedback round. Share feedback exposes first-class Copy and Save; the optional external-skill and integrated routes add convenience without controlling lifecycle. A user never has to rewrite an already-authored review into a second prompt.

Capture revision and reviewer verification form the return path even without recipient reports. Per-tab material identity, review receipts, history and ambiguity remain visible. Publication preview shows the actual outgoing candidate, not a misleading local-only diff, and keeps local conversations private unless selected.

Use existing native keyboard/accessibility/editor behavior and conduct bounded attended design decisions where genuinely unresolved. This pack does not assert approval of new art or override the accepted scaffolding. Validate usability on actual supported platforms and measure human steering/reconstruction savings without assuming token or quality improvements from batching alone.

## Owner amendment, 2026-10-01: Suggest change in the main pane

The owner's decision on `BPR-1156-composer-layout-v003` is recorded in
[RFC-BPR-0055](RFC-BPR-0055-source-feedback-and-exact-suggested-edits.md#owner-amendment-2026-10-01-suggest-change-edits-the-file-in-place).
For this workbench it means: Suggest change is offered with only a caret, from
the review bar and the source pane's context menu; the whole file is edited in
the main pane rather than in the tray; and the tray's before/after colours its
removed and added lines the way the diff panes do. RC61-04 is amended to match.

## Owner decision, 2026-10-02: Return and Shift+Return in the code, and visible focus

The owner rejected `BPR-1157-native-keyboard-v001` (events 793 and 794): Tab
moved between controls but nothing showed which had focus, and a key was
wanted for commenting, "maybe when focused on the code pane in the read-only
mode enter adds comment, shift+enter suggests edit". That suggestion is the
decision. Reading the code, Return (Enter) makes a note on the selection, or
on the caret's line when nothing is selected, and Shift+Return begins Suggest
change at the caret; while Suggest change edits, Return stays a line break.
Every stop Tab reaches on the cycle page shows the window's focus ring, the
theme's selection colour lifted to the 3:1 indicator floor. RC61-09 is
amended to match; the evidence is on
[BPR-1157](../issues/60-done/BPR-1157-integrate-local-entry-and-source-first-tabs-with-the-accepted-chrome.md#owner-verdicts-at-hand-back-795-and-native-keyboard-v002--macos-2026-10-02).

## Owner request, 2026-10-02: a key that cancels the note or suggestion

When the owner accepted `BPR-1157-native-keyboard-v002` (event 806), the
accept carried a request: "can control+c (mac) shortcut to  cancel the
suggestion/comment?" The request is recorded as a decision under RC61-09's
shortcuts. While a note or Suggest change is open and keyboard focus is inside
it, Escape cancels it on every platform, as its Cancel button does. On macOS,
Control-C cancels it as well; Command-C stays Copy. On Windows and Linux,
Ctrl+C is Copy, so cancel is not bound to it there. In the code, Escape first
collapses a selection or extra carets. The evidence is on
[BPR-1157](../issues/60-done/BPR-1157-integrate-local-entry-and-source-first-tabs-with-the-accepted-chrome.md#owner-verdicts-at-hand-back-815-and-a-key-to-cancel--macos-2026-10-02).

## Owner requests, 2026-10-04: a key that saves, and a shorter Send box

Accepting `BPR-1182-agent-free-loop-v001` (event 947), the owner asked: "I
want cmd+return to save the note so I don't have to click save note with a
mouse". This is recorded as a decision under RC61-09's shortcuts. While a
note, a reply or Suggest change is open and keyboard focus is inside it,
Command-Return on macOS and Control-Enter on Windows and Linux save it, as
its Save button does; a suggestion with nothing to save stays open. Each Save
button names the key. Return keeps its meaning: a line break while writing,
a note in the read-only code. The work and evidence are BPR-1280's.

Accepting `BPR-1182-send-box-v001` (event 951), the owner found the sheet's
text overwhelming and asked that the Send box name the harness that is
configured, not Claude, since Claude, Codex and custom harnesses are all
supported. The Send box says what Send does in one line, naming the selected
profile's harness (Claude, Codex, or a custom harness by its own name), and
keeps the rest behind Details. A session to continue is offered only by a
harness that continues sessions. The work and evidence are BPR-1282's.

## Normative contracts

The detailed contracts [UX.md](assets/bpr-review-cycles-v1/specs/UX.md), [LIMITS.md](assets/bpr-review-cycles-v1/specs/LIMITS.md) are normative parts of this proposal. The common [domain](assets/bpr-review-cycles-v1/specs/DOMAIN.md), [decisions](assets/bpr-review-cycles-v1/supporting/DECISIONS.md) and [security boundary](assets/bpr-review-cycles-v1/specs/SECURITY-AND-PACKAGING.md) apply. If a live predecessor differs, record a specific compatibility decision; do not quietly implement a weaker alternate workflow.

## Acceptance criteria

- **RC61-01:** Open Project/Review Cycles offers a simple no-account/no-agent local entry with a clear Local cycle identity distinct from PRs and Review Tours.
- **RC61-02:** Cycle controls integrate the 0052 title/review bar, folding Updates line, tabs and tray without a new unapproved persistent row or PR regression.
- **RC61-03:** Initial/full-source browsing is single-pane and remains available after comparisons; unchanged/historical local source stays commentable.
- **RC61-04:** Native note/suggestion workflows support primary/related anchors, accurate before/after, keyboard editing and explicit stale/ambiguous states; Suggest change starts from the caret, including the source pane's context menu, edits the whole file in the main pane, and previews removed and added lines in the diff panes' colours (owner amendment, 2026-10-01).
- **RC61-05:** Compose/Share prepares one complete assignment with first-class Copy and Save and no mandatory AI setup, second prompt or per-item ceremony.
- **RC61-06:** Whole-round feedback and exact delivery observations remain legible; optional direct/external routes add no hidden context or automatic dispatch.
- **RC61-07:** Capture, response display, comparison, reviewer verification and next-round composition work without callback/protocol prerequisites.
- **RC61-08:** Publication preview differentiates bases and selected final content and links the actual PR while preserving local history/privacy.
- **RC61-09:** Native width/focus/shortcuts/accessibility/IME/tab-restoration courses preserve accepted workbench design and record actual platform/user evidence; every Tab stop shows a visible focus ring, and in the read-only code Return notes the selection or the caret's line and Shift+Return begins Suggest change there (owner decision, 2026-10-02).
- **RC61-10:** Named-corpus latency/memory and the eight-item/two-suggestion human-steering course measure benefit/limitations without unverified savings claims.

## Implementation and evidence

Use the associated authored cards and imported milestone grouping. New names/types/paths
are proposals until reconciled with the live tree. Preserve functional-core/host/Qt
ownership and existing test isolation. Implement focused production-path unit/host/
native checks; use unchanged predecessor evidence where inputs have not changed.
Source inspection, reference fixtures and this archive's validator are not runtime
implementation, native/platform behavior, live provider acceptance or user approval.

Record command/gesture, result, producing revision, environment and limitations for
each acceptance criterion. Required live or attended observations stay open until
actually performed on authorized targets. Do not start models, alter shared skills,
write live repositories, or bypass corporate policy merely because a test card exists.
Keep historical RFC status/evidence unchanged; update current architecture forward.

See the [integration map](assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md), [acceptance matrix](assets/bpr-review-cycles-v1/verification/ACCEPTANCE-MATRIX.md) and [product verification contract](assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md).

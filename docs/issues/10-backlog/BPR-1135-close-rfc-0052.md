---
schema: "implement-rfc.card.v1"
id: "BPR-1135"
title: "Verify and close RFC-BPR-0052"
status: "Backlog"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129","BPR-1130","BPR-1131","BPR-1132","BPR-1133","BPR-1134"]
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
closeout_for: ["RFC-BPR-0052"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0052

## Objective

Reconcile [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
against its linked implementation cards, direct evidence and the owner's
attended look at the built chrome on macOS and Windows.

RFC base: `0453103ddd11337ee8b3adf5053b25149104acd8`.
Requested RFC set: RFC-BPR-0052; workstream position: 1 of 1.
Planned cards: BPR-1129, BPR-1130, BPR-1131, BPR-1132, BPR-1133, BPR-1134, BPR-1135. Add subsequently allocated required slices to
this list and dependencies before closeout. Registration is not implementation.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01 The chrome above the comparison is the title row, then the review bar, then the Updates line's timeline while it is open, with no other row between them in any state, and the comparison's header is the document tab strip. The Updates line folds from the review bar without moving it, a new window starts with it folded, and each window restores its own choice. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-02 The title row holds Home, the drawer toggles, the pull request's name, the modality at its right beside refresh, the comments toggle carrying the new-comment count, account and settings, with the platform's window controls. No navigation, progress or composition control sits in it, and every control stays top level at each platform's width floor. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-03 The review bar reads, from its left, the Updates disclosure with the comparison and its dates, open or folded; then previous and next change with the position, previous and next file with the position, and private progress as an icon whose tooltip names the active tab's file state; and at its right Compose review and Comment as icons whose tooltips name the command and its key. Open, the Updates line's timeline runs the full width just below it. Compose review marks a kept review draft, and Review details is reached from the comparison's menu. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-04 The Updates line carries no permanent caption. Its tooltip carries the history statement, and the line shows a mark whenever history is incomplete or ambiguous, as RFC-BPR-0029 requires. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-05 Document tabs follow RFC-BPR-0025's tab contract: a tab is a file, a comparison and a reading position; clicking a file reuses the active tab, Cmd-click (Ctrl-click) and Open in New Tab open another, Duplicate Tab copies the exact view, previous and next file change the active tab, closing a tab follows the neighbour rule, and each window keeps its pull request's tabs across restart. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-06 Each tab has its own comparison. New tabs take the window's comparison, a tab with its own pair says so on its label, and the tab's menu returns it to the window's comparison or makes its pair the one new tabs take. The Updates line shows the active tab's pair, Overview and Discuss show the window's, late work cannot reach another tab, and no exact view is silently retargeted. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-07 A tab over an older pair places only the threads that belong to its revisions, says how many are on later updates with a way to show them, and offers no new comment off the current head. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-08 Comment starts a comment on the current line, or a reply to the selected thread, and its menu names the target; its arrow is where RFC-BPR-0048's labels are offered. Every thread card offers Reply, beside the code as in Discuss. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-09 The right tray's header holds tabs, Comments and Findings, and Tour once RFC-BPR-0039 provides it, with each tab's tools beneath the tabs rather than stacked in one column. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-10 Option-R (Alt+R) composes the review, Option-C (Alt+C) comments or replies, Ctrl+Tab and Ctrl+Shift+Tab move between tabs, Cmd-W (Ctrl+W) closes the tab, with the window moving to Cmd-Shift-W (Ctrl+Shift+W) if the tab takes its key, Esc returns from a composition page, and Option-] and Option-[ are reserved for RFC-BPR-0040's tour stops. Both composition commands are in the Pull Request menu, and no key sequence is used twice in the assembled window. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |
| AC-11 The rows are measured on every supported platform and the tests follow them: application_uiforetw_selected_diff_contract and its course script take the measured stacks with the Updates line open and folded, BPR-0940's window course asserts the new arrangement, and each platform's width floor is swept again. | The linked cards' focused courses and the composed course, re-run at closeout | Not run; not started | None |

## Completion rules

Record the exact commands, results and producing commits for each criterion.
Every required linked card must be done, including slices allocated later.
Use focused changed-surface checks and a composed public path. Limit native
and attended claims to actual evidence. Run the documentation gate, then
reconcile the RFC's index status only when canonical order and its
dependencies allow.

# RFC-BPR-0127 verification

Mandate `v0-5-0-prerelease-targets-r1`, plan `comments-absorb-notes-v1`
([[BPR-2157]], [[BPR-2158]], [[BPR-2159]], [[BPR-2160]], [[BPR-2161]],
closeout [[BPR-2162]]). Frontend-design was loaded before every Qt surface
round. Owner amendments in force: "If you need it you need it go build it.";
"main should be the source of truth for the tracker"; "Continue working
unattended, you can file reviews but dont block on their results". Debug
timings are nongating; nothing below is a timing claim.

Engineering evidence for each card is on the card:
[BPR-2157](../issues/60-done/BPR-2157-comments-absorb-notes-model-store-migration.md),
[BPR-2158](../issues/60-done/BPR-2158-comments-drawer-holds-local-comments.md).

## Owner verdicts, first look (attended session 12534e154303dd1913382442a83b8c8f, handed back 2026-11-13T17:44:00Z)

Frozen build from `beede2fc33b5` (`--attended-local-comments-look` on
`conventional-comments-v1`). Verified in the session's `session.json`, each
decision by the human actor with an empty note:

| Candidate | Outcome | Saved | Note |
|---|---|---|---|
| `comment-states-on-a-line` | **Accept** | 17:41:50Z | none |
| `composer-state-and-computer-icon` (including the line-drawn computer glyph in place of `QStyle::SP_ComputerIcon`) | **Accept** | 17:42:15Z | none |
| `notes-tab-retired` | **Accept** | 17:43:58Z | none |

These count toward [[BPR-2158]]'s owner look (the milestone's "owner's look
on a frozen build with all three states on one line"). The line-drawn computer
glyph is accepted as the "OS-matched computer glyph" the RFC allows. Host
68507 (its `tailscale serve` child 68517) was stopped with the attended-review
`stop`; nothing ran from the root, and
`/Users/jaeger/comments-absorb-notes-v001.1a7vqce8` was removed with
`freeze_attended_build.py remove`.

## Criteria so far (2026-11-13, second round)

| Criterion | Card | Evidence | Result |
|---|---|---|---|
| AC03 | [[BPR-2157]] | `application_review_submission_composition_public_path` (line and pull request Local comments through every submission, single comment and reply); `application_local_comments_composition_public_path` (line, thread reply, whole-file and pull request Local comments; Send in the thread hands only the written reply to publication; a submitted review received with its draft; no received body carries Local words). Publish response has no surface before milestone 4 | Pass |
| AC11 | [[BPR-2157]] | `sqlite_review_ledger_storage` on the shipped schema 11 fixture, interrupted at every statement | Pass |
| AC13 | [[BPR-2157]] | `application_local_comments_composition_public_path`: Status, the Comments tab, Next new, Threads' rows and label counts, Overview's per-file thread counts and Home's row unchanged by creating and deleting Local comments; `core_feedback_partition` | Pass |
| AC01, AC02, AC14 | [[BPR-2158]] | `qt_conversation_cards_view`, `qt_workbench_window_scale_{100,125,200}`; owner look 12534e15 accepted | Pass (Qt and look) |
| AC04 | [[BPR-2158]] | `application_local_comments_composition_public_path`: Keep local lowers the in-review count by one, no draft list holds it, it is Local at its line with its words; Make draft returns a not-in-review draft at the same side and line | Pass |
| AC05 | [[BPR-2158]] | Same course: held by a submitted review refused with its reason; Make draft where my draft stands refused with its reason; nothing changes; a published thread offers no state change | Pass |
| AC06 | [[BPR-2158]] | `qt_conversation_cards_view` `test_local_comments_edges_and_editing` and `test_local_comments_share_a_line`; the composed course's file chip and pull request count | Pass (other files and outdated in Qt only) |

## Owner look v002 filed (2026-11-13)

Session `3fefa3666ff9e2c2fc0943c4672f8b1e` (state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v002-2026-11-13/state`, cursor 12), frozen root `/Users/jaeger/comments-absorb-notes-v002._jfvvf9h` from `6ffcf4cce274`, launcher check `ATTENDED_LOCAL_COMMENTS_SHOWN:6`. One Accept/Reject each: `local-edge-chips`, `reply-local-state`, `local-reply-actions`, `local-edit-and-delete`. Not waited on.

## Milestone 2 closed (2026-11-13, third round)

`private_notes_panel` and both Notes tabs are deleted; every course and Qt test that drove the panel uses the Comments drawer. AC01's "no Notes tab" is now structural (`qt_workbench_window_scale_*` checks no tab, shown or hidden, is named Notes; `qt_drawer_parts_scale_100` and the Overview drawer test count five tabs). AC05's held-draft refusal is a disabled Local segment with its reason (`application_local_comments_composition_public_path`). AC06 adds compact chips in a narrow drawer (`qt_drawer_surfaces_fit_scale_{100,125,200}`). Evidence list: the [BPR-2158 card](../issues/60-done/BPR-2158-comments-drawer-holds-local-comments.md#progress-2026-11-13-third-round-the-notes-panel-retired). Owner look v002 is filed, verdict pending.

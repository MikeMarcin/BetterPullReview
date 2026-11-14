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
[BPR-2158](../issues/60-done/BPR-2158-comments-drawer-holds-local-comments.md),
[BPR-2159](../issues/60-done/BPR-2159-local-comments-found-and-counted.md).

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

## Owner verdict, look v002 (attended session 3fefa3666ff9e2c2fc0943c4672f8b1e, handed back 2026-11-13T19:32:44Z)

`local-edge-chips` (AC06): **Reject**. The other three candidates were not answered. The owner's note, verbatim:

> if I scroll until just 5 is visible at the top and then add a comment on line 5 then full pull comment composer isn't shown on the drawer. This might be related to that cutout space at the top for the pull request control?
>
> Also I want a real next comment prev comment control in that top bar in the comment drawer controls. Instead of this "next new comment" which I find useless in practice for navigating.
>
> 2nd img this local reply tooltip looks very detatched, i think this is the same old tooltip placement bug we've fixed a million times.
>
> 3rd img these lines between comments are slightly too subtle, compare it to slack's (the window behind).
>
> 4th img this compose looks like a mess and I don't understand why the mark draft button is unavailable here,  looks like we have 2 local comments on line 6 but neither are a draft and only this one i can't make a draft but the other one i can?
>
> Img 5 I dont understand why this comment says "pull request" and also other comments that are not focused need to show their line numbers.

The six fixes and their evidence are on the [BPR-2158 card](../issues/60-done/BPR-2158-comments-drawer-holds-local-comments.md#owner-look-v002-reject-session-3fefa366-handed-back-2026-11-13t193244z).

## Owner look v003 filed (2026-11-13)

The six v002 fixes and the narrow-drawer chips, one question each: session `acaf71a2f3272811be5bfa4f25222fb9` (state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v003-2026-11-13/state`, cursor 27), frozen root `/Users/jaeger/comments-absorb-notes-v003.as90oxpw` from `6e7bbc13a36f`. Not waited on. Evidence is on the [BPR-2158 card](../issues/60-done/BPR-2158-comments-drawer-holds-local-comments.md#look-v002-rework-2026-11-13-third-round).

## Milestone 3 closed (2026-11-13, fourth round)

[[BPR-2159]] Done at `b0ea75ce8`. Evidence is on the [BPR-2159 card](../issues/60-done/BPR-2159-local-comments-found-and-counted.md#progress-2026-11-13).

| Criterion | Card | Evidence | Result |
|---|---|---|---|
| AC07 | [[BPR-2159]] | `qt_conversation_finder_scale_{100,125,200}` `test_my_drafts`; `application_local_comments_found_public_path`: Local rows in their file's group; each of All, Published, Draft and Local shows exactly its rows with its count; under Local, Only new and Hide resolved hide none; a reopened pull request starts on All | Pass |
| AC08 | [[BPR-2159]] | `qt_pull_request_overview_page` `test_local_chip_and_my_review_by_state`; the composed course: the Local chip beside the drafts chip, the folder's sum, its activation lands on Threads on Local at the file's first, no Note column, My review by state with no Private notes row | Pass |
| AC09 | [[BPR-2159]] | `qt_review_submission_editor_scale_{100,125,200}`; the composed course: no Local comment pending or not in the review; Keep Local on a not-in-review row removes it; "N local comments stay on this computer" with Show, absent at zero | Pass |

AC13's course now counts Threads' threads, not its Local rows (Local comments are Threads rows by AC07, and no thread's count moves). Owner look v004 for milestone 3 is filed below, not waited on.

## Owner look v004 filed (2026-11-13)

Milestone 3's look, one Accept/Reject each: `threads-state-filter`, `threads-row-move`, `overview-local-chip`, `overview-no-note-column`, `my-review-by-state`, `finish-review-local-line`, `finish-review-keep-local`. Session `8ce7e45524c31cb13950f64babfa0ddf` (state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v004-2026-11-13/state`, cursor 21), frozen root `/Users/jaeger/comments-absorb-notes-v004.r230gldo` from `c5d5da4df652` (`--attended-local-comments-look` on `conventional-comments-v1`), launcher check `ATTENDED_FILE_SHOWN`. Not waited on.

## Milestones 4 and 5 closed (2026-11-13, fifth round)

[[BPR-2160]] Done at `a7db82152`; [[BPR-2161]] Done at `eeaeb2c20`. Evidence is on the cards ([BPR-2160](../issues/60-done/BPR-2160-one-keep-action-on-agent-surfaces.md#progress-2026-11-13), [BPR-2161](../issues/60-done/BPR-2161-local-pr-draft-notes-become-local-comments.md#progress-2026-11-13)).

| Criterion | Card | Evidence | Result |
|---|---|---|---|
| AC10 | [[BPR-2160]] | `application_agent_keep_action_public_path` (new, three runs): the answer about the pull request offers Keep local alone and keeps a Local comment on the pull request, then Go to comment; the answer about a selected line offers Draft comment alone and drafts there, then Go to draft; of six finding cards, Draft comment where the line takes a draft, Keep local past the file's end (a whole-file Local comment) and on a file the pull request does not change (on the pull request); the finding queue offers exactly one of the two. `application_explain_file_public_path`: Keep local, Go to comment. `qt_workbench_window_scale_*`, `qt_finding_queue_view`, `core_local_comment` (`decide_keep`) | Pass (owner look v005 pending) |
| AC12 | [[BPR-2160]] | Same course, all four settings of Include my local comments and Include my drafts: the turn request carries Local comments exactly with the first and the draft exactly with the second; `core_agent_context_supplement` (admission per kind, per session) | Pass |
| AC03, AC04, AC05, AC07, AC08, AC09, AC13 | | `application_local_comments_composition_public_path` and `application_local_comments_found_public_path` rerun at `a7db82152` | Pass |

Ask agent on my own unsent comment, one recording path: my draft's menu offers Ask agent about this draft, and the same course shows the agent receiving it as my own comment with its line, recorded in Private history by `record_note_press`. The local PR draft's notes read as Local comments (BPR-2161): `application_local_draft_desktop_public_path` checks the words, and a render of it is on the card.

## Owner look v005 filed (2026-11-13)

Milestone 4's look, one Accept/Reject each: `answer-one-keep-action`, `finding-card-keep-action`, `kept-local-go-to-comment`, `ask-agent-about-draft`. Session `4329741d72227f49c0ee4a0b94c4e9f1` (state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v005-2026-11-13/state`, cursor 12), frozen root `/Users/jaeger/comments-absorb-notes-v005.7ub_34li` from `ff4a39c437c8`, launcher check `ATTENDED_AGENT_SHOWN:findings=6`. Not waited on.

## Owner verdicts, looks v003 to v005 (handed back 2026-11-13)

Verified in each session's `session.json`; every decision is by the human actor.

Look v003, session `acaf71a2f3272811be5bfa4f25222fb9` (handed back 22:18:34Z):

| Candidate | Outcome | Saved | Note |
|---|---|---|---|
| `composer-in-view` | **Accept** | 20:49:15Z | none |
| `previous-next-comment` | **Accept** | 22:18:26Z | none |
| `local-reply-tooltip` | **Accept** | 21:07:51Z | none |
| `post-rules` | **Accept** | 22:17:59Z | none |
| `reply-composer-layout` | **Accept** | 22:18:32Z | none |
| `make-draft-reason` | **Reject** | 22:16:46Z | verbatim below |
| `closed-card-line-numbers` | **Accept** | 20:46:40Z | none |
| `no-pull-request-label` | **Accept** | 22:17:38Z | none |
| `compact-edge-chips` | **Accept** | 20:48:46Z | none |

> This is way too long taking too much space, just the tooltip is enough if it cant post but i still don't understand the rules myself because I didnt see any other comments on there there were drafts and i deleted the other notes on that line and it still couldnt post... unless this is just a fixture behavior.

Look v004, session `8ce7e45524c31cb13950f64babfa0ddf` (handed back 22:19:52Z): `finish-review-keep-local` **Reject** (22:19:51Z, one screenshot attached: a whole-file Local reply card whose "Make draft on line 12" button is cut off), note verbatim:

> I dunno what im looking for but this is getting cut off.

`threads-state-filter`, `threads-row-move`, `overview-local-chip`, `overview-no-note-column`, `my-review-by-state` and `finish-review-local-line`: no decision.

Look v005, session `4329741d72227f49c0ee4a0b94c4e9f1`: `answer-one-keep-action` **Reject** (22:21:08Z, one screenshot attached: the draft the answer made, open in its composer, with the suggestion chip and the agent glyph drawn over the body text), note verbatim:

> There's a lot of overlap on the UX here.

`finding-card-keep-action`, `kept-local-go-to-comment`, `ask-agent-about-draft`: no decision.

With the screenshot of the composer's foot (the computer and pencil segmented toggle), the owner also said:

> i dunno why we have 2 differnt UX for the local vs draft. This one seems much simpler and subtler than the big ass make draft button and I don't know what it's not sufficient.

> can we do pencil with slash when you're not allowed to make a draft

Hosts 98149/98159, 42869/42879 and 82478/82486 were stopped with the attended-review `stop`. No process ran from the roots, and `/Users/jaeger/comments-absorb-notes-v003.as90oxpw`, `-v004.r230gldo` and `-v005.7ub_34li` were removed with `freeze_attended_build.py remove`. The rejects open a rework round (below).

## Rework round 6 (2026-11-13): one state control

The owner's direction is an [amendment on the RFC](../rfcs/RFC-BPR-0127-comments-absorb-notes.md#owner-amendment-one-state-control-looks-v003-to-v005-2026-11-13); the four fixes, their causes and evidence are on [[BPR-2162]] ([rework round 6](../issues/60-done/BPR-2162-close-rfc-0127.md#rework-round-6-2026-11-13-one-state-control)). Integration pass at `8b16fbefb`: 37 suites and courses, all passing; the documentation gate is OK.

## Owner look v006 filed (2026-11-13)

Session `5dd1e60daeb1b265f39c6e3f84247a43`, state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v006-2026-11-13/state` (its URL is in `serve.log` beside it), cursor 36, host PID 9121 (Tailscale child 9129). Frozen from `dd8acd200f9b`, three roots, each launcher check passed:
- `/Users/jaeger/comments-absorb-notes-v006.csdaortz` (conventional-comments-v1, `--attended-local-comments-look`, `ATTENDED_LOCAL_COMMENTS_SHOWN:6`);
- `/Users/jaeger/comments-absorb-notes-agent-v006.6vrnvubx` (nested-folders-v1, `--attended-agent-look` with the suggestions fixture as `codex`, `ATTENDED_AGENT_SHOWN:findings=6`);
- `/Users/jaeger/discuss-busy-pr-v002.16ews593` (conversations-v1, `--attended-read-state --attended-busy-discuss-look`, `ATTENDED_FILE_SHOWN`).

One Accept/Reject each: `state-control-cards-replies`, `draft-refused-struck-pencil`, `finish-review-rows`, `agent-keep-one-control`, `threads-row-move` (its menu items now wear the glyphs), and v004's five unchanged surfaces asked again: `threads-state-filter`, `overview-local-chip`, `overview-no-note-column`, `my-review-by-state`, `finish-review-local-line`. For BPR-2171: `discuss-from-review-open-comment`, `discuss-only-this-file-column`. Not waited on.

## Owner verdicts, look v006 (session 5dd1e60daeb1b265f39c6e3f84247a43, handed back 2026-11-14T00:40:36Z)

Verified in `session.json`, by the human actor:

| Candidate | Outcome | Saved | Note |
|---|---|---|---|
| `agent-keep-one-control` | **Reject** | 00:35:51Z | "That's too subtle in the agent context." (one screenshot: a finding card in the Agent tab, its icon-only control at the card's foot) |
| `discuss-from-review-open-comment` | **Reject** | 00:40:33Z | "didn't work. seems like going to discuss (cmd+3) is deslecting the comment because its unselected when i go back to review (cmd+2) and discuss never scrolls or selects it." |

No decision: `state-control-cards-replies`, `draft-refused-struck-pencil`, `finish-review-rows`, `threads-row-move`, `threads-state-filter`, `overview-local-chip`, `overview-no-note-column`, `my-review-by-state`, `finish-review-local-line`, `discuss-only-this-file-column`. Asked again in v007.

Host 9121 (Tailscale 9129) stopped with the attended-review `stop`; nothing ran from the roots, and `comments-absorb-notes-v006.csdaortz`, `comments-absorb-notes-agent-v006.6vrnvubx` and `discuss-busy-pr-v002.16ews593` were removed with `freeze_attended_build.py remove`.

## Rework round 7 (2026-11-14)

AC10 amended by the owner (Draft alone on the agent's surfaces) and BPR-2171's Command-3 carry fixed; details on [[BPR-2162]] and [[BPR-2171]]. Integration pass: 33 suites and courses, all passing.

## Owner look v007 filed (2026-11-14)

Session `efe2da80b42f60b91e24da153dc8ea5b`, state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v007-2026-11-14/state` (URL in `serve.log` beside it), cursor 36, host PID 25283 (Tailscale child 25291). Frozen from `debf04a89a09`, three roots, each launcher check passed: `/Users/jaeger/comments-absorb-notes-v007.00rkixl7` (local comments look), `/Users/jaeger/comments-absorb-notes-agent-v007.md0c4l3i` (agent look), `/Users/jaeger/discuss-busy-pr-v003.yq18qmr0` (busy Discuss). One Accept/Reject each: `agent-draft-only` (new, AC10 as amended), `discuss-from-review-open-comment` (reworked), and v006's ten undecided asked again: `state-control-cards-replies`, `draft-refused-struck-pencil`, `finish-review-rows`, `threads-row-move`, `threads-state-filter`, `overview-local-chip`, `overview-no-note-column`, `my-review-by-state`, `finish-review-local-line`, `discuss-only-this-file-column`. Not waited on.

## Owner verdicts, look v007 (session efe2da80b42f60b91e24da153dc8ea5b, handed back 2026-11-14T01:50:15Z)

Verified in `session.json`, every decision by the human actor: all twelve **Accept**.

| Candidate | Outcome | Saved | Note |
|---|---|---|---|
| `state-control-cards-replies` | **Accept** | 01:49:35Z | none |
| `draft-refused-struck-pencil` | **Accept** | 01:40:05Z | “Aside: reproducible bug on comment drawer scrolling, going from image 2 and clicking the bottom comment gives image 1. (two screenshots)” |
| `finish-review-rows` | **Accept** | 01:48:43Z | none |
| `agent-draft-only` | **Accept** | 01:36:08Z | “Can we make this warning take less space, maybe it's just a warning sign on the row above and expands a disclosure on click or something? (one screenshot)” |
| `threads-row-move` | **Accept** | 01:50:06Z | none |
| `threads-state-filter` | **Accept** | 01:50:13Z | none |
| `overview-local-chip` | **Accept** | 01:49:09Z | none |
| `overview-no-note-column` | **Accept** | 01:49:24Z | none |
| `my-review-by-state` | **Accept** | 01:48:56Z | none |
| `finish-review-local-line` | **Accept** | 01:48:12Z | “Aside: i'm really confused at what i'm looking at here, why is this draft reply minimized like that? It also desn't seem to respect normal undo/redo rules. Draft reply seems very funky. / I feel like clicking off compose review into the diff or drawers or review bar or using the mode tabs should hide it. It feels more in the way than helpful staying open when i'm trying to navigate the app. (one screenshot)” |
| `discuss-from-review-open-comment` | **Accept** | 01:36:37Z | none |
| `discuss-only-this-file-column` | **Accept** | 01:37:14Z | none |

The asides are follow-up cards, allocated on main: [[BPR-2172]] (the composer's placement warning takes less space), [[BPR-2173]] (drawer scroll after opening the bottom comment; the screenshots are in [RFC-BPR-0127-v007-asides](RFC-BPR-0127-v007-asides/)), [[BPR-2174]] (my draft reply in a thread, and its undo), [[BPR-2175]] (Compose review hides when navigating).

Host 25283 (Tailscale 25291) stopped with the attended-review `stop`. The three roots were removed with `freeze_attended_build.py remove`. One fault: `comments-absorb-notes-v007.00rkixl7` was removed while an app the owner had launched from it after the hand-back (PID 56196) was still running. The process check ran in the same command as the removal and did not stop it. The process was left alone.

## Closeout state

Every criterion has direct engineering evidence and every owner look is accepted (looks 12534e15, v003 to v007). **RFC-BPR-0127 is closed** (Auto-Verified, 2026-11-14) by [[BPR-2162]].

---
schema: "implement-rfc.card.v1"
id: "BPR-2162"
title: "Verify and close RFC-BPR-0127"
status: "In Progress"
rfcs: ["RFC-BPR-0127"]
depends_on: ["BPR-2157", "BPR-2158", "BPR-2159", "BPR-2160", "BPR-2161"]
closeout_for: ["RFC-BPR-0127"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2157--BPR-2162--9d1250ec0d11.json","milestone_plan":{"id":"comments-absorb-notes-v1","path":"docs/rfcs/plans/comments-absorb-notes-v1/milestone-plan.json"}}
---

# BPR-2162: Verify and close RFC-BPR-0127

RFC base: `f85b216ecc7d4a7df2c7c5c263a0cf8be1f3d93a`.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **AC01:** The right drawer's tabs are Comments, Tour and Agent, and the Overview drawer has no Notes tab; N on a selected line opens the composer on Local in Comments. | Pass (engineering); owner look accepted (12534e15) | `d85b98576` |
| **AC02:** On a line that has a published thread, a Local reply and a draft reply of mine, one card shows all three in the order defined, each with its state's mark, and the line has one gutter mark whose accessible text names all three. | Pass (engineering); owner look accepted (12534e15) | `d85b98576` |
| **AC03:** With Local comments present on lines, in threads, on a file and on the pull request, Add single comment, the review's Submit, Add all, Send and Publish response write nothing of them: the fixture provider receives no Local body, and each is still Local afterwards. | Pass | `b79461670`; rerun at `a7db82152` |
| **AC04:** Keep local on a draft in the review lowers the review's pending count by one, removes it from every "not in this review" list, and keeps its text, line or range and reply target; Make draft on it returns a draft of the same kind at the same place, not in the review. | Pass | `64476cd4d`; rerun at `a7db82152` |
| **AC05:** Keep local on a draft held by a submitted review or being published, and Make draft where my draft already stands, are refused with their reasons and change nothing; no action offers a state change on a published comment. | Pass | `d85b98576`; rerun at `a7db82152` |
| **AC06:** A Local comment on the pull request, on a whole file, on another file, and on a line the shown comparison does not place each appears where "Where a Local comment can be" says, the last with its reason; none is shown at a line it was not written on, and Make draft on it is offered only as Make draft on line N with a line selected. | Pass (engineering; other file and outdated in Qt only); owner look v002 Reject reworked; v003 `make-draft-reason` Reject (8 others Accept), rework round 6 | `b92a35101` |
| **AC07:** Threads lists Local comments in their files' groups and its state filter's four choices show exactly the rows of that state with matching counts; the filter is not restored on reopening; under Local, Hide resolved and Only new hide no Local comment. | Pass (engineering); owner look v004 not decided, re-asked | `b0ea75ce8`; rerun at `a7db82152` |
| **AC08:** A file's Comments cell on Overview shows a Local chip with its count beside the drafts chip, a folder shows the sum, the chip's activation opens Threads on Local at that file's first, and Overview has no Note column or Private notes row. | Pass (engineering); owner look v004 not decided, re-asked | `b0ea75ce8`; rerun at `a7db82152` |
| **AC09:** Finish your review never lists a Local comment as pending or as not in the review; Keep Local on a not-in-review row removes that row; the Local line shows the count and is absent at zero. | Pass (engineering); owner look v004 `finish-review-keep-local` Reject, rework round 6 | `b0ea75ce8`; rerun at `a7db82152` |
| **AC10:** The Agent answer footer, finding card, finding queue and Explain this file card each offer one keep action; it makes a draft where a line can take one and a Local comment where none can, and its follow-up goes to what it made. | Pass (engineering); owner look v005 `answer-one-keep-action` Reject, rework round 6 | `a7db82152` |
| **AC11:** A profile whose ledger holds notes written before and after the path and line side tables opens after migration with one Local comment for each: equal text bytes, equal anchor facts, equal order; a note-era Go to note arrives at its comment; a migration interrupted at any statement and run again gives the same result. | Pass | `72a0d55af` |
| **AC12:** An agent request carries a Local comment only when the session's Local opt-in is set and a draft only when its drafts opt-in is set, each independently of the other. | Pass | `a7db82152` |
| **AC13:** Label counters, thread counts, the Comments tab's new count and Home are unchanged by creating, moving or deleting Local comments. | Pass | `b0ea75ce8`; rerun at `a7db82152` |
| **AC14:** Each state is announced in words to assistive technology ("Local comment, only on this computer", "Draft, not in your review", "Draft, in your review"), and the state control, Make draft and Keep local are reachable and operable by keyboard alone. | Pass (engineering); owner look accepted (12534e15) | `d85b98576` |

## Outcome and work

Reconcile the live documents the RFC's owner amendments name (RFC-BPR-0007, 0011, 0012, 0023, 0049, 0085, 0086, 0094, 0101, 0102, 0107, 0108, 0110, 0111, 0113, 0115, 0116, 0117, 0118, 0121, 0125) with what was built; update `docs/architecture/product-contract.md` and the drawer and private-material sections of `docs/architecture/system-architecture.md`; record the three owner looks; run the documentation gate.

## Progress (2026-11-13)

Evidence by criterion: the [RFC-BPR-0127 ledger](../../verification/RFC-BPR-0127.md) and the cards [BPR-2157](../60-done/BPR-2157-comments-absorb-notes-model-store-migration.md), [BPR-2158](../60-done/BPR-2158-comments-drawer-holds-local-comments.md), [BPR-2159](../60-done/BPR-2159-local-comments-found-and-counted.md), [BPR-2160](../60-done/BPR-2160-one-keep-action-on-agent-surfaces.md), [BPR-2161](../60-done/BPR-2161-local-pr-draft-notes-become-local-comments.md).

Reconciled:
- Each amended RFC (0007, 0011, 0012, 0023, 0049, 0085, 0086, 0094, 0101, 0102, 0107, 0108, 0110, 0111, 0113, 0115, 0116, 0117, 0118, 0121, 0125) carries its RFC-BPR-0127 owner amendment from the import.
- `docs/architecture/product-contract.md` (private workflow state names Local comments) and `docs/architecture/system-architecture.md` (three states, the drawer's tabs, the ledger's Local comments, `decide_make_draft`, `decide_keep`, the two opt-ins, one Ask agent path) follow what was built. `docs/ux/ai-experience.md`, `docs/ux/workbench-specification.md`, `docs/ux/shared-action-buttons.md` and `docs/architecture/ai-agent-boundary.md` were updated with BPR-2160.
- The local PR draft's vocabulary (BPR-2161) is shown in a rendered local draft (`local-draft-local-comments.png`, from `application_local_draft_desktop_public_path`).
- The documentation gate is OK.

**Not closed.** Every criterion has direct engineering evidence. The RFC's closeout waits on three owner looks, filed and not waited on:
- v003 (session `acaf71a2…`, the v002 rework: AC06 and the drawer);
- v004 (session `8ce7e455…`, AC07 to AC09);
- v005 (session `4329741d…`, AC10 and Ask agent).

On their verdicts, record them here and on the ledger. If all are Accept, finish this card and mark RFC-BPR-0127 closed; a Reject is a new rework round.

## Owner verdicts on v003 to v005 (2026-11-13)

Recorded verbatim on the [ledger](../../verification/RFC-BPR-0127.md#owner-verdicts-looks-v003-to-v005-handed-back-2026-11-13). v003: eight Accept, `make-draft-reason` Reject. v004: `finish-review-keep-local` Reject, six undecided. v005: `answer-one-keep-action` Reject, three undecided. The owner asked for one state control everywhere (the composer's Local/Draft toggle) and a pencil with a slash where Draft is not allowed. Hosts stopped, roots removed. Rework round 6 follows.

## Rework round 6 (2026-11-13): one state control

Frontend-design was loaded first. The owner's direction is recorded as an amendment on [RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md#owner-amendment-one-state-control-looks-v003-to-v005-2026-11-13). Lane commits `bd40efb4e`, `ae46036fe`, `8b16fbefb`.

1. **One state control everywhere** (`comment_state_toggle`): the composer's two joined icon segments, the computer (Local) and the pencil (Draft), now on the Local comment's card, my Local reply, my draft's card, my draft reply (beside its Reply field), the reply composer, Finish your review's not-in-review rows, the agent answer footer, finding cards and Explain this file. Threads' row menu and the finding queue wear the same glyphs. A refused move disables its segment; a refused Draft is the pencil struck through (`conversation_glyph::edit_refused`), its reason the tooltip alone, at most 60 characters ("Your draft on this line already stands", "Your draft reply in this thread already stands", "Select a line to make it a draft there", "Your submitted review holds this draft", "No line here takes a draft", "A whole file takes no draft"). The reason written under the card is gone.
2. **Make draft blocked with no draft in sight (v003).** Reproduced in the composed path and in the frozen look's own sequence. The rule counted nothing deleted or hidden: the look makes the second Local reply a draft reply, and a thread holds one draft reply of mine, so the first Local reply's Make draft was refused. But that draft reply showed only as the folded Reply field, "Continue your reply", which reads as an empty field, so there was no draft to be seen, and deleting the line's Local comment changed nothing. Fixed by showing it: the Reply field of a thread holding my draft reply shows the pencil, "Draft reply" and its words, with the state control beside it, whose Local keeps it local and frees the thread. `application_local_comments_composition_public_path` now runs the look's sequence: second Local reply, Draft, the first one's Draft struck through with the short tooltip and no words in the card, the draft reply visible as a draft; Local on the draft reply, then the first one's Draft is offered (`LOCAL_COMMENTS_REPLY_REFUSAL Your draft reply in this thread already stands`).
3. **Cut off (v004).** The screenshot was the whole-file Local reply's "Make draft on line 12" pill cut by the drawer's width; the state control replaces it. Finish your review's rows carry the control beside Add, and `qt_review_submission_editor_scale_{100,125,200}` checks every row at 300, 380 and 560 px: the control and Add whole, the row's words ending before them, Local keeping that draft local.
4. **Overlap (v005).** The screenshot was the draft the agent's Draft comment opened: the Enhance glyph and the label chip were drawn over the last lines of the text, and the header had two identical expand glyphs. Cause: the composer's field refitted its height on every resize, and showing its scrollbar rewrapped the text into a different height, so it swung between 220 and 167 px and was left with a minimum above its maximum; the layout then overlapped the rows. The field now fits once at a time and shows its scrollbar only past its tallest, and tells the card when its height changes (`test_composer_notice_keeps_rows_apart` failed first with the field at 79-299 and the tool row at 280-305). A draft being written has one way to Discuss (Open in Discuss), not also Show in Discuss. The answer's keep action is the state control; Go to comment or Go to draft stands in its place once kept.

Evidence (dev, Debug, macOS): `qt_conversation_cards_view`, `qt_workbench_window_scale_{100,125,200}`, `qt_review_submission_editor_scale_{100,125,200}`, `qt_drawer_surfaces_fit_scale_{100,125,200}`, `qt_conversation_finder_scale_{100,125}`, `qt_agent_finding_cards_{retained,restart}`, `qt_explain_file_card_scale_{100,150,200}`, `qt_finding_queue_view`, `qt_composed_github_scale_100`, `qt_button_faces`; `application_local_comments_composition_public_path`, `application_local_comments_found_public_path`, `application_agent_keep_action_public_path`, `application_explain_file_public_path`, `application_review_submission_composition_public_path`, `application_local_draft_desktop_public_path`, `application_agent_chat_*_contract`, `application_discuss_busy_public_path`; `application_composition_source_contract`, `application_review_round_trip_composition_contract` (app source pin resealed; it had not been resealed since BPR-2160), `architecture_boundaries`. Offscreen renders of the local-comments look and the agent look (`--attended-agent-draft-first`) checked by eye.

### Owner look v006 filed (2026-11-13)

Session `5dd1e60daeb1b265f39c6e3f84247a43`, state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v006-2026-11-13/state` (its URL is in `serve.log` beside it), cursor 36, host PID 9121 (Tailscale child 9129). Frozen from `dd8acd200f9b`, three roots, each launcher check passed:
- `/Users/jaeger/comments-absorb-notes-v006.csdaortz` (conventional-comments-v1, `--attended-local-comments-look`, `ATTENDED_LOCAL_COMMENTS_SHOWN:6`);
- `/Users/jaeger/comments-absorb-notes-agent-v006.6vrnvubx` (nested-folders-v1, `--attended-agent-look` with the suggestions fixture as `codex`, `ATTENDED_AGENT_SHOWN:findings=6`);
- `/Users/jaeger/discuss-busy-pr-v002.16ews593` (conversations-v1, `--attended-read-state --attended-busy-discuss-look`, `ATTENDED_FILE_SHOWN`).

One Accept/Reject each: `state-control-cards-replies`, `draft-refused-struck-pencil`, `finish-review-rows`, `agent-keep-one-control`, `threads-row-move` (its menu items now wear the glyphs), and v004's five unchanged surfaces asked again: `threads-state-filter`, `overview-local-chip`, `overview-no-note-column`, `my-review-by-state`, `finish-review-local-line`. For BPR-2171: `discuss-from-review-open-comment`, `discuss-only-this-file-column`. Not waited on.

Every criterion holds apart from these pending looks. On their verdicts: all Accept finishes this card and closes RFC-BPR-0127; a Reject is another round.

### Look v006 verdicts

`agent-keep-one-control` **Reject**: "That's too subtle in the agent context." The nine RFC-BPR-0127 questions besides it: no decision, asked again. Verbatim on the [ledger](../../verification/RFC-BPR-0127.md#owner-verdicts-look-v006-session-5dd1e60daeb1b265f39c6e3f84247a43-handed-back-2026-11-14t004036z).

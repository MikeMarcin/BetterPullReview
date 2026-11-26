---
schema: "implement-rfc.card.v1"
id: "BPR-2407"
title: "The tour side chat draws its runs in the one transcript from a core projection: what a turn proposed, its check card, its focus, the revision and the refused parts are attachments, the feedback waiting for the draft is a turn, and rebuild() is gone"
status: "Done"
rfcs: ["RFC-BPR-0130"]
depends_on: ["BPR-2404", "BPR-2405"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","reservation":"docs/issues/_reservations/BPR-2407--BPR-2407--f9ab89ba8ca9.json","mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"369aa1eba","origin":"RFC-BPR-0130 stage 4 (draft card DRAFT-0008 of the attended review 3cd822672efb50572b10ab10747889ba); owner, 2026-11-24: one chat component for all three chats","related_cards":["BPR-2404","BPR-2405","BPR-1995","BPR-1930","BPR-2154"],"related_rfcs":["RFC-BPR-0130","RFC-BPR-0100","RFC-BPR-0120"],"acceptance":["AC1","AC2","AC3","AC4"],"stage_3_card":"the stage-3 card of RFC-BPR-0130 (card-rfc130-stage-3.md); add its ID to depends_on at allocation"}
acceptance: []
---

# BPR-2407: The tour side chat draws the shared transcript

## Why

Owner, 2026-11-24, accepting one chat component (attended review
`3cd822672efb50572b10ab10747889ba`): "We should also more closely reuse
implementation between the side chats and agent chat so that we're not
constantly having to double implement changes." RFC-BPR-0130 stage 4: the
tour side chat adopts the one transcript; proposal and revision cards become
attachments. Visible: nothing.

### What was there (stage 3)

`tour_side_chat::rebuild()` (about 680 lines) emptied the column and made
every turn again on each change: each run's bubble (with its quoted passage
and the prompt it was sent from), steered bubbles, the reply, the check card,
why it did not finish, the ending row with Copy, Resend and Delete, the
activity block, the focus line, the whole tour's revision card, the refused
parts, the unchanged line, each proposed part's card, the waiting feedback,
the Draft Tour line and the suggested prompts. A proposal's Edit text drew the
whole column again.

## Change

1. **Core** `project_tour_thread` (`chat_tour_transcript.cpp`, new): a turn
   each run of the thread, identity `tour-run:<id>`; a reply, or why it did
   not finish ("Didn't finish: …", "It didn't finish.", "Stopped. Nothing in
   the tour changed.") with the run's own words for Copy
   (`chat_turn::FailureDetail`); the one ending line (the agent that ran it,
   "The tour agent" where none is known); the ended turn's actions
   (`decide_agent_turn_actions`, Delete only where the chat offers it); what
   the run did (`chat_activity`); and attachments: a local draft's check card
   (`chat_tour_check`, under the reply: `chat_attachment_place::after_reply`),
   the focus it set (`chat_tour_focus`), the whole tour's revision
   (`chat_tour_revision`: its parts and their decisions), the refused parts
   (`chat_tour_refused`), the update line under the newest answer that changed
   nothing (`chat_tour_unchanged`) and each part a part's turn proposed
   (`chat_tour_proposal`). The whole tour's feedback waiting for its draft is
   the last turn, with `chat_turn::QuestionNote` "Sent when the draft is
   ready, to revise it." `tour_agent_session` is unchanged.
2. **Qt** `chat_transcript_view`: a chat dresses a bubble above its words
   (`DressBubble`: the quoted passage, the prompt's name and Show prompt),
   shows a reply's references as links (`ReplyBody`), and stops a run from the
   activity block's quiet notice (`Stop`); a turn draws the question's note,
   the run's activity (live while it runs, folded once it ended, a fresh block
   when it settles), attachments under the reply or after the ending, and
   Resend enabled as the chat says (`set_resend_enabled`);
   `remake_attachments` draws a card again when the host says its own controls
   must go back (a declined build grant); every bubble in the column is
   fitted.
3. **Tour side chat**: `rebuild()`, its per-turn vectors (bubbles, replies,
   endings, activity blocks, Copy, Resend, Delete, readers) and the live line
   are gone; `present_thread()` projects the open thread and presents it, and
   keeps the chat's own items: the invitation (lead), the Draft Tour line and
   the suggested prompts (trail). A proposal's Edit text turns its card
   between the text and its source in place. Accessors (`bubble`, `reply`,
   `status_line`, `copy_reply_button`, `resend_button`, `delete_button`,
   `activity_view`, `accept_button`, `proposal_reader`, `revision_card`,
   `refused_line`, `unchanged_line`, `focus_line`, `waiting_feedback`,
   `draft_status_line`) read the transcript. A local draft's discussion is the
   same widget and follows without work of its own.
4. **Courses**: `core_chat_transcript` (the tour projection: a part's runs,
   the one ending line, Copy and what the run did, a failure's words and
   Copy text, Resend and Delete only while nothing runs and where Delete is
   offered, the update line on the newest answer alone, a part's proposal and
   its decision changing that turn alone, the whole tour's focus, revision and
   refused parts in order, the waiting feedback, a check card under its reply,
   a running run); `qt_chat_transcript_view` (the tour chat: a turn that
   settles with a proposal, and the proposal decided, keep the other turn's
   widgets and the settled reply). `qt_tour_side_chat` compares the bubble,
   the reply and the ending line in the chat's coordinates (the ending stands
   in its own row now).
5. **Docs**: `docs/architecture/review-tours.md`, `src/adapters/qt/AGENTS.md`.

No `src/app` file changes; the sealed production manifest is untouched.

Not in this card: the Agent tab (stage 5), one notice (stage 6; the tour's
notice and its moved-head card stay as they are).

## Acceptance

- AC1 (RFC AC06 for the tour side chat). The tour side chat draws its turns
  in `chat_transcript_view`; while a turn settles and a proposal is decided,
  the widgets of the other turns are the same objects; `rebuild()` is gone.
- AC2 (DRAFT-0008). Accept, Update this stop from this chat, Resend, Delete
  and Copy behave as before.
- AC3 (DRAFT-0008). A local draft's discussion follows without its own work.
- AC4. The tour chat's tests and end-to-end courses and the composed course
  in the native look pass with no visible regression; format and tidy on
  touched units.

## Evidence

Revision `369aa1eba`, macOS arm64 Debug, `ctest --test-dir build/dev -R '^<row>$'`.

- AC1: `qt_chat_transcript_view` passed (the tour chat: a turn that settles
  with a proposal keeps the other turn's widgets, one `settled` change names
  it; deciding the proposal changes its card alone and keeps the settled
  reply's reader); `core_chat_transcript` passed in dev and in the
  `core-only` preset.
- AC2: `qt_tour_side_chat` (Accept, Edit text, Discard, Update this stop from
  this chat, Resend, Delete, Copy, Show prompt, the revision card's Apply all,
  Review each and Discard, the focus line's Undo, the refused parts, the
  Draft Tour line), `qt_tour_chat_in_tab`, `qt_tour_agent_session_view`,
  `qt_tour_side_chat_width_scale_100`, `_125`, `_200`, `qt_stick_to_bottom`,
  `qt_side_chat_tab_and_send_scale_100`, `_200`, `qt_item_side_chat`,
  `qt_workbench_window_scale_100` passed.
- AC3: `application_local_draft_tour_desktop_public_path` and
  `application_tour_chat_queue_local_draft_public_path` passed.
- AC4: `application_tour_chat_queue_stop_public_path`,
  `_whole_tour_public_path`, `application_follow_up_public_path`,
  `application_composed_drawer_fit_scale_100` and `_200` (39 states) passed.
  The same course under the native look (Cocoa, kept off screen, light, the
  display's 200%): "composed-drawer-fit: 39 states, 0 faults, 50 transcript
  edges drawn, dpr 2, cocoa/macos". `architecture_boundaries`,
  `architecture_boundary_contract` passed.
- `scripts/verify_documentation.py --root .` (DOCUMENTATION_OK).
- Format: `_coding-standard/scripts/format.py` (clang-format 22) over every
  touched C++ file. Tidy over `chat_tour_transcript.cpp`,
  `chat_transcript_view.cpp`, `tour_side_chat.cpp` and the two tests: clean.

Renders, native look, light, 200%, under
`/private/tmp/claude-501/-Users-jaeger-Documents-GitHub-BetterPullReview/78f52950-62e8-4272-b0ec-08fec46ca508/scratchpad/rfc130-renders/`:
before is `stage-3-after/cocoa-macos/`, after is `stage-4-after/cocoa-macos/`
(`after-light-200-<state>-<width>.png`). All 274 renders are byte-identical,
the tour chat's states (`tour-chat-*`, `tour-stop-chat-*`) among them.

## Owner judgement

- None asked: nothing changes on screen.

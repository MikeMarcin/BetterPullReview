---
schema: "implement-rfc.card.v1"
id: "BPR-2160"
title: "One keep action on the agent surfaces, and the wording sweep"
status: "Done"
rfcs: ["RFC-BPR-0127"]
depends_on: ["BPR-2158"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2157--BPR-2162--9d1250ec0d11.json","milestone_plan":{"id":"comments-absorb-notes-v1","path":"docs/rfcs/plans/comments-absorb-notes-v1/milestone-plan.json"},"criteria":["AC10","AC12"],"origin":"imported 2026-11-13 from comments-absorb-notes-v1 (RFC-DRAFT-0001); cut from the draft's suggested milestones"}
---

# BPR-2160: One keep action on the agent surfaces, and the wording sweep

Part of [RFC-BPR-0127](../../rfcs/RFC-BPR-0127-comments-absorb-notes.md); milestone `one-keep-action` of the [plan](../../rfcs/plans/comments-absorb-notes-v1/MILESTONES.md).

## Scope

The Agent answer footer, finding cards (Draft all loses its noted case), the finding queue, the Explain this file card and What changed each offer one keep action: Draft comment where a line can take one, Keep local otherwise, then Go to what it made. Agent private context: Include my local comments and Include my drafts, two opt-ins with the same bounds. Ask agent on my own unsent comment through one recording path. Wording in Settings (Enhance), RFC-BPR-0094's chip, RFC-BPR-0101/0102/0125 prompts and RFC-BPR-0086/0121 tiles.

## Acceptance

- **AC10:** The Agent answer footer, finding card, finding queue and Explain this file card each offer one keep action; it makes a draft where a line can take one and a Local comment where none can, and its follow-up goes to what it made.
- **AC12:** An agent request carries a Local comment only when the session's Local opt-in is set and a draft only when its drafts opt-in is set, each independently of the other.

## Notes

Ends in an owner look (Accept or Reject). How the agent surfaces themselves are reached follows RFC-BPR-0128 (Ask); this card changes only what they keep.

## Progress (2026-11-13)

Frontend-design was loaded first. Lane commits `a9346a9fd`, `3efbc109c`, `a7db82152`.

Built:
- **Core decides where it can be kept.** `core::decide_keep` (in `local_comment.hpp`): a thread reply or a line that takes a draft makes a draft (**Draft comment**). Otherwise it is kept local (**Keep local**): on the file when it names one the pull request changes, else on the pull request. `core::agent_turn_actions` has one `Keep` in place of Save as note and Draft comment.
- **Agent answer footer.** One keep action in one place: Draft comment where the question was about a thread or selected lines (`decide_answer_draft` decides at the send), Keep local otherwise, a Local comment on the pull request. They then read Go to draft and Go to comment. An answer about the pull request no longer drafts a pull request comment.
- **Finding cards.** Draft comment where the finding's line takes a draft; Keep local where it is past its file's end or on a file the pull request does not change. Keep local opens the file first and keeps a whole-file Local comment, or keeps it on the pull request. The card then says "Kept local." with Go to comment. A card re-reads where it can be kept on each refresh, so a card whose line is found gone offers Keep local. Draft all passes a kept card over and says "N kept local".
- **Finding queue.** Draft comment and Keep local (once Convert to local draft comment and Add to private notes); only the one `decide_keep` picks from the verdict is shown. Kept on a file, the Local comment names the file.
- **Explain this file.** Keep local, then Go to comment.
- **Ask agent on my own unsent comment, one path.** A draft's menu offers Ask agent about this draft beside the Local comment's own. Both reach the question as "Your local comment on …" or "Your draft on …". Both are recorded in Private history by `record_note_press` at the send, read through one `own_comment_asked`.
- **Opt-ins and wording.** Side chat: Include my local comments; Include my drafts. Agent launch panel: the "Private notes" row is "Local comments" ("Local comment #N"), and "Private draft" is "Draft". The refusal names local comments. Enhance writing reads "Comments and replies". The note prompt reads "Explain the code this comment is on" (its ID is unchanged). Enhance's writing kind names a local comment. The Overview drawer's private tab is "Private history". `docs/ux/ai-experience.md`, `docs/ux/workbench-specification.md`, `docs/ux/shared-action-buttons.md` and `docs/architecture/ai-agent-boundary.md` follow.

Evidence (dev, Debug, macOS):
- `core_local_comment` (`decide_keep`), `core_agent_draft_all` (a kept card is passed over and counted), `core_agent_turn` (one keep action), `core_prompt_macro`, `core_explain_file`: pass.
- `qt_workbench_window_scale_{100,125,200}`: an answer about the pull request offers Keep local alone, which becomes Go to comment. An answer about a conversation offers Draft comment alone, which becomes Go to draft, and again Draft comment once its draft is released. A card offers Draft comment, and Keep local alone once its line is known not to take a draft; "Kept local." with Go to comment.
- `qt_finding_queue_view`: an unanchored finding offers Keep local, not Draft comment. `qt_explain_file_card_scale_*`, `qt_agent_finding_cards_{retained,restart}`, `qt_prompt_macro_chips`, `qt_agent_launch_panel`, `qt_side_chat_panel`, `qt_conversation_cards_view`, `qt_local_draft_window`: pass.
- `application_agent_keep_action_public_path` (new, composed, nested-folders-v1, the suggestions fixture as the only Codex), three runs:
  - **AC10.** The answer asked on Overview offers Keep local alone, keeps a Local comment on the pull request, and becomes Go to comment, which keeps nothing more. Of six cards, the placed one offers Draft comment and drafts. The one past its file's end offers Keep local and keeps a whole-file Local comment. The one on a file the pull request does not change keeps a Local comment on the pull request. The answer about a selected line offers Draft comment alone, drafts at that line and becomes Go to draft. The finding queue offers exactly one of the two per finding (1 Draft comment, 5 Keep local).
  - **Ask agent.** My draft's menu attaches it ("Your draft on src/engine/renderer.cpp line 5"); asked, the agent gets it as my own comment with its line.
  - **AC12.** With Local comments and a draft present, all four opt-in settings: the turn request carries Local comments exactly when Include my local comments is set, and the draft exactly when Include my drafts is set. The launch then refuses mutable prose and directs to Private history (RFC-BPR-0023), unchanged.
  - Nothing reaches GitHub.
- `application_explain_file_public_path` (Keep local, Go to comment), `application_review_tour_composition_public_path`, `application_local_comments_{composition,found}_public_path`, `application_local_draft_desktop_public_path`, `application_agent_{chat,invocation}_composition_contract`, `application_composition_source_contract` (pin resealed), `architecture_boundaries`: pass. Format gate passes.

How the agent surfaces are reached is unchanged (RFC-BPR-0128's).

## Owner look v005 filed (2026-11-13)

Session `4329741d72227f49c0ee4a0b94c4e9f1` (state `/Users/jaeger/.attended-review-state/comments-absorb-notes-v005-2026-11-13/state`, cursor 12, host PID 82478, Tailscale serve child 82486), frozen root `/Users/jaeger/comments-absorb-notes-v005.7ub_34li` from `ff4a39c437c8` (`--attended-agent-look` with the suggestions fixture as `codex` on nested-folders-v1, `src/engine/renderer.cpp`). The launcher check reached `ATTENDED_AGENT_SHOWN:findings=6`. One Accept/Reject each: `answer-one-keep-action`, `finding-card-keep-action`, `kept-local-go-to-comment`, `ask-agent-about-draft`. Not waited on. After hand-back: read from cursor 12, record verbatim here and on the ledger, stop host 82478 with the attended-review `stop`, and remove the root with `freeze_attended_build.py remove` once nothing runs from it.

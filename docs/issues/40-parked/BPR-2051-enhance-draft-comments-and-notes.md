---
schema: "implement-rfc.card.v1"
id: "BPR-2051"
title: "Enhance on draft comments and notes, each surface with its own editable prompt"
status: "Parked"
rfcs: ["RFC-BPR-0106", "RFC-BPR-0107"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2051--BPR-2051--efa66a8a01d3.json","origin":"owner request, 2026-11-10"}
depends_on: []
acceptance: [{"criterion": "BPR-2051 scope 1", "outcome": "pass", "revision": "821307015704fc7051eb29197973731350555d53", "evidence": {"kind": "command", "detail": "Comment and note Enhance build and decode through the tour writing operation", "command": "ctest --preset dev -R 'application_writing_delivery|core_review_preparation'"}}, {"criterion": "BPR-2051 scope 2", "outcome": "pass", "revision": "821307015704fc7051eb29197973731350555d53", "evidence": {"kind": "command", "detail": "Runs only when pressed; a suggestion applied only by Use, one Undo; nothing to GitHub", "command": "ctest --preset dev -R 'qt_enhance_control|application_enhance_comment_note_public_path'"}}, {"criterion": "BPR-2051 scope 3", "outcome": "pass", "revision": "821307015704fc7051eb29197973731350555d53", "evidence": {"kind": "command", "detail": "Enhance wears the Agent accent; the window inventory stays one set", "command": "ctest --preset dev -R 'qt_enhance_control|qt_workbench_window'"}}, {"criterion": "BPR-2051 scope 4", "outcome": "pass", "revision": "821307015704fc7051eb29197973731350555d53", "evidence": {"kind": "command", "detail": "Own prompt per surface with default and restore; schema 25", "command": "ctest --preset dev -R 'qt_settings_window|sqlite_workbench_settings_storage|application_enhance_comment_note_public_path'"}}, {"criterion": "BPR-2051 scope 5", "outcome": "pass", "revision": "821307015704fc7051eb29197973731350555d53", "evidence": {"kind": "command", "detail": "A run uses the prompt saved at the press; a save meanwhile neither changes nor discards it", "command": "ctest --preset dev -R application_enhance_comment_note_public_path"}}, {"criterion": "BPR-2051 scope 6", "outcome": "pass", "revision": "821307015704fc7051eb29197973731350555d53", "evidence": {"kind": "command", "detail": "Stable purpose IDs, versions and digests; RFC-BPR-0107 amended", "command": "ctest --preset dev -R application_writing_delivery"}}]
parked_on: "human-review"
---

# Enhance on draft comments and notes, each surface with its own editable prompt

## Origin

The owner, 2026-11-10: "I want to add enhance to draft comment and notes like
we have it on tour stops. It might need an icon design to fit into the ux" and
"Enhance needs to be based on a prompt we can edit in agent settings".

## Scope and acceptance

1. The draft comment editor (a new comment and a reply) and the private note
   editor offer Enhance. It calls the operation a tour stop's Enhance uses
   (the writing contract, the structured rewrite result and its decoder), not
   a second one.
2. Enhance runs only when pressed. Its result is a suggestion shown beside
   the text, which the user takes or discards; it is never applied without
   that choice. Enhance writes nothing to GitHub.
3. The Enhance control wears the Agent accent (it starts a run), and the
   accent inventory stays one set with the run controls.
4. Each surface (draft comment, note, tour stop) has its own Enhance prompt
   in Settings › Agents & context › Built-in prompts, with a built-in default
   and a one-click restore. A tour stop's Enhance stops borrowing the
   Generate comment prompt.
5. A run uses the prompt saved when Enhance was pressed: saving a changed
   prompt while it runs neither changes nor discards it.
6. The prompts fit RFC-BPR-0107's exchange: they are writing-hook purposes in
   the same list and store, recorded in that RFC.
7. Owner look on a frozen build whose fixture already holds a draft comment
   and a note: one icon choice among two or three drawn options (light and
   dark), Accept/Reject for placement, Accept/Reject for the prompt editor.

Load frontend-design before the Qt work.

## Evidence

Record: [RFC-BPR-0106 verification, Enhance on comments and notes](../../verification/RFC-BPR-0106.md#enhance-on-comments-and-notes-bpr-2051-2026-11-10).
Frontend-design was loaded before the Qt work. Product candidate `821307015`.

Purpose IDs defined for RFC-BPR-0107: `enhance.tour-stop`, `enhance.comment`,
`enhance.note` (and, for the existing hooks, `tour.problem`, `tour.solution`,
`tour.outline`, `tour.stop-comment`, `review.feedback`).

## Owner look (filed, not waited on)

Attended session `ac6e3a324ea3472583fe1d6e82c300f6`, state
`/Users/jaeger/.attended-review-state/enhance-v001-2026-11-10/state`,
cursor 9, three candidates ready and protected: `BPR-2051-icon` (choose one
of three drawn icons, Light and Dark), `BPR-2051-placement` and
`BPR-2051-prompt-editor` (Accept/Reject), on the frozen composed course of
`821307015` in `/Users/jaeger/enhance-v001.dzk_0xf4` (app SHA-256
`f91330402280…`, course `31349bdc97de…`; launcher `--check` passed; the
course's `--attended` look reached its ready line offscreen). The URL stays
in the session state.

## Owner hand-back, session `ac6e3a32…` (2026-11-10)

Read with `attended_review.py status` (sequence 93); host 77843/77881
stopped, root `enhance-v001.dzk_0xf4` removed.

| Candidate | Verdict | Note |
|---|---|---|
| `BPR-2051-icon` | **Reject** (`1ecc48cc`) | "something more like nib and spark" (attached: a fountain-pen nib with a four-point spark cut into it) |
| `BPR-2051-placement` | Accept (`edda65a4`), with changes | Place it consistently across the whole app surface; a much shorter tooltip that adapts to the context, e.g. "Enhance writing for this comment with agent." / "Enhance writing for the selected text with agent." |
| `BPR-2051-prompt-editor` | Accept (`9ea23ba3`), with changes | The bottom prompt list is good; the top part of Agents & context "is ugly as sin and needs a redesign"; rename "Enhance a note" to "Enhance writing" |

Rework (next round): nib-and-spark icon; one placement on every surface;
context tooltips; one `enhance.writing` prompt for comments and notes (the
tour stop keeps `enhance.tour-stop`); a redesigned top of Agents & context.

## Rework after the owner's look (2026-11-10, `90785ae34`)

Frontend-design was loaded before the Qt work.

- Icon: three nib glyphs drawn in `enhance_control.cpp`, light and dark:
  `nib_cut` (a solid nib with the spark cut out of its body, after the
  owner's picture; the product default until the owner chooses),
  `nib_outline` (outline nib, slit and hole, spark above) and `nib_spark`
  (solid nib, spark above). The tour fields' Enhance wears the same nib.
- Placement: Enhance is the first control of the row directly under the
  field on every surface: a new draft comment and a reply (before the label
  chip), a private note (before Delete and Ask agent), a tour stop and the
  Problem and Solution (first in their helper rows, as before).
- Tooltip: "Enhance writing for this comment with agent." (reply, note,
  stop, the Problem, the Solution in their words); "Enhance writing for the
  selected text with agent." while text is selected; it follows the
  selection (`enhance_tooltip`, `enhance_button::set_selection`). New
  surface `enhance_surface::reply`.
- Prompt: one `enhance.writing` prompt ("Enhance writing", version
  `enhance-writing-v1`, `resources/review-preparation/enhance-writing.md`)
  for comments, replies and notes; a stop keeps `enhance.tour-stop`. Each run
  carries a `writing-kind` source saying whether the text is a comment or a
  private note. Built-in prompts lists it under "Comments and notes" with
  Review feedback. `bpr tour hooks` names it `enhance-writing`.
- Migration (profile schema 27): the comment's override is kept; a profile
  that changed only the note's keeps the note's for every surface; the
  note's column (`writing_enhance_note`) is retired and always empty. The
  retired purpose IDs `enhance.comment` and `enhance.note` name
  `enhance.writing`. Neither had shipped outside this lane.
- Settings › Agents & context top: three options behind
  `set_agents_context_top` (a look switch, default "Next request group"):
  1 the next request's choices in one titled group with a one-line caption;
  2 Agents first (no per-request choices; one line says the Agent tab holds
  them); 3 a one-line summary with Change… opening the same choices. Each
  moves the long account of what the agent receives into a tooltip; the
  working copy is a check box like the draft's.

Tests: `qt_enhance_control` (tooltips per surface and selection, default
glyph), `qt_settings_window` (prompt list, Enhance writing's default,
version, override and Reset), `core_review_preparation`,
`application_writing_delivery` (default, version, purpose IDs and retired
aliases, override), `sqlite_workbench_settings_storage` (v24, v25 and the
v26 fold in its four cases), `tour_cli_composition`,
`application_profile_tour_handoff_public_path`,
`application_enhance_comment_note_public_path` (the stub answers only when
the `writing-kind` source names the comment or the note) and
`application_review_tour_composition_public_path`.

---
schema: "implement-rfc.card.v1"
id: "BPR-2051"
title: "Enhance on draft comments and notes, each surface with its own editable prompt"
status: "Done"
rfcs: ["RFC-BPR-0106", "RFC-BPR-0107"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2051--BPR-2051--efa66a8a01d3.json","origin":"owner request, 2026-11-10"}
depends_on: []
acceptance: [{"criterion":"BPR-2051 scope 7","outcome":"pass","revision":"3285ec887627035f0d1ac76b1d6f3521ea158f0e","evidence":{"kind":"observation","detail":"Owner looks accepted: icon (ab426ddc, session 5ea0ce22), placement and tooltip (a955d55c), prompt list (9ea23ba3, session ac6e3a32); the rejected Agents & context top replaced by the roster, Accept 4fec6767 (BPR-2051-roster-top) and the footer Accept 585b2013 (BPR-2051-agent-footer), session 7c9206759d935f663d6a13ce0e008afc, frozen from 3285ec887"}}]
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

## Owner hand-back, session `5ea0ce22…` (2026-11-11)

Read with `attended_review.py status --after 18` (handed back at sequence
49); host 78319/78332 stopped, root `owner-look-v003.w0k3mfr3` removed with
`freeze_attended_build.py remove`.

- `BPR-2051-icon-v2`: Select "1. Nib with the spark cut out" (`ab426ddc`),
  already the default (`CurrentStyle`); nothing changes.
- `BPR-2051-placement-tooltip`: Accept (`a955d55c`).
- `BPR-2051-agents-top`: **Reject** (`06aceb8f`): "These are all ugly we
  need a deep-worker to come up with some mockups using the frontend-design
  skill". Not implemented here; it goes to the next layout round, which
  starts from mockups made with the frontend-design skill.

The card stays parked on `human-review` until a new Agents & context top is
accepted.

## Owner decision: "A + footer 1" (2026-11-11)

The owner chose the mockups "A · Roster" for the top of Agents & context and
"footer 1 · In the status bar" for the pull request window ("A + footer 1"),
from the frontend-design mockups made after the `06aceb8f` rejection. In the
owner's words, earlier: "As we make agent a bigger part of this tool, I'm
thinking we might need ot move agent picker out of the individual drawers
and into an always present footer on the PR window." The accepted design is
recorded at [docs/design/BPR-2051-agents-roster-and-footer](../../design/BPR-2051-agents-roster-and-footer/README.md).

## Layout round (2026-11-11, `f51063b5b`, `be50866a1`)

Frontend-design was loaded before the Qt work.

- Footer (`agent_footer`, in the window's status bar, hidden on Home): the
  one picker over the Agent tab's source (the Agent tab, tour chat, Enhance
  and Draft the tour use its choice); the run with where it started, what it
  does, its time and Stop; several runs as a count whose list has a Stop
  each; quiet tone after 120 s without a frame; a finished run's ending for
  six seconds; the session cost summed over the Agent tab and the tour lane
  (chat, Enhance, drafting), reset when another pull request opens. One
  fixed height, so the bar never moves. Setup is the footer's state: the
  picker says what is missing in the failed check's colour and Agent
  settings… opens Settings.
- Removed: the Agent tab's in-field picker (the tab borrows the footer's for
  /model), the Tour tab's agent bar and its "Drafting help needs the agent"
  band. Enhance's Stop now stops the tour lane it runs on (it stopped the
  Agent tab's host).
- Roster: rings and words from each agent's kept check (`EntryChecks`,
  `agent_entry_checks`, kept across restarts); a row opens its sheet (the
  agent dialog); its menu checks it, moves it or removes it; drag reorders;
  Check all (`agent_setup_host::check_all`) checks each agent in turn and
  keeps the chosen one chosen. The next request's choices left the page.
  Import/Export: RFC-BPR-0107 places them; not shown until it is built.
- RFC-BPR-0104 amended (grant line moves to the picker's menu), RFC-BPR-0101
  (session total in the footer) and RFC-BPR-0103 (running summary in the
  footer); drawers keep detail.

Tests: `qt_agent_footer` (new: states, Stop per run, picker in place across
states, setup, one choice), `application_agent_setup_host` (Check all keeps
each result; a row's check; removal drops it), `sqlite_agent_setup_storage`
(checks across restart), `qt_settings_window`, `qt_agent_setup_view`, 39
Qt tour/agent/window rows, 55 application/contract rows (composition source
and round-trip contracts, `architecture_boundaries`, network-isolation
manifest, agent chat/finding, tour courses), `clang-tidy` on the new and
changed host and Qt sources, format check, documentation gate.

Renders (Cocoa, off screen, light and dark): the roster and every footer
state, in the attended session below.

Filed, not waited on (the URL stays in the session state's `serve.log`): session `7c9206759d935f663d6a13ce0e008afc`, state `/Users/jaeger/.attended-review-state/layout-round-v001-2026-11-11/state`, cursor 12, host PID 73758 (its `tailscale serve` child 73768), frozen root `/Users/jaeger/layout-round-v001.5q22yahv` from `3285ec887` (app SHA-256 `97d0861e19dc…`, launcher check reached `ATTENDED_TOUR_SHOWN`). Candidates: `BPR-2051-roster-top` and `BPR-2051-agent-footer` (looks: `agent_footer --look`, `better_pull_review_qt_settings_window_tests --agents-roster-look` in the root's build, and the frozen app). The card stays parked on `human-review`.

## Owner hand-back, session `7c920675…` (2026-11-11)

Read from `session.json` (handed back at sequence 27); host 73758/73768
stopped, root `layout-round-v001.5q22yahv` removed with
`freeze_attended_build.py remove`.

- `BPR-2051-roster-top`: Accept (`4fec6767`).
- `BPR-2051-agent-footer`: Accept (`585b2013`), note "Why is it red?". The
  frozen build had no agent set up, so the footer showed its setup state, which
  drew the picker in the failed check's (error) red. Needing setup is not a
  failure; the post-review round of `agent-scratch` (2026-11-11) draws it in
  the calm secondary tone with an outline, and keeps red for a check that ran
  and failed.

Every look the card asks for is accepted (the icon, placement and prompt list
earlier; the roster that replaced the rejected Agents & context top and the
footer now). The card is Done.

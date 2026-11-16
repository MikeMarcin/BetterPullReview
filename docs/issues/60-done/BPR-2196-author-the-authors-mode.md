---
schema: "implement-rfc.card.v1"
id: "BPR-2196"
title: "Author: the author's mode"
status: "Done"
rfcs: ["RFC-BPR-0112"]
metadata: {"criteria":["AC07","AC09"],"design":"docs/design/local-draft-workflow-v2/README.md","design_session":"273f0aafcc76fd284605b51649bbdbdf","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2196--BPR-2196--fefe6c514ce0.json"}
depends_on: ["BPR-2032", "BPR-2033", "BPR-2034"]
acceptance: [{"criterion":"AC07","outcome":"pass","revision":"737e06d81","evidence":{"kind":"command","command":"ctest --preset dev -R 'core_author_mode|qt_workbench_window_scale_100|application_local_draft_(workflow|author|publication|tour)_desktop_public_path'","detail":"Author is offered only to the owner of the pull request or local draft, on Ctrl+5 (Command-5 on macOS), and opens per case (local draft and owned draft in Author, owned open pull request on Overview); its main area holds This update and the file pane (Edit, one pane, two panes), its drawer opens on Workflow with filled seen and outlined Reported marks and one handoff at the bar's end; the core decides availability, opening mode, handoff and checklist. The owner accepted Author in look workflow-v002 (session 1d68e30d0ac7b53a07163a1f10777bdf, candidate BPR-2196-author-mode, decision fd71fc9f, outcome accept, no note). The same look rejected the Workflow tab's horizontal pan on BPR-2035's question; that is engineered under BPR-2035, not here."}},{"criterion":"AC09","outcome":"pass","revision":"737e06d81","evidence":{"kind":"command","command":"ctest --preset dev -R 'core_author_mode|application_local_draft_workflow_desktop_public_path'","detail":"Agent rows in Author's checklist wear the agent accent and script rows do not; a companion's report is an outlined Reported mark, never filled. The owner accepted the accent in look workflow-v002 (candidate BPR-2033-agent-accent-on-actions, decision accept, no note)."}}]
---

# BPR-2196: Author: the author's mode

## The mode is named Author (owner, 2026-11-15)

The accepted design called the fifth mode Revise. The owner renamed it
before it landed: "I changed my mind, make it Author". The switcher reads "Author ⌘5" on macOS and
"Author Ctrl+5" on Windows and Linux; code uses `author`
(`review_modality::author`, `core::author_mode::author`). Nothing else in
the design changed.

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0112](../../rfcs/RFC-BPR-0112-optional-workflow-extensions.md)
(AC07's native author workbench, AC09's accent), as the owner reshaped the
author's workbench in the accepted design
`local-draft-workflow-v2` (`docs/design/local-draft-workflow-v2/README.md`
on the lane, merged from `claude/local-draft-workflow-mockups` at `042e035a9`)
(session `273f0aafcc76fd284605b51649bbdbdf`, Accepted 2026-11-15, all six
recommended defaults; note: "we got rid of the notes tab so not sure why the
mockup shows it", so Author's drawer has no Notes tab). Filed under
RFC-BPR-0112 because it replaces that RFC's rejected Workflow area
(`workflow-v001`) and its closeout (BPR-2035) asks the owner about it.

- **Author is a fifth mode** in the window's mode switcher, on Command-5,
  present only when the viewer owns the pull request or the local draft.
  On anyone else's pull request it is absent and Overview, Review, Discuss
  and Threads keep Command-1 to Command-4.
- **Main area:** the author's bench, moved out of Review: This update (the
  files that would go and the commit box) and the selected file. The Edit
  view and the Changes tab leave Review for Author.
- **Drawer:** opens on Workflow, the first tab: the author's checklist. A
  filled mark is something BetterPullReview saw; an outlined Reported mark is
  something a companion reported. Who runs each row is said once; the agent
  accent is on agent rows only. A local draft's publish steps move into
  Workflow, so a local draft has no Status tab until its pull request exists;
  Status keeps its single meaning.
- **Handoff:** one button at the right end of Author's bar: Create Draft PR…
  (or Hand Off Plan… when the workflow's publisher is chosen), Ready for
  review, or Publish update…. Ready for review stays on Overview's title line.
- **Wording:** never "sent back"; "Publish update…", never "Publish
  response". Unbuilt parts of RFC-BPR-0122, 0123 and 0124 are left out.
- **Defaults:** a local draft and an owned draft pull request open in Author,
  an owned open pull request on Overview, and afterwards a window reopens in
  the mode it was left in; Author remembers the comparison, file and drawer
  tab per pull request; without a worktree Author asks for one once.
- **Architecture:** the core decides availability, the opening mode, the
  handoff and the checklist; the host performs; Qt presents.

## Intended verification

`core_author_mode` (availability owned and not owned, opening mode per
case, handoff, checklist marks); a Qt course for the switcher and Command-5;
the RFC-BPR-0112 courses updated to the new surfaces; owner look
`workflow-v002` through BPR-2035.

## Owner look workflow-v002 filed (2026-11-15)

Session `1d68e30d0ac7b53a07163a1f10777bdf`, state
`/Users/jaeger/.attended-review-state/workflow-v002-2026-11-15/state` (URL in
that directory's `serve.log`), cursor 12, host PID 57962 (Tailscale child
57974). Frozen root `/Users/jaeger/workflow-v002.1z4xyif5` from `6867e30ef06e`
(executable SHA-256 `0fb49352ee56…`; the launcher's check reached
`ATTENDED_LOCAL_DRAFT_WORKFLOW_SHOWN` with 5 actions and started no real
agent). Four questions, one each: `BPR-2196-author-mode` (Author and the
switcher, including the file pane's three views), `BPR-2035-workflow-tab`
(AC07), `BPR-2032-workflow-import-preview` (AC02's preview) and
`BPR-2033-agent-accent-on-actions` (AC09). If all four are accepted,
BPR-2196 is Done, and BPR-2035 closes RFC-BPR-0112 (Auto-Verified) once
BPR-2197 is Done too.

## Owner look workflow-v002 handed back (2026-11-15): Done

Session `1d68e30d0ac7b53a07163a1f10777bdf` handed back at sequence 45.
`BPR-2196-author-mode`: **Accepted**, no note. The other three questions are
recorded on [BPR-2035](../10-backlog/BPR-2035-close-rfc-0112.md): the agent
accent was accepted; the Workflow tab was rejected for panning sideways
("It's panning horizontally again. I don't like my drawers doing that. This
should be a rule by now."), which is engineered under BPR-2035 with a drawer
fit guard; the import preview was rejected and goes to a frontend-design
pass. Host 57962 (Tailscale child 57974) stopped with the attended-review
`stop` command; root `/Users/jaeger/workflow-v002.1z4xyif5` removed with
`freeze_attended_build.py remove`. BPR-2196 is Done at `737e06d81`.

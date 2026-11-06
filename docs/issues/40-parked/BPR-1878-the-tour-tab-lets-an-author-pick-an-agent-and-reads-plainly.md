---
schema: "implement-rfc.card.v1"
id: "BPR-1878"
title: "The Tour tab lets an author pick an agent and says plainly what each control does"
status: "Parked"
parked_on: "human-review"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-05: 'I see absolutely no way to pick an agent on the tour tab … This whole tab is a disaster.'"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "d1ea88005476da7f1a4ce705dfc083a65996dce9", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_help (tests/adapters_qt/review_tour_agent_help_tests.cpp): with two agents offered by an agent_launch_panel, the Tour tab's Drafting agent menu shares its model and shows its choice; choosing in the Tour tab changes the Agent tab's selected_profile and a choice in the Agent tab shows in the Tour tab; Draft from PR context asks for Problem help; with no agent offered the menu gives way to 'No agent is set up, so drafting is off' and Set up an agent, which dispatches set_up_agent (Settings, Agents & context); manual writing stays open; a read-only draft shows no agent help. Passes. The app supplies the line's reasons (tour_agent_help: cannot write tour text, another agent writes instead, busy) and refreshes it on agent progress."}}, {"criterion": "AC2", "outcome": "pass", "revision": "d1ea88005476da7f1a4ce705dfc083a65996dce9", "evidence": {"kind": "command", "detail": "qt_review_tour_agent_help asserts in both the agent and no-agent states that no visible button reads Next, Recover explanation, Use selected description text or Enhance text, that the Problem helpers and Next carry tooltips, and that no notice says 'Choose an available agent'. Labels now: Draft from PR context, Enhance problem statement / solution / comment, Enhance selection, Generate comment (RFC-BPR-0043 names), Copy selected description text, Next: Solution / Next: Stops. request_writing's refusal names its real cause and only controls the tab shows. qt_review_tour_steps, qt_scoped_prose_editor, qt_review_tour_publication_working, qt_review_tour_stop_view, qt_drawer_parts_scale_{100,125,200}, qt_drawer_surfaces_fit_scale_{100,125,200}, application_review_tour_composition_public_path, application_review_tour_writing and application_review_tour_writing_wrapper pass."}}, {"criterion": "AC4", "outcome": "pass", "revision": "d1ea88005476da7f1a4ce705dfc083a65996dce9", "evidence": {"kind": "command", "detail": "python3 scripts/verify_documentation.py --root . printed DOCUMENTATION_OK; coding-format-check passed (clang-format 22.1.8)."}}]
---

# BPR-1878: The Tour tab lets an author pick an agent and says plainly what each control does

## Observed

The owner, 2026-11-05, authoring a tour for their own pull request. The Tour
tab shows:

- A banner: "Choose an available agent and wait for its current request.
  Manual editing is available." There is no agent picker anywhere on the tab,
  so the instruction cannot be followed.
- Step 1 "Problem" with a text box "What problem does this change solve?",
  then buttons **Recover explanation**, **Use selected description text** and
  **Next**, then steps 2 "Solution" and 3 "Stops", and "Saved locally. Nothing
  published." with **Preview and publish**.
- The owner could not tell what "Recover explanation" or "Use selected
  description text" do, and "Next" made no sense in that position.

## Change

Load the frontend-design skill and redesign the authoring surface so a first
time author understands it without reading docs:

- An agent picker on the tab (or a clear link to the one in the Agent tab)
  wherever agent help is offered; if no agent is available, say why and how to
  get one, in one line. Never instruct the author to choose something the tab
  does not show.
- Rename or replace "Recover explanation" and "Use selected description text"
  with labels that say the effect (e.g. draft from the PR description, draft
  with the agent), or drop them if redundant. Each control's tooltip says what
  it does.
- Step navigation that reads as a guided flow, or no "Next" at all if every
  step is visible; the current step and what is left are obvious.
- Keep manual editing first-class.

## Acceptance

- AC1. With an agent available, the author can choose it from the Tour tab's
  flow and start agent help; with none, the tab says why. Qt test.
- AC2. No control on the tab names an action the tab does not offer; every
  button label states its effect.
- AC3. The owner accepts the redesigned look (attended review, one question).
- AC4. `python3 scripts/verify_documentation.py --root .` passes.

## Diagnosis

- There was no agent picker on the Tour tab. Writing asked
  `writing_agent()`: the Agent tab's chosen agent if it can write tour text,
  else the first added agent that speaks Claude Code's stream JSON. The only
  menu was in the Agent tab (and Settings mirrored it). The banner "Choose an
  available agent and wait for its current request" covered three different
  refusals: no agent, a draft this window does not own, and an agent busy
  with another request.
- **Recover explanation** (the helper on an empty Problem or Solution)
  copied the PR description's own `Problem`/`Solution` section verbatim when
  there was exactly one, and otherwise asked the agent for a draft. With text
  in the field it read "Enhance text"; with a selection, "Enhance selection".
  RFC-BPR-0043 already named these "Draft from PR context" and "Enhance
  problem statement/solution/comment"; the code had drifted from it, so the
  RFC needed no amendment.
- **Use selected description text** copied the selection in Overview's
  description Source view into the field, keeping its source span as the
  field's baseline for updating the description in place later.
- **Next** went to the next step. It sat in the helpers' row.

## Implementation

Commit `d1ea88005` (frontend-design skill loaded for the redesign):

- `review_tour_editor`: agent help at the top of the tab. A Drafting agent
  menu shares the Agent tab's model (`share_agent_choices`) and hands a
  choice to that menu, which still decides the pick in core (BPR-1823). The
  line under it comes from the host (`set_agent_help_source`), with defaults
  when the host has nothing to add. With no agent offered: the reason and
  Set up an agent (`tour_editor_action::set_up_agent`). It is hidden on a
  read-only draft. Helper labels and tooltips follow RFC-BPR-0043. The copy
  helper is renamed. Next is named for its destination and sits on its own
  row.
- `github_application`: `tour_agent_help()` says when the chosen agent
  cannot write tour text, when another agent writes instead, or when it is
  busy, refreshed on agent progress. The refusal notice names its real
  cause. `set_up_agent` opens Settings at Agents & context.
- `workbench_window`: shares the Agent tab's menu with the Tour tab.
- `main.cpp`: `--attended-tour-authoring AGENT` opens a new tour's first
  step with a Claude stream-JSON fixture agent chosen.
- `docs/architecture/review-tours.md` describes the agent help and labels.

## Attended review (AC3)

- Frozen build of `d1ea88005` at
  `/Users/jaeger/.codex/attended-builds/bpr1878-tour-authoring-v001.30yciz1v`
  (executable SHA-256
  `bd3bc6b2f3ad88d7d1f3b15b9750a80e9bfe663e3e9254d6634f0e41871033f6`),
  fixture `conventional-comments-v1`, `--attended-open-file
  src/engine/renderer.cpp --attended-tour-authoring
  {build}/tests/end_to_end/scoped_writing_agent`. The launcher's own
  offscreen check reached `ATTENDED_TOUR_SHOWN`. Launch:
  `python3 /Users/jaeger/.codex/attended-builds/bpr1878-tour-authoring-v001.30yciz1v/launch.py`.
- New short session `3c2a6241b7bf337b8ac749c89d496abd`, state
  `/Users/jaeger/.codex/attended-review/bpr1878-v001-99RcVJ/state`, repo
  root the implementing worktree, host started with `serve --tailscale`.
  One candidate, `BPR-1878-tour-authoring` (revision `d1ea88005-v001`), with
  one Accept/Reject question. Status: ready, awaiting the owner.


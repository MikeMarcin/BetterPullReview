---
schema: "implement-rfc.card.v1"
id: "BPR-2196"
title: "Revise: the author's mode"
status: "In Progress"
rfcs: ["RFC-BPR-0112"]
metadata: {"criteria":["AC07","AC09"],"design":"docs/design/local-draft-workflow-v2/README.md","design_session":"273f0aafcc76fd284605b51649bbdbdf","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2196--BPR-2196--fefe6c514ce0.json"}
depends_on: ["BPR-2032", "BPR-2033", "BPR-2034"]
---

# BPR-2196: Revise: the author's mode

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0112](../../rfcs/RFC-BPR-0112-optional-workflow-extensions.md)
(AC07's native author workbench, AC09's accent), as the owner reshaped the
author's workbench in the accepted design
[local-draft-workflow-v2](../../design/local-draft-workflow-v2/README.md)
(session `273f0aafcc76fd284605b51649bbdbdf`, Accepted 2026-11-15, all six
recommended defaults; note: "we got rid of the notes tab so not sure why the
mockup shows it", so Revise's drawer has no Notes tab). Filed under
RFC-BPR-0112 because it replaces that RFC's rejected Workflow area
(`workflow-v001`) and its closeout (BPR-2035) asks the owner about it.

- **Revise is a fifth mode** in the window's mode switcher, on Command-5,
  present only when the viewer owns the pull request or the local draft.
  On anyone else's pull request it is absent and Overview, Review, Discuss
  and Threads keep Command-1 to Command-4.
- **Main area:** the author's bench, moved out of Review: This update (the
  files that would go and the commit box) and the selected file. The Edit
  view and the Changes tab leave Review for Revise.
- **Drawer:** opens on Workflow, the first tab: the author's checklist. A
  filled mark is something BetterPullReview saw; an outlined Reported mark is
  something a companion reported. Who runs each row is said once; the agent
  accent is on agent rows only. A local draft's publish steps move into
  Workflow, so a local draft has no Status tab until its pull request exists;
  Status keeps its single meaning.
- **Handoff:** one button at the right end of Revise's bar: Create Draft PR…
  (or Hand Off Plan… when the workflow's publisher is chosen), Ready for
  review, or Publish update…. Ready for review stays on Overview's title line.
- **Wording:** never "sent back"; "Publish update…", never "Publish
  response". Unbuilt parts of RFC-BPR-0122, 0123 and 0124 are left out.
- **Defaults:** a local draft and an owned draft pull request open in Revise,
  an owned open pull request on Overview, and afterwards a window reopens in
  the mode it was left in; Revise remembers the comparison, file and drawer
  tab per pull request; without a worktree Revise asks for one once.
- **Architecture:** the core decides availability, the opening mode, the
  handoff and the checklist; the host performs; Qt presents.

## Intended verification

`core_author_revise` (availability owned and not owned, opening mode per
case, handoff, checklist marks); a Qt course for the switcher and Command-5;
the RFC-BPR-0112 courses updated to the new surfaces; owner look
`workflow-v002` through BPR-2035.

---
id: "RFC-BPR-0099"
title: "Keep tour publishing in reach, report each step and choose its form"
status: "Auto-Verified"
metadata: {"origin":"owner notes, 2026-11-10, notes 2, 3, 6 and 7","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
---

# Keep tour publishing in reach, report each step and choose its form

## Problem and grounded current behavior

The owner reports that publishing a tour gives no progress, that its
refusals do not look important, that the app asks for a choice it could
make itself, that the publish control and the model picker scroll out of
reach, and that the tour on Overview needs fixing
([notes 2, 3, 6 and 7](assets/v0-5-0-prerelease-targets-v1/SOURCE_NOTES.md)). In the inspected code:

- The Tour tab's editor is one column inside a scroll area
  (`workbenchTourScroll`,
  [workbench_window_central_surface.cpp](../../src/adapters/qt/src/workbench_window_central_surface.cpp)).
  Its last two rows are `Foot`, holding "Preview and publish", and
  `AgentBar`, holding the agent, model and effort pill
  ([review_tour_editor.cpp](../../src/adapters/qt/src/review_tour_editor.cpp)).
  The outline has a fixed height policy and no scroll bar of its own, so a
  long route pushes both rows off screen. This is read from the layout code,
  not observed running.
- The publish controls proper (Preview, Publish tour, Resume, Check status,
  Stop) exist only on the tour page's right column, reached through that
  bottom button.
- The author chooses a publication mode from a list: "Keep single-review
  publication", "Publish a mixed tour (non-atomic)", "Publish with
  compatibility comments (non-atomic)". Yet `plan_tour_destination` in
  [review_tour_destination.cpp](../../src/core/src/review_tour_destination.cpp)
  already decides, per stop, which representation can carry it.
- `staged_publication_stages` in
  [staged_review_publication.cpp](../../src/application/src/staged_review_publication.cpp)
  lists the steps. The projection carries a phase and that list, but no
  per-step state, so the list shown under "Sending, in order:" does not
  advance.
- Refusals are plain sentences in the placeholder colour, for example "This
  connection does not support the operation. Keep the draft locally or use a
  supported connection."
  ([tour_publication_plan.cpp](../../src/app/src/tour_publication_plan.cpp))
  and "Not published. This connection does not support this operation. Your
  draft is kept."
  ([github_cli_consent.cpp](../../src/adapters/qt/src/github_cli_consent.cpp)).
  Neither names the stops or the operation.
- The Overview summary
  ([overview_tour_summary.cpp](../../src/adapters/qt/src/overview_tour_summary.cpp))
  draws Problem and Solution as plain text although tour text is authored as
  Markdown, shows each stop as a title with a readiness word, and gives an
  author one action, "Continue in Review", with no publication state.

## Proposed behavior and boundaries

The frames are in the design round:
[Tour tab](../design/v0-5-0-targets-v1/index.html#tour),
[publishing](../design/v0-5-0-targets-v1/index.html#publish) and
[Overview](../design/v0-5-0-targets-v1/index.html#overview).

### Three bands in the Tour tab

The Tour tab has a head, a route and an agent bar. Only the route scrolls.

The **head** states where the tour stands and carries its one publication
action:

| State | Head text | Action |
|---|---|---|
| Not publishable yet | Private draft, with the stop count and how many are still to accept | Publish…, disabled, its tooltip naming what is missing |
| Publishable | Private draft, with the stop count, all accepted | Publish… |
| Publishing | Publishing, step N of M, with a determinate bar | Details |
| Stopped | Publishing stopped at step N of M, in the error role | Resume, Details |
| Published, unchanged | Published | On GitHub |
| Published, changed | Published, with the count of changes not on GitHub | Update on GitHub…, On GitHub |

Accepting a stop is defined in
[RFC-BPR-0106](RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md).

Publish… and Update on GitHub… open the tour page with its preview already
computed. Details opens the same page. The bottom "Preview and publish"
button is removed.

The **agent bar** keeps the agent, model and effort pill and the Draft the
tour action, at the bottom of the tab at every route length.

**Amended 2026-11-11 ([[BPR-2051]], owner decision "A + footer 1").** The
agent bar is gone. The agent, model and effort are chosen in the window's
agent footer, and Draft the tour is in the route's head, as
[RFC-BPR-0106](RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md)
records. The Tour tab has two bands: the head and the route.

**Established in implementation, 2026-11-10 ([[BPR-1991]]).** Until
[RFC-BPR-0106](RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md)
lands, a stop counts as accepted when it passes the rule a first publication
already applies to every stop (exact source, no attention, an accepted
suggestion, an explanation current at the observed head); the core names it
`tour_stop_publishable` and `assess_tour_publication` uses it. "Changes not on
GitHub" counts each stop that is new or moved, each stop whose title or
explanation differs from its published text, each published stop removed from
the route, and Problem and Solution where their published text is known; a
route only reordered counts the published comments its update would edit.
The save state ("Saved locally", a refused edit) stays a quiet line just above
the agent bar, outside the route. The head and the route's band are the same
in the drawer and on the tour page.

### The plan chooses the form

The publication mode list is removed. The plan uses the first of these that
can carry every stop on the current connection:

1. one review;
2. a mixed tour: one review plus standalone file or conversation comments;
3. compatibility comments.

The publication panel states the choice in one sentence with the stops that
forced it, for example "Publishes in 11 steps. Seven stops go in one review.
Stops 4 and 8 sit outside the lines GitHub shows in this diff, so they are
sent as file comments." When the chosen form is not one review, the panel
offers **Keep it to one review instead**, which lists the stops to move and
changes nothing until the author moves them. The description checkbox and
its meaning are unchanged.

**Established in implementation, 2026-11-10 ([[BPR-1992]]).** A stop outside
the lines the diff shows is sent as a file comment whenever the connection's
recorded facts allow one, without the author asking stop by stop; the per-stop
Publish as file comment remains as an explicit request. Keep it to one review
instead lists the stops to move, each opening its stop, and shows the moves
each already has (Re-pin to current head, Move to nearest change). The `bpr
tour` CLI keeps its explicit choice. An update chooses the same way for the
stops it adds.

### Steps that advance

The core's publication projection gives each step one state: waiting,
sending, sent, failed, or not sent. The panel shows the steps as a checklist
in those states, and the head shows the same count. Consecutive sent steps
of one kind may be summarised ("Stops 1 to 7"). A step is marked sent only
after GitHub confirmed it.

**Established in implementation, 2026-11-10 ([[BPR-1992]]).** The steps are
the publication's writes in the order it makes them: the pending review with
the stops it carries, each standalone comment, each stop's links, the
submission, an update's edits to the published comments, and the description.
The projection lives beside the staged plan and record it reads, in the
application's pure publication module (`staged_publication_steps`,
`staged_publication_confirmed`), because those types are the application's;
it takes no effect. A step counts as sent only once the durable record holds
GitHub's returned identity or confirmed edit. Before each write the host
hands the projection the record it authorised, on the decision lane, so the
next step reads as sending; consecutive sent link steps read as one row.

### Failures that read as failures

A refusal or failure is a banner in the error role at the top of the
publication panel, never a status line. It states, in this order: what
stopped and at which step; why, naming the stops and the operation; what is
on GitHub and what is not; and the actions that move forward, the first of
them primary. The failed step in the checklist carries the provider's short
reason.

Because the plan chooses a form the connection can carry, "does not support"
remains only for a tour no form can carry on this connection. That refusal
is shown before anything is sent and names the stops, the missing operation
and both ways out: move those stops, or use another connection.

Resume, Check status and Stop keep their RFC-BPR-0041 meanings.

**Established in implementation, 2026-11-10 ([[BPR-1992]]).** A failure after
part of the tour was sent, a stop, a restart that finds a part-sent record and
a published tour whose description failed all say "Publishing stopped at step
N of M"; Resume previews the captured plan and resumes it, and Check status
comes first while a write's outcome is unknown or the record conflicts. A
refusal before the first write reads "Not published", with what nothing was
sent and Preview again (Check status for a conflicting pending review).
"Does not support" no longer appears in the provider failure text.

### The tour on Overview

The summary renders Problem and Solution as Markdown, as the Tour tab does.
A stop row shows its title, its file and lines, and the first sentence of
its explanation, and opens that stop in Review. Its state line and actions
are the Tour tab head's, so an author can publish or update from Overview
and sees a stopped publication there.

### Boundaries

- Publication semantics, recovery, the staged order and the GitHub format
  are RFC-BPR-0040, RFC-BPR-0041 and RFC-BPR-0042, unchanged. This RFC
  changes who picks the form, what is reported and where the controls are.
- The `bpr tour` CLI and its explicit `--compatibility` option are
  unchanged. Only the application stops asking for the form.
- No write is added, reordered or retried automatically.

### Settled choice: what is wrong on Overview

Note 3 does not say what is wrong. The Overview section above fixes what the
code shows to be rough: Markdown drawn as plain text, stop rows that lead
nowhere, and no publication state or action for an author. Asked on
2026-11-10 what the note meant, the owner answered "Your fixes cover it".
Those three faults are therefore the scope of the Overview work, and its
criterion is ready.

## Acceptance criteria

- **AC01:** With a route long enough to scroll, the head's publication
  action and the agent bar remain visible and operable at the top and bottom
  of the Tour tab while the route scrolls between them.
  (Amended 2026-11-11 at closeout: the owner's layout decision "A + footer 1"
  ([[BPR-2051]]) removed the agent bar; the agent picker is the window's
  footer, outside the tab, and Draft the tour is in the route's head. The
  criterion is met by the head's publication action staying visible and
  operable at the top while the route scrolls.)
- **AC02:** The head shows each state of the table for a tour in that state,
  and Publish… opens the tour page with its preview present.
- **AC03:** No mode list is offered. For fixture tours whose stops need one
  review, a mixed tour and compatibility comments, the plan chooses that
  form and the panel's sentence names the stops that forced it.
- **AC04:** During a publication of several steps, each step is shown
  waiting, then sending, then sent, in order, and the head's count agrees
  with the panel's at every step.
- **AC05:** When a step fails after earlier steps were sent, the banner
  names the step, the reason, what is on GitHub and what is not, with Resume
  primary; nothing in the failure is drawn in the placeholder colour.
- **AC06:** A tour no form can carry on the current connection is refused
  before any write, with the stops, the operation and both ways out named.
- **AC07:** On Overview, Problem and Solution render as Markdown, a stop row
  opens that stop in Review, and the state line and actions equal the Tour
  tab head's for the same tour.
- **AC08:** A live publication of a mixed tour on MikeMarcin/BPR-Test shows
  the advancing steps and produces the same GitHub result as the same tour
  published with the mode chosen by hand before this change.
- **AC09:** The owner accepts the Tour tab, the publication panel in its
  running and stopped states, and the Overview summary on a frozen build.

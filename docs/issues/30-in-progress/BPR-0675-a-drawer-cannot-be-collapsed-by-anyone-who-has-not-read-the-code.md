---
schema: "implement-rfc.card.v1"
id: "BPR-0675"
title: "A drawer cannot be collapsed by anyone who has not read the code"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# A drawer cannot be collapsed by anyone who has not read the code

The owner, 2026-09-07: "I cannot figure out how to collapse a drawer at
all."

They then said, of the toggles in the strip: "I thought those things in
the strip might be the toggles but they do nothing." They were right, and
that is the larger half of this card. The controls are wired, and the core
refused every gesture they sent.

`apply_drawer` refused with `no_active_workbench` whenever the interaction
state had no navigation, and `drawer_command` reported all three drawer
commands disabled for the same reason. A pull request whose comparison did
not resolve, which is the state the owner was in, has no navigation, so
three drawers full of files and threads could not be collapsed and the
strip's toggles did nothing when pressed. The window disabled them a
second time on its own account, because it treats an absent command
projection as a refusal, and a workbench that never resolved publishes no
projection at all.

The discoverability half stands as well: nothing on a drawer offered to
collapse it, and the strip's toggles are unlabelled icons split across the
strip. An affordance only its author can find is not an affordance.

## Scope

- A drawer offers its own collapse where the reviewer is looking: on the
  drawer, not only in the strip.
- The strip's toggles say which drawer they are for, and there is one per
  drawer.
- Collapsing and expanding are reachable by keyboard and named in the
  accessibility tree.
- Owned paths: `src/adapters/qt` drawer panel and workbench window strip,
  their tests.

## Acceptance

- A reviewer who has never seen the application can collapse and expand
  each drawer without being told how; the owner judges that in an attended
  session.
- Each control names its drawer, and a test pins one control per drawer.

## Notes (2026-09-07)

- **Why they did nothing.** The core owns drawer state and refused every
  drawer gesture without an active workbench, and reported the three
  drawer commands disabled for the same reason. That rule is right for
  line numbers, which need source panes, and wrong for drawers: a drawer
  holds the pull request's files, its threads and the reviewer's
  repositories, all of which exist before any comparison does, and the room
  it takes is the reviewer's to give back whenever they like. `apply_drawer`
  and `drawer_command` no longer consult the surface. The window kept its
  own second gate, treating an absent projection as a refusal, and a
  comparison that never resolved publishes no projection; a drawer toggle
  is now offered when the core has said nothing.
- **Where the control is.** Each drawer carries its own "Hide" beside its
  heading, named for the drawer, which sends the same gesture the strip's
  toggle does; the rail already brought it back. The strip keeps its
  toggles, and on Home it still shows none of them, because Home has no
  drawers.
- **Evidence.** `core_workbench_interaction` pins that a drawer collapses
  with no comparison open, that the command is offered rather than
  disabled, that a deactivation leaves the drawers alone while still
  disabling the line numbers, and that every other command stays refused.
  `qt_composed_github_scale_{100,125,200}` pin, in the owner's state (a
  pull request open, no comparison), that the drawer offers a named collapse,
  that the strip's toggle is enabled, and that both send the same gesture
  for that drawer. Eighty-six core, Qt and composition selectors pass, with
  `check_boundaries`, strict tidy on the three touched translation units,
  `clang-format --Werror` and `verify_documentation.py`.
- **Negative mutations.** Restoring the core's refusal fails "inactive: an
  enabled command is exactly an accepted gesture"; restoring the window's
  gate fails "the strip's toggle is offered with no comparison open";
  dropping the drawer's own control fails "the drawer's own control and the
  strip send the same gesture for that drawer".
- **Left open.** Whether the collapse reads as a collapse, and whether one
  toggle per drawer in the strip is the right arrangement, is the owner's
  judgement in the next attended session. The acceptance criterion that a
  reviewer who has never seen the application can work it out is not
  something a test can close.

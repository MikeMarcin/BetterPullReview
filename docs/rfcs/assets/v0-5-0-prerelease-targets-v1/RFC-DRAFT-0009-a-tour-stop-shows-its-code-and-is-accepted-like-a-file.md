---
id: "RFC-DRAFT-0009"
title: "A tour stop shows its code on arrival and is accepted the way a file is"
status: "Proposed"
metadata: {"origin":"owner notes, 2026-11-10, notes 19 to 23","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"authoring":"draft only; no implementation authorized"}
---

# A tour stop shows its code on arrival and is accepted the way a file is

## Problem and grounded current behavior

The owner finds that going to a stop costs an extra click to reach its code,
that a stop offers too much while it is not being edited, that "Ready, next"
is awkward, and that Suggest an outline is a button they have never used.
They want to keep an explicit accept for a stop, in the visual language
Review already uses for files, and they like the stop's yellow and green
ring ([notes 19 to 23](SOURCE_NOTES.md)). In the inspected code:

- Making a different stop current triggers navigation to its code (the
  outline's row-change handler in
  [review_tour_editor.cpp](../../../../src/adapters/qt/src/review_tour_editor.cpp)).
  Clicking the stop that is already current changes no row and navigates
  nowhere, and the card keeps a **Show in code** button as the way to get
  there. Which path cost the owner the extra click was not reproduced.
- A stop that is not being edited shows its title, a Head changed chip when
  it applies, the pencil, its rendered text, Show in code, Ready, next when
  it can act, and its side chat button; the route's tools and menus add
  Insert after, Duplicate, Merge and Remove. Editing swaps in the title and
  text fields, Use selected lines, Enhance, the trash, Cancel and Save
  (`present_stop_view`). This list is read from the code, not from a running
  window.
- A stop carries three separate author acknowledgements. **Ready**
  (`tour_preparation`) is required for publication only of a stop that came
  from Suggest an outline; that is the only place a stop's `Suggestion` is
  set
  ([review_tour.cpp](../../../../src/core/src/review_tour.cpp)). **Head
  changed** is a chip clicked to confirm the explanation after the pull
  request's head moved; publication requires every stop reaffirmed at the
  observed head, whether or not its code changed
  ([review_tour_destination.cpp](../../../../src/core/src/review_tour_destination.cpp)).
  **Lines confirmed** applies to a stop an agent proposed, which has a
  destination but no confirmed range until the author chooses Show in code,
  selects lines, edits the stop and presses Use selected lines.
- RFC-BPR-0043 requires that generated text "receive an author completion
  decision while visible, e.g. **Ready and next**", and forbids a bulk
  Ready.
- A file in Review is accepted with one named **Accept** button and Alt+A,
  and its states include Accepted and Changed since accepted
  ([review-state-semantics.md](../../../ux/review-state-semantics.md)).

## Proposed behavior and boundaries

The stop in each of its states is drawn in
[the stop mockup](../../../design/v0-5-0-targets-v1/stop.html); the Tour tab
around it is in the
[design round](../../../design/v0-5-0-targets-v1/index.html#tour).

### Arriving at a stop shows its code

Every way of arriving at a stop opens its file in Review and reveals its
lines, with no further click: choosing its row, including the row that is
already current; Previous and Next; Next changed stop; the tour's stop keys;
a link to the stop from a notice, a proposal or Overview; and a reader's
Start tour and Next stop. The Tour tab keeps its own scroll position while
the code moves, as it does today.

Show in code is removed as a button. The stop's file-and-lines line is the
way back after the reviewer has scrolled the code elsewhere: it is a link
that reveals the lines again.

### A stop is accepted, in the files' language

One acknowledgement replaces Ready, the Head changed chip and the separate
confirmation of lines. The author **accepts** a stop: its text, its
destination, and the code at that destination and at every reference in its
text, as they are when accepted. The core records what was accepted and
decides the state from it:

| State | Ring | When |
|---|---|---|
| Accepted | Green | What was accepted is what the stop is now |
| Not accepted yet | Yellow | The stop is new, or came into the tour through an agent proposal the author has not accepted for this stop |
| Changed since accepted | Yellow | The stop's lines, or lines a reference in its text names, differ at the pull request's current head or can no longer be found; or the stop was moved to another destination |

The words and their meaning are Review's: Accepted, and Changed since
accepted. The ring keeps its place and its other jobs: choosing and opening
the stop. Clicking it accepts nothing.

**What accepts a stop:**

- **Accept**, a named button on the current stop, shown only while the stop
  is yellow, with one line that says why ("Not accepted yet", "The code
  under this stop changed in the last push", "`frame_pool.cpp:41-47`, which
  this stop links to, changed"). It keeps the key Ready, next has today. It
  does not move to another stop.
- **Save**, when the author edits the stop.
- **Accept on that stop's own proposal** in its side chat.

Each is an act on one stop while its code is on screen, which arrival
guarantees. Accepting a stop an agent proposed accepts the lines it shows:
the author changes them first with Use selected lines when they are wrong.

**What does not:** opening a stop; **Accept all** on a revision of the whole
tour, which puts the changes in the draft and leaves each new or changed
stop yellow; any agent; any bulk action.

**What a new head does:** a stop whose lines and referenced lines are
byte-identical at the new head stays Accepted. Only a stop whose material
actually changed turns yellow. This replaces the rule that every stop is
reaffirmed at every head.

**Publication** requires every stop Accepted. The Tour tab's head counts
them, and the publication panel lists each yellow stop with its reason as a
link to it.

### A stop at rest, and a stop being edited

A stop that is not being edited is for reading and going. It offers exactly:

- its title, its ring, its file-and-lines link and its rendered text;
- **Edit** (the pencil);
- its side chat;
- **Accept** with its reason, only while the stop is yellow.

Everything that changes the stop is offered while it is being edited: the
title and text fields, Use selected lines, Enhance, Remove, Cancel and Save.
Route operations on a stop (Insert after, Duplicate, Merge, Move, Remove)
and Mark not accepted are in the stop's context menu and nowhere else on
the card. Adding a stop and drafting the tour belong to the route and the
agent bar.

### Suggest an outline leaves the application

Suggest an outline and Insert outline are removed from the Tour tab and the
tour page. Draft the tour is the one way to ask the agent for a route.

- The `bpr tour outline` commands and the outline hook are unchanged.
- A saved draft that holds outline-suggested stops shows them as Not
  accepted yet, and they are accepted like any other stop.
- The Suggested and Ready words leave the application's stop presentation.

### Boundaries

- This amends RFC-BPR-0043 (the application no longer offers the outline;
  the author's completion decision is Accept, Save or the stop's own
  proposal Accept, and there is still no bulk form), RFC-BPR-0042 and
  RFC-BPR-0096 (a stop is reaffirmed only when its material changed, and
  accepting an agent-proposed stop confirms the lines shown). Source
  mapping, repair and what GitHub receives are unchanged.
- Acceptance is local and private, like a file's. Nothing of it is
  published.
- No agent can accept a stop.
- The proposal cards keep their words (Accept, Accept all, Edit text,
  Discard) under
  [RFC-DRAFT-0003](RFC-DRAFT-0003-tour-conversation-that-changes-the-tour-in-one-turn.md).

### What is settled and what is proposed

Settled by the owner on 2026-11-10: arriving shows the code; the outline is
retired from the application; an explicit accept stays, in the files'
language; a stop is accepted again when the pull request's update changes
its hunk or a link in its text; the yellow and green ring stays; the ring is
not the accept control; Ready, next goes.

Accepted with the mockup. The rest of this section's rules were drawn and
listed in [the stop mockup](../../../design/v0-5-0-targets-v1/stop.html),
which named three of them as the owner's to judge. Asked on 2026-11-10
whether the mockup matched what they had in mind, the owner answered "Yes,
keep it". That covers, as drawn there: Save and the stop's own proposal
Accept counting as acceptance; Accept all not counting; accepting an
agent-proposed stop confirming its shown lines; unchanged stops staying
Accepted across a new head; the set a stop offers at rest; and folding the
Head changed chip into the ring and the reason line. The owner answered for
the mockup as a whole and not rule by rule, and has yet to see any of it
running; the owner's look on a frozen build stays a criterion.

## Acceptance

- **AC01:** Each listed way of arriving at a stop, including choosing the
  row that is already current after the code was scrolled elsewhere, leaves
  the stop's file open in Review with its lines revealed, and no Show in
  code button exists.
- **AC02:** For each row of the state table, a fixture stop in that
  condition yields that state from the core and that ring colour.
- **AC03:** Accept on the current stop, Save of an edit, and Accept on that
  stop's own proposal each leave the stop Accepted; opening the stop,
  clicking its ring and Accept all on a tour revision do not.
- **AC04:** After a push that changes one stop's lines and the lines one
  other stop links to, exactly those two stops are Changed since accepted,
  each with its reason, and every other stop stays Accepted.
- **AC05:** Publication is refused while any stop is yellow, naming each
  with its reason, and allowed once all are Accepted; the published result
  for an all-Accepted tour equals the result before this change.
- **AC06:** A stop at rest shows only the listed items; Use selected lines,
  Enhance, Remove, Cancel and Save appear only while it is edited; no Ready,
  next button and no Head changed chip exists; and every operation offered
  before this change is still reachable.
- **AC07:** Suggest an outline and Insert outline are absent from the
  application, `bpr tour outline` behaves as before, and a saved draft with
  outline-suggested stops opens with them Not accepted yet and can be
  accepted and published.
- **AC08:** The owner accepts the stop at rest, being edited, and in each
  ring state on a frozen build.

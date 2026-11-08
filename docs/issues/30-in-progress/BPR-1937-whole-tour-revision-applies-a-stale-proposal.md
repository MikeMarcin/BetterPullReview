---
schema: "implement-rfc.card.v1"
id: "BPR-1937"
title: "A later whole-tour revision doesn't replace the earlier one: asking for 8 stops applies the first 10-stop plan"
status: "In Progress"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-1930"]
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-08, coordinator chat","related_cards":["BPR-1930","BPR-1934","BPR-1925"]}
acceptance: []
---

# BPR-1937: A later whole-tour revision doesn't replace the earlier one: asking for 8 stops applies the first 10-stop plan

## Observed (owner, 2026-11-08)

In the whole-tour chat on a 12-stop tour:

1. The owner pressed the suggested prompt "Too many stops?". The agent
   suggested going from 12 to 10.
2. The owner asked to go lower. The agent suggested a way to get to 8.
3. The owner said "apply to review". The tour became 10 stops.
4. The owner said "it's still 10". The agent agreed and again suggested a way
   to get to 8.
5. "Apply to review" again. Still 10.

## Suspects (verify)

- Only the first pending revision is kept: a later turn's revision doesn't
  replace or merge with the waiting one, and Apply, Accept all, or Update
  from this chat applies the stale one.
- The later turns described the 8-stop plan only in prose, and the answer
  carried no `bpr.tour-revise.v1` payload (or carried the earlier one
  again). Then "apply" re-derives from the conversation and lands on the
  first plan.
- The 8-stop plan's merges or removals were refused by the core's
  validation, e.g. a merge across files with unconfirmed lines (BPR-1930
  uncertainty), so they were silently dropped, and only a count was shown.
  The owner saw no reason.
- The replay given to the agent on each turn shows the tour as it was
  before the applied revision, so the agent re-proposes against stale state.

## Change

- Reproduce with a stub agent following the exact sequence above (12 → a
  10-stop revision, then an 8-stop revision, then apply), and find the cause.
- A newer revision from the same chat supersedes the pending one. The
  in-place marks show the newest plan, and applying applies exactly what is
  shown.
- Every turn's replay and prompt carries the tour as it is now, including
  applied revisions.
- If any part of a revision is refused, the chat and the tour say which part
  and why, in plain words, not just a count.
- If the agent describes a plan without a revision payload, the chat offers
  "Update the tour from this chat", which asks for the structured revision,
  rather than leaving the person to type "apply".

## Acceptance

- AC1. A stub-driven course of the sequence above ends at 8 stops after
  apply (application and Qt tests).
- AC2. A refused part is named with its reason in the chat and on the tour.
- AC3. The replay after an applied revision reflects the applied tour.
- AC4. The documentation gate passes. No owner look is needed unless visible
  behaviour beyond messages changes.

## Diagnosis (2026-11-08)

Reproduced with stub answers in
`tests/application/tour_agent_conversation_tests.cpp` (`stale_revision`),
which runs the tour chat as `finish_tour_ask` and `decide_tour_proposal` do,
on a 12-stop tour of accepted proposed stops (lines still to confirm) in
three files. Before the fix:

- A. 12 → 10 by removing stops 11 and 12, then an 8-stop plan merging four
  other pairs: Accept all gave **6** stops. `core::merge_tour_proposal` let a
  new structural part supersede an old one only when they named the same
  stop, so the earlier plan stayed beside the newer one and was applied too.
- B. 12 → 10 by two same-file merges, then an 8-stop plan adding two merges
  across files, then "apply to review": **10** stops, the owner's result.
  `propose_tour_changes` refuses a merge of stops whose lines are still to
  confirm in different files (BPR-1930), and the refusal was only a count
  that nothing showed. The earlier plan's parts were superseded by the same
  parts restated, the new ones vanished, and the agent was never told, so it
  proposed the same refused merges again.
- C. A plan described only in the reply proposes nothing; the earlier plan
  waits as it was.
- Owner screenshot (coordinator, 2026-11-08): the agent answered Update the
  tour from this chat with "An update can only rewrite text. To merge stops 4
  and 5 ... ask me in the chat". The whole tour's Update prompt was the
  part chats' text rewrite ("Leave every other field empty"), so the
  structural plan could never arrive through Update either.

Suspects checked: the replay was not stale. `tour_so_far` is built from the
tour host's draft at each turn, so it carries applied revisions (now
asserted, AC3). The waiting structural plan, though, was not in the
request at all.

## Implementation (2026-11-08)

- Core (`tour_agent_session`): a refused revision part is kept as
  `tour_refused_part` (kind, stop numbers, reason) on the proposal and the
  turn's tally, in place of the bare count. A turn whose answer revises the
  tour's shape (`RevisesStructure`: any removal, merge, split or order,
  proposed or refused) replaces every pending structural part in
  `merge_tour_proposal`, so the newest plan alone waits and applies. The
  replay carries each turn's refused parts.
- Application (`build_tour_ask_prompt`): the tour chat is given
  `waiting_revision` (the pending structural parts by today's numbers) and
  told that a new revision replaces it whole; an earlier turn's `refused`
  parts with why; that it can't apply anything itself, so "apply" means
  answering with the whole revision; that a change request is answered with
  the revision, not words; and how merges keep lines. The whole tour's
  Update now asks for every agreed change, shape included. A part's
  Update stays a rewrite of its text.
- Qt (`tour_side_chat`, `tour_agent_panel`): a whole-tour turn lists its
  refused parts in plain words ("Merging stops 4 and 9 wasn't proposed:
  their lines are in different files and not confirmed yet, so one stop
  can't keep them all."); the proposal box in the tour says the same; the
  newest whole-tour answer that changed nothing says so, with a link to
  Update the tour from this chat. The host (`window_tours.cpp`) names
  refusals in the tour's notice. No change to `review_tour_editor.cpp`.

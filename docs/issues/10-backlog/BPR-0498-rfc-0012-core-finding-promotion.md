---
id: BPR-0498
title: "Core: finding promotion into a local draft or defect, never a publication"
status: Backlog
rfc: RFC-BPR-0012
depends_on: [BPR-0497]
---

# Core: finding promotion into a local draft or defect, never a publication

One pure core function takes one validated `core::agent_finding` and one
explicit promotion choice and returns exactly one `review_draft_command`
(`src/core/include/bpr/core/review_round_trip.hpp`), one `defect_command`
(`src/core/include/bpr/core/defect_workflow.hpp`), or one private-note payload
built from `core::supplement_note`
(`src/core/include/bpr/core/agent_context_supplement.hpp`). A dismissed or
stale finding produces nothing. There is no path from a finding to a
`review_publication_proposal` and none to the RFC-BPR-0008 mutation host.

## Required final state

- Promotion is a function of the finding, its anchor verdict, and the choice —
  no store, no clock, no host type, so it runs under `core-only`.
- Exactly one command comes out. Not a list, not a command plus a side effect,
  not a draft that also opens a defect.
- A dismissed finding, an unanchored finding, and a stale finding each return a
  typed refusal naming why, rather than an empty or default command.
- The result type cannot hold a `review_publication_proposal`. There is no
  field, overload, or branch that produces one; `propose_publication` stays
  reachable only from the reviewer's own publish gesture.
- The suggested comment is carried into the draft body unchanged. Promotion
  never re-words, shortens, or appends to what the agent proposed — the
  reviewer's edit is the only thing that changes it.
- The private-note result is a payload, not a store call. Writing it durably as
  an `application::local_note_record` is the host's job.

## Non-goals

- The finding type and its anchor verdict — BPR-0497.
- Legality of the action that produced the finding, and chat scope — BPR-0499.
- Persisting the note through `review_ledger_port::store_note` — BPR-0500.
- The six Qt actions that choose a promotion — BPR-0503.
- Publishing anything at all — RFC-BPR-0008's mutation path, unchanged here.

## Required direct evidence

- `ctest --preset core-only -R '^core_agent_finding(_promotion)?$'
  --output-on-failure`. The implementing slice records whether the promotion
  cases extend `core_agent_finding` or register `core_agent_finding_promotion`.
- Promoting one finding yields exactly one command, asserted on a count;
  emitting a second fails.
- A sweep over every refusal reason — dismissed, unanchored, stale — asserts a
  typed refusal and zero commands for each, rather than a hand-picked sample.
- A type-level assertion that no promotion result can carry a
  `review_publication_proposal`; adding such a field or overload fails to
  build.
- The suite links `BetterPullReview::Core` only, so a host or adapter
  dependency introduced into the promotion translation unit fails to link.
- Each assertion is shown to fail when the behaviour it constrains is
  neutered.

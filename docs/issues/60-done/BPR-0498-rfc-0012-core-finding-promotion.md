---
id: BPR-0498
title: "Core: finding promotion into a local draft or defect, never a publication"
status: Done
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

## What this slice delivered — 2026-09-01

`core::promote_agent_finding` in `src/core/include/bpr/core/agent_finding_promotion.hpp`
and `src/core/src/agent_finding_promotion.cpp`, proven by
`core_agent_finding_promotion` (`tests/core/agent_finding_promotion_tests.cpp`,
820 lines) under both `core-only` and `dev`. Implemented by a bounded
`rfc-worker`; the integrator inspected the complete diff and reproduced the
verification below. BPR-0497's surface is untouched.

### Decisions worth knowing

- **Promotion is onto an existing local artifact, never a creation.** The
  core's `review_draft_command` is `edit`/`discard`/`begin_publish`/… against
  a `review_draft_id`, and `defect_command_kind` has no "file a defect", so
  the command a finding becomes is an `edit` of the reviewer's draft whose
  body is the suggested comment byte for byte, or a `reclassify` of the
  reviewer's defect carrying the finding's severity and category, or a
  `supplement_note` payload attached by file identity and comparison. Creating
  the draft or defect stays where it is today, in the host and editor paths;
  [[BPR-0503]]'s actions create the target through those paths and then apply
  this command.
- **The anchor is checked, not translated.** Neither command carries an
  anchor, so the request names the target and the target must be anchored at
  exactly the finding's `source_line_ref`; a mismatch is `anchor_elsewhere`,
  a boundary-anchored finding is `boundary_anchor` for draft and defect and
  stays promotable into a note. No `review_mutation_target` is built anywhere.
- **Dismissal is standing, not a choice.** `agent_finding_standing::dismissed`
  on the request refuses before the verdict is read; the three choices stay
  exactly three.
- **The result type cannot hold a `review_publication_proposal`**: a
  structured-binding guard plus a per-field trait fail to compile when one is
  added, and the draft command it emits is always an `edit`.
- **Nothing is composed**: the draft body is the suggested comment and the note
  text is the claim, verbatim; a `static_assert` pins the claim bound inside
  the note bound so a note is never shortened.
- **The refusal set is eleven**, each a distinct repair, swept over a
  coverage array tied to the enumeration count rather than sampled.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The suite and its neighbours pass under `core-only` | `cmake --build --preset core-only` then `ctest --preset core-only -R '^(core_agent_finding\|core_agent_finding_promotion\|core_dependency_boundary\|core_boundary_contract\|core_review_round_trip\|core_defect_workflow)$' --output-on-failure` | build exit 0; 6/6 passed | this commit |
| The dev graph registers and passes it; the boundaries hold | `cmake --build --preset dev --target test_better_pull_review_agent_finding_promotion_core_tests` then `ctest --test-dir build/dev -R '^(core_agent_finding_promotion\|architecture_boundaries)$' --output-on-failure` | build exit 0; 2/2 passed | this commit |
| Formatting | `clang-format --dry-run --Werror` on the three new files | exit 0 | this commit |
| Exactly one command per promotion; the comment byte for byte; every refusal swept | the named cases and the 26-row sweep | pass | this commit |
| Non-vacuity | worker mutations, each restored and md5-verified: a second command from the note branch, a stale finding falling through to a draft, one byte appended to the body, and a `review_publication_proposal` field on the result | each failed by name; the field fails to compile at the binding guard and again at the trait | this commit |

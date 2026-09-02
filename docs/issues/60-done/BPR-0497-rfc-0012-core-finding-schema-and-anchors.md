---
id: BPR-0497
title: "Core: structured finding schema and anchor validation"
status: Done
rfc: RFC-BPR-0012
depends_on: [BPR-0496]
---

# Core: structured finding schema and anchor validation

A new `src/core/include/bpr/core/agent_finding.hpp` and its `.cpp` give a
returned finding a type and give its anchor a verdict. The anchor validates
only against the RFC-BPR-0004 identities already in
`src/core/include/bpr/core/diff_model.hpp` — `source_line_ref`,
`source_boundary_ref`, `line_span`, `alignment_segment`, `change_group`, and
`move_relation` — and against the frozen `agent_revision_binding` in
`src/core/include/bpr/core/agent_session.hpp`. Anything that does not validate
becomes an explicit unanchored or stale state; nothing is guessed.

## Required final state

- `core::agent_finding` carries severity, confidence, category, path, line
  range, claim, evidence, consequence, suggested comment, required context,
  provider, and the OIDs the finding was analyzed against.
- The anchor is one of the RFC-BPR-0004 identities above, checked against the
  session's `agent_revision_binding`, not against the current head.
- Validation returns a typed verdict with three distinguishable states —
  anchored, unanchored, and stale — and never a nearest or inferred line.
- No Qt coordinate, y-position, viewport offset, or ribbon path is
  representable: the type has no field one could occupy, in the shape
  `core_agent_context_supplement` already uses for its credential guard.
- Confidence is data, not authority: no branch reads it to decide a verdict.
- Text fields and the analyzed-OID list carry declared `maximum_*` bounds, as
  `agent_context_supplement.hpp` already does.
- Pure: no Qt, provider, process, clock, or filesystem dependency, so the
  suite builds and runs under `core-only`.

## Non-goals

- Turning a finding into a draft, defect, or note — BPR-0498.
- Which user actions may produce a finding, and chat scope — BPR-0499.
- Parsing an agent's bytes into this type — BPR-0500.
- Any queue, list, or presentation of findings — BPR-0503.

## Required direct evidence

- A new `core_agent_finding` is registered in `tests/core/CMakeLists.txt`
  beside `core_agent_session` and run with
  `ctest --preset core-only -R '^core_agent_finding$' --output-on-failure`.
- A valid anchor against the session's binding validates as anchored; the same
  anchor against a binding whose `HeadOid` moved validates as stale and keeps
  the OIDs the finding was analyzed against.
- An anchor naming a path absent from the frozen snapshot, and one whose
  `line_span` runs past the file, each validate as unanchored with no
  substitute line.
- A field-reading guard fails to **compile** when a coordinate, pixel, or
  ribbon field is added to either type.
- Each assertion is shown to fail when the behaviour it constrains is
  neutered; in particular, making stale fall back to anchored fails the
  staleness case by name.

## What this slice delivered — 2026-09-01

`core::agent_finding`, `agent_finding_anchor`, `agent_finding_snapshot`, and
the pure `validate_agent_finding` in `src/core/include/bpr/core/agent_finding.hpp`
and `src/core/src/agent_finding.cpp`, proven by `core_agent_finding`
(`tests/core/agent_finding_tests.cpp`, 861 lines) under both `core-only` and
`dev`. Implemented by a bounded `rfc-worker`; the integrator inspected the
complete diff and reproduced the verification below.

### Decisions worth knowing

- **The anchor is a position plus refinements.** Exactly one of a
  `source_line_ref` or a `source_boundary_ref` is engaged; `alignment_segment_id`,
  `change_group_id`, and `move_relation_id` ride beside it as identities the
  snapshot must contain, never as alternative positions — a segment covers a
  span and a move names two segments, so admitting either as a position would
  let one finding anchor to two places. `line_pair_id` is excluded because it
  would give one position a second spelling.
- **Severity and category reuse the defect vocabulary** (`defect_severity`,
  `defect_category`), so BPR-0498's promotion into a defect needs no mapping,
  which is where a severity gets widened. The core already carried that
  dependency edge.
- **Confidence is data, never authority.** `agent_finding_confidence` has
  `unstated` as its default and is not read anywhere, not even to bound it; a
  decoder (BPR-0500) refuses undeclared enumerators.
- **Validation order is the policy**: revision-independent well-formedness,
  then staleness against the session's `agent_revision_binding`, then the
  snapshot's content. A stale finding is refused before any line count is
  consulted, so it is never measured against a newer snapshot.
- **A finding's range must contain its own anchor** — an addition beyond the
  card's words, so a finding cannot name two places; swept in four refusal
  rows.
- **Every verdict carries the finding's analyzed base and head**, refusals
  included, and has no field a line could occupy: "no substitute line" is a
  property of the type.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The suite builds and passes under `core-only` | `cmake --build --preset core-only` then `ctest --preset core-only -R '^(core_agent_finding\|core_agent_session\|core_agent_context_supplement\|core_dependency_boundary)$' --output-on-failure` | build exit 0; 4/4 passed, `core_agent_finding` 0.06 s | this commit |
| The dev graph registers and passes it, and the boundaries hold | `cmake --build --preset dev --target test_better_pull_review_agent_finding_core_tests` then `ctest --test-dir build/dev -R '^(core_agent_finding\|architecture_boundaries)$' --output-on-failure` | build exit 0; 2/2 passed | this commit |
| Formatting | `clang-format --dry-run --Werror` on the three new files | exit 0 | this commit |
| Anchored, stale, unknown path, span past end | the four named cases, the latter two asserted by whole-verdict equality | pass | this commit |
| Every refusal reason and every declared bound is swept | 29 refusal rows over every `return false` site; 7 bounds admitted at the limit and refused one byte past | pass; a `static_assert` ties the sweep to the enumeration | this commit |
| Non-vacuity | worker mutations, each restored and md5-verified: stale falling back to anchored, the line-count check made always-true, an early confidence branch, and a `ViewportY` field | each failed the named case; the field addition fails to compile (`decomposes into 16 elements, but only 15 names were provided`) | this commit |
| The ordinary core suites are undisturbed | `ctest --preset core-only -R '^core_'` (worker) | 23/23 | this commit |

Not in this slice: promotion (BPR-0498), the seven entry points and chat
scope (BPR-0499), and the decoder that turns agent bytes into this type
(BPR-0500) — the last is where an undeclared confidence enumerator is refused.

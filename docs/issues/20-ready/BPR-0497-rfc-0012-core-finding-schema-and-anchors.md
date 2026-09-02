---
id: BPR-0497
title: "Core: structured finding schema and anchor validation"
status: Ready
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

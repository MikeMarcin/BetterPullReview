---
id: BPR-0606
title: Diagnose the live epoch-preparation refusal
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0602, BPR-0559]
---

# Diagnose the live epoch-preparation refusal

The v009 designated run authenticated and opened the correct PR, but its exact
comparison never became available. The selected-file stale message is a
secondary refusal. Current Quill records stop after capability discovery and
cannot identify the first failed epoch operation.

## Required final state

- Wire the existing optional bounded diagnostic port into the production epoch
  host, preserving the decision/effect boundary and shutdown ownership.
- Record preparation stages and typed failure domains for catalog/cache open,
  smart-transport acquisition, protocol acquisition and revision preparation.
  Distinguish protocol failures from transport failures without logging remote
  prose, repository URLs, paths, source, payloads, credentials or Git stderr.
- Exercise a failed preparation through the host and production composition;
  recording unavailable/full/dropped must leave the same typed failure and zero
  selected diff/publication effects. Existing successful preparation remains
  unchanged.
- Identify a reproducible cause if offline evidence supports it; otherwise
  explicitly leave the live root cause unresolved for one diagnostic candidate.

## Non-goals

No live launch, credential access, remote mutation, replay reset, broad logging
settings or schema-method evaluation. Do not guess a transport fix, relabel a
failed file read as success, or weaken exact comparison and publication guards.

## Verification

Focused epoch-host diagnostics and existing preparation tests, application
logging composition, Quill formatting/backpressure, affected scoped format,
documentation structure and diff hygiene. Reuse unchanged broader evidence.

## Completed diagnostic slice — 2026-09-05

Implemented an optional shared bounded sink captured by epoch effect jobs.
Fixed stage names and typed catalog/cache/transport/protocol/core failure enums
reach Quill before the existing core mapping; protocol pack failures retain
their own enum. The production composition passes its existing sink. Protocol,
authority, selected-file admission and failure behavior are unchanged.

Verification on the implementation based at `4b078ca5`:

- Dev and tests-disabled Release builds passed.
- `ctest --test-dir build/dev --output-on-failure -R
  '^(application_epoch_host|quill_logging|quill_logging_backpressure|application_logging|application_review_round_trip_composition_contract)$'`:
  5/5 passed in 49.33 seconds. Seven injected host failures retain the original
  core diagnostic across available/unavailable/full/dropped log status; normal
  preparation still succeeds. Real composition signs in and opens its fixture
  PR, records an injected unsafe cache-path failure through the real Quill file,
  and displays no comparison. No live service or production credentials used.
- Integrator reproduction selected `application_epoch_host`,
  `application_logging`, and `quill_logging`: 3/3 passed in 5.94 seconds.
- Strict tidy passed all six changed translation units; scoped format and
  diff hygiene passed. The simplification review kept one effect wrapper and
  fixed enum formatting, with normal argument indentation. Two inspected
  composition pins changed solely for the optional sink injection.
- Initial mechanical compile errors and a Quill analyzer finding from new
  C-string tables were corrected; final tables use the existing string_view
  pattern, with no suppression. No unresolved check failure remains.

The live root cause remains unproven. The v009 selected-file message was
secondary to missing epoch context, and that run had no diagnostic identifying
its first failed history effect. Nested recovery still reports its mapped core
reason under catalog_recovery; already-erased inner adapter detail is not
reconstructed. Stage success is not accepted-comparison evidence. BPR-0256 owns
the next candidate and attended observation; this card claims only the tested
diagnostic gap repaired, not successful live file loading or publication.

Documentation structure passes. Removed all 17 task-created temporary logs and
implementation helper scripts after their consumers completed; existing dev,
core-only and ordinary Release build roots remain, with no extra worktree.

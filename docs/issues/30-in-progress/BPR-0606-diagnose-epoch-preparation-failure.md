---
id: BPR-0606
title: Diagnose the live epoch-preparation refusal
status: In Progress
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

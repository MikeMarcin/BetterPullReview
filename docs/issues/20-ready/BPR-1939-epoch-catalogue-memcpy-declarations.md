---
schema: "implement-rfc.card.v1"
id: "BPR-1939"
title: "Include the declaration of memcpy in epoch catalogue owners"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"portability","found_by":"v0.4.2 build38 native Ubuntu GCC Release compilation","milestone_plan":{"id":"bpr-1939-epoch-cstring","path":"docs/rfcs/plans/bpr-1939-epoch-cstring/milestone-plan.json"}}
---

# BPR-1939: Epoch catalogue memcpy declarations

## Acceptance

- AC1. Include `<cstring>` directly where the epoch catalogue's shared header,
  append reservation and support implementation call `std::memcpy`; preserve
  behavior, compiler flags and assertions.
- AC2. Retain native build38's actual GCC failure and obtain native compilation
  controls for the affected implementation boundary: unmodified shadow inputs
  fail at the missing declaration and corrected inputs compile all eight epoch
  catalogue translation units with the original configured flags and external
  object/dependency outputs.
- AC3. Focused existing catalogue checks and formatting/documentation gates
  pass, then land the bounded include correction on authoritative main. Keep
  frozen build38 source and its accepted output bytes unchanged.

## Bounded plan and ownership

The shared support header uses `std::memcpy` in `read_oid` without `<cstring>`.
Append reservation has its own `read_pack` call without a direct include, as
has the support implementation's `read_blob`. Prior platform builds inherited
transitive declarations; native GCC correctly exposed the unsupported include
assumption. This milestone owns direct standard-library includes only; it
changes no schema, storage policy, data representation or runtime operation.
No new test mirrors an include. Native affected-TU compilation and existing
catalogue contracts supply the relevant evidence.

The implementer uses an isolated branch/worktree. The coordinator granted
exclusive native diagnostic ownership only after build38's Linux compile and
full controller stopped at the failed barrier. Any native proof uses an
external shadow implementation directory and redirects every generated output.
No release source, configured flags, producer outputs, package identity or
published asset is changed. Final release compilation and qualification remain
separate coordinator obligations.

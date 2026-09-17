---
schema: "implement-rfc.card.v1"
id: "BPR-0810"
title: "Windows: the multi-window public path reaches an empty optional"
status: "Backlog"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win under BPR-0806","found_at":"d587b5f1","run":"run-20260916-stations","dispatch":"dsp-001-r2"}
---

# BPR-0810: Windows: the multi-window public path reaches an empty optional

## Defect

On `windows-dev` Debug at `d587b5f1`:

- through ctest (contained), before the uncontained decision:
  `FAILED: private desktop test root under the canonical storage policy`;
- run directly (uncontained), after 30.45 s: MSVC
  `optional(429) : Assertion failed: operator->() called on empty optional`.

Before [[BPR-0806]] it stopped at `startup failure: 5 / FAILED: profile owner admitted`
after 0.09 s. The same course passes on macOS at `c0f2fa31` in 79.36 s. In a
release build an empty-optional dereference is undefined behaviour, not an
assertion, so the defect may exist on every platform and only MSVC's checked
iterators report it.

## Scope

- Get a stack for the assertion on the Windows station and identify the
  optional and the path that reaches it empty.
- Fix the source. If the empty state is legitimate on Windows (for example a
  capability the platform lacks), make the caller handle it explicitly and
  check whether macOS and Linux can reach it too.
- Register the course contained or uncontained with a measured cause, as
  BPR-0806 did.

## Acceptance

- `application_profile_multi_window_public_path` passes through
  `ctest --preset windows-dev` on the Windows station.
- The dereference site is guarded or made unreachable by construction, and the
  macOS course still passes.

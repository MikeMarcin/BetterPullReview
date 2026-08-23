---
id: BPR-0412
title: Run the Qt graph under the AppContainer boundary
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0347]
---

# Run the Qt graph under the AppContainer boundary

[[BPR-0347]] built the Windows AppContainer boundary and took the suite from 0
of 84 executing to a measured contained matrix. It says twice, in its own words,
that it did not do this:

> Beyond the failures, `BPR_BUILD_QT=ON` has never been run under this boundary;
> only the Qt-free `core-only` graph has.

> `BPR_BUILD_QT=ON` still has never been run under this boundary, unchanged by
> any of this.

That is a scope statement, not an omission — BPR-0347's own required final state
is about the `core-only` graph. But it leaves the Qt graph under containment
owned by nobody, and at least one card is waiting on it while pointing somewhere
else.

## Why this card exists rather than a wider BPR-0347

[[BPR-0364]] declared `depends_on: [BPR-0347]` because its last unmet bullet is
"`composed_application_hosts` passes in the Qt graph under the boundary". That
gate lives in the Qt graph, so the edge was pointing at the card that owns the
boundary — reasonably, but wrongly: BPR-0347 can close, exactly as scoped, with
the Qt graph still never having run. A dependency that its blocker disclaims
cannot discharge it.

Rather than widen BPR-0347 after the fact, the work gets a card that names it.

## Required final state

- `BPR_BUILD_QT=ON` configures, builds, and runs under the AppContainer launcher
  on the Windows host, or the reason it cannot is measured and written down
  rather than inferred.
- The Qt-graph tests that the boundary affects are enumerated with their
  results, the way [[BPR-0347]] enumerated the `core-only` ones.
- Any failure is attributed to a cause rather than to a symptom string. This
  program has already had one taxonomy carried forward across runs until five of
  thirteen rows turned out to be stale.
- `composed_application_hosts` is among the tests reported, because [[BPR-0364]]
  waits on exactly that.

## Non-goals

- Do not widen the boundary, and do not exempt tests from it, to make the Qt
  graph pass. [[BPR-0362]] owns the one containment concession under discussion
  and it is an owner decision.
- Do not implement the Windows symbol inspector here. That is [[BPR-0364]]'s
  code; this card supplies the surface that can prove it.

## What this needs

A Windows session. Nothing here is executable from macOS or Linux, and the card
is in `10-backlog/` rather than `50-blocked/` for the reason [[BPR-0358]]
established: the project has a Windows host, so this is a queue position rather
than a platform block.

## Verification

```sh
cmake --preset windows-dev
cmake --build --preset windows-dev
ctest --preset windows-dev --output-on-failure
```

run with the MSVC x64 environment imported, which [[BPR-0358]] records as
mandatory and easy to forget.

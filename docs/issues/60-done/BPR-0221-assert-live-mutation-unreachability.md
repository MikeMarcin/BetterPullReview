---
schema: "implement-rfc.card.v1"
id: "BPR-0221"
title: "Assert live-mutation unreachability before the live provider exists"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: []
---

# Assert live-mutation unreachability before the live provider exists

[[BPR-0142]] says it plainly: today's safety property holds *"for a reason
stronger than care"* — `review_mutation_port` has exactly one implementation,
a fake inside a test, and no production code constructs a coordinator. That
card introduces the first live implementation and **destroys that argument**,
and *"a scan that keeps passing afterwards would be passing for a reason that
no longer exists, which is worse than no scan at all."*

This card replaces the structural argument with an asserted one, and does it
**before** the live provider exists — the only moment the guard can be shown to
fail on a tree whose answer is already known.

## What is asserted

`scripts/verify_live_mutation_isolation.py`, registered as the standing ctest
`live_mutation_isolation` under both `dev` and `core-only`. Three clauses,
because a live mutation can become reachable three ways.

| | Clause | Positive control |
|---|---|---|
| A | Files implementing `application::review_mutation_port` are a subset of a three-path allowlist | the fake **must be found** |
| B | No non-production binary carries a `live_review_mutation_provider` symbol | `review_mutation_tests` must be in the manifest **and** contain `review_mutation_coordinator` |
| C | `make_live_review_mutation_provider` is called nowhere under `tests/`, and under `src/` only in the provider's two files and the composition root | — |

The positive controls are the point. An inventory that reads nothing satisfies
a subset check perfectly, and an unread symbol table is indistinguishable from
a clean one by exit code alone.

## Measured

Baseline on the tree as it stands, confirming every premise:

- `review_mutation_port` is declared at
  `src/application/include/bpr/application/review_mutation_port.hpp:68`.
- It has **exactly one** implementation: `fake_provider` at
  `tests/application/review_mutation_tests.cpp:66`.
- `live_review_mutation_provider` and `make_live_review_mutation_provider`
  appear **nowhere** in `src/`, `tests/`, or `scripts/`.

```
LIVE_MUTATION_ISOLATION_OK: 87 non-production binaries carry no
live_review_mutation_provider symbol, 3 paths may implement the port,
3 may construct it
```

### Enumeration comes from the build, not a directory walk

Every first-party executable passes through `bpr_add_executable`
(`CMakeLists.txt:67`), which already enforces the category-prefix contract. It
now also appends each non-`app_` target to the global property
`BPR_AUTOMATED_EXECUTABLES`, and the end of `tests/CMakeLists.txt` writes them
to `_bpr/automated-executables.txt` via `file(GENERATE)`. A future `bench_`,
`demo_`, `fuzz_`, or `tool_` target is covered the day it is added, because it
has to be added there.

That block is deliberately the last thing in `tests/CMakeLists.txt`. Above the
`add_subdirectory` calls it would describe a subset of the binaries and the
guard would pass by not looking, so it `FATAL_ERROR`s on an empty property.

**87, not 91.** The tree declares 91 `test_`-prefixed targets; the macOS `dev`
manifest holds 87. The four absent ones are platform-guarded — the Linux Secret
Service and Windows Credential Manager suites — and are not configured on this
host, so no binary exists to inspect. Each configuration proves the property
for the binaries it actually built, and the Windows and WSL2 sessions cover
theirs.

## Shown to fail, five ways

A rogue `live_review_mutation_provider` deriving from the real port was written
into `tests/application/rogue_live_provider_probe.cpp` and compiled into the
`review_mutation_tests` target.

| Probe | Observed |
|---|---|
| Rogue implementation, source half | `FAILED: tests/application/rogue_live_provider_probe.cpp implements review_mutation_port and is not on the allowlist` |
| Same rogue, compiled and linked | `FAILED: better_pull_review_review_mutation_tests contains a live_review_mutation_provider symbol: … observe_target()` — nine matching symbols, confirmed independently with `nm \| c++filt` |
| Factory called from `tests/` | `FAILED: … calls make_live_review_mutation_provider. Only the live implementation and the composition root may construct it, and a test may not` |
| Control binary removed from the manifest | `FAILED: no binary matching review_mutation_tests is in the manifest. Without the positive control this check cannot distinguish a clean tree from an unread one` |
| Empty manifest | `FAILED: … lists no executables; the build wrote an empty manifest and every binary check below would vacuously pass` |

Probe reverted, target restored byte-for-byte, guard green again at 87
binaries. `tests/scripts/verify_live_mutation_isolation_tests.py` drives the
three pure functions to failure on synthesised trees: 11 tests, including that
a mere mention of the port is not an implementation and that naming the factory
without calling it is not a construction site.

## Bookkeeping that now forces itself

- `verify_composed_hosts.py` gains `live_review_mutation_provider` under
  `NOT_COMPOSED`, so the day BPR-0142 composes it that file must be edited in
  the same commit.
- `composition_contract_tests.py` gains the factory to
  `CONCRETE_SELECTION_PATTERNS`. That list scans a curated subset of `src/`
  which excludes `src/adapters/github` and `src/app` by design, so it is *not*
  the whole guarantee — clause (C)'s full-tree walk is. Recorded because a
  reader could otherwise mistake the entry for complete coverage. See
  [[BPR-0222]]: that list also scans `src/adapters/ai`, which does not exist.

## The production binary is deliberately excluded

End-to-end tests execute `app_better_pull_review`, so after BPR-0142 the
shipped application will contain live code while being driven in fixture mode.
Clause (B) would be unsatisfiable and wrong to apply there. The runtime
complement belongs to BPR-0142: a fixture-driven run emits
`REVIEW_MUTATION_PROVIDER:fixture`, asserted in the end-to-end pass
expressions. Symbol absence and runtime selection are different claims and
neither substitutes for the other. This is noted on BPR-0142.

## What must not be done instead

Adding the live provider under a different name or path because the allowlist
rejected it. The allowlist is the contract; the code moves to fit it.

Relaxing a clause so a legitimate change passes. If a real second implementation
of the port is ever justified, it is added to the allowlist deliberately, in a
commit that says why — not by widening the rule.

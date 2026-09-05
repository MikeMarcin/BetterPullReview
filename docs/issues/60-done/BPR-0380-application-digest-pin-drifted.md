---
schema: "implement-rfc.card.v1"
id: "BPR-0380"
title: "Re-pin the application digests BPR-0341 left behind"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
completed: "2026-08-18"
---

# Re-pin the application digests BPR-0341 left behind

`application_composition_contract` fails on a clean checkout:

```
full LF-normalized github_application.cpp digest drifted: expected
857636faf3d9b198a35cc6762183db6903392aee8fada9e9357e43c75149a175, found
2e6427b0ddd61d1a9c7f89082ccb071e94940dea18f00b8448b7e586568fb640
```

That pin is
`tests/end_to_end/application_review_round_trip_composition_contract.cmake:42`.
It is not the only one. Repairing it exposes a second, at line 134:

```
production application source content digest drifted: expected
a0f15b6df67f321a65d1a3cd7851cbbbde459900b07d28aec0679b44c5768a45, found
4e1518309333a95458cdfe5cd583ad797416f8a5c24ecfddeca2d769521f4058
```

That one is a manifest over the eight production `src/app` sources rather than
one file, and `github_application.cpp` is among them, so the same edit moved
both. The contract stops at the first, which is why the second is only visible
once the first is fixed. A third pin at line 444 covers a narrower mutation
surface and did not drift.

## When it drifted

Exactly at `27e4bed`, the [[BPR-0341]] slice that put the agent workspace on the
cache's filesystem:

| commit | file digest (line 42) | manifest digest (line 134) |
|---|---|---|
| `6d7a945` — the last re-pin, [[BPR-0346]] | `857636fa…` | `a0f15b6d…` |
| `27e4bed~1` | `857636fa…` | `a0f15b6d…` |
| `27e4bed` | `2e6427b0…` | `4e151830…` |
| `605aa56` | `2e6427b0…` | `4e151830…` |
| `main` today | `2e6427b0…` | `4e151830…` |

Both pins were correct up to `27e4bed~1` and both moved at `27e4bed`. Neither
has moved since, in this lane or on `main`.

That slice edited the file the pin covers and did not re-pin it. The digest is
LF-normalized, so this is not a line-ending artifact and not platform-specific;
it fails wherever the contract runs.

## Why it went unnoticed until now

The contract never got this far on Linux. It failed earlier, in [[BPR-0343]],
because the tests-disabled Release graph did not compile under GCC. With that
repaired the contract reaches the digest check for the first time and finds
this waiting. Same shape as the second GCC site BPR-0343 uncovered: one failure
was standing in front of another.

This is the third gate that one slice missed. [[BPR-0367]] records that the
same commit also left `github_application.cpp` misformatted under the version
the tree is formatted for. Whether the board's per-slice verification rule needs
teeth rather than trust is a repository-policy question and is not decided here,
but it now has three data points instead of one.

## Required final state

- Both pins record the current content they cover.
- `application_composition_contract` passes on Linux.
- The re-pin is only a re-pin. Each drifted value is traced to BPR-0341's
  accepted edit before it is replaced; a digest that disagrees for any other
  reason is a different finding and does not belong here.

## Note for BPR-0367

[[BPR-0367]] will reformat `github_application.cpp`, which changes both of
these digests again and requires another re-pin in that slice. That is expected, not a conflict: the pin
records content, and the content will change once more.

## Verification

```sh
ctest --preset dev -R '^application_composition_contract$' --output-on-failure
```

## Result — 2026-08-18

Both pins now record current content, and
`application_composition_contract` passes on Linux:

```
ctest --preset dev -R '^application_composition_contract$' --output-on-failure
# Ran 5 tests in 223.8s ... 100% tests passed out of 1
```

The change is two hexadecimal literals. No assertion was removed, relaxed, or
made conditional, and no source the pins cover was edited.

## Evidence

Each replaced value was traced before it was replaced, by recomputing the
contract's own definitions over `git show` output rather than by trusting the
error message:

- Line 42 is the LF-normalized SHA-256 of `src/app/src/github_application.cpp`.
  `27e4bed~1` gives `857636fa…`, matching the old pin; `27e4bed`, `605aa56`,
  `main`, and this branch all give `2e6427b0…`.
- Line 134 is the SHA-256 of a manifest of `"<relative path>\n<LF digest>\n"`
  over the eight production `src/app` sources. `6d7a945` — the previous re-pin,
  [[BPR-0346]] — and `27e4bed~1` both give `a0f15b6d…`, matching the old pin;
  `27e4bed` onward gives `4e151830…`.
- The recomputation agrees with CMake: the value my recomputation produces for
  this branch is byte-identical to the `found` digest the contract reported
  before the re-pin, for both pins.
- This branch's own commits are not implicated. They touch
  `src/adapters/github/` and `src/adapters/sqlite/`; neither pin covers those
  paths, and the digests at this branch's HEAD equal those at `main`.

The failure was reached at all only because [[BPR-0343]] repaired the GCC
Release compile that had been stopping the contract earlier. Before that, the
contract never evaluated either pin on this platform.

Not addressed here: `github_application.cpp` is still misformatted under the
version the tree is formatted for. That is [[BPR-0367]], and reformatting it
will move both of these digests again.

---
id: BPR-0368
title: Re-pin the application digest BPR-0341 left behind
status: In Progress
rfc: RFC-BPR-0013
depends_on: []
---

# Re-pin the application digest BPR-0341 left behind

`application_composition_contract` fails on a clean checkout:

```
full LF-normalized github_application.cpp digest drifted: expected
857636faf3d9b198a35cc6762183db6903392aee8fada9e9357e43c75149a175, found
2e6427b0ddd61d1a9c7f89082ccb071e94940dea18f00b8448b7e586568fb640
```

The pin is `tests/end_to_end/application_review_round_trip_composition_contract.cmake:42`.

## When it drifted

Exactly at `27e4bed`, the [[BPR-0341]] slice that put the agent workspace on the
cache's filesystem:

| commit | LF digest of `src/app/src/github_application.cpp` |
|---|---|
| `27e4bed~1` | `857636fa…` — matches the pin |
| `27e4bed` | `2e6427b0…` |
| `605aa56` | `2e6427b0…` |
| `main` today | `2e6427b0…` |

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

- The pin records the current content of `src/app/src/github_application.cpp`.
- `application_composition_contract` passes on Linux.
- The re-pin is only a re-pin. If the digest disagrees for any reason other than
  BPR-0341's accepted edit, that is a different finding and does not belong here.

## Note for BPR-0367

[[BPR-0367]] will reformat this same file, which changes this digest again and
requires another re-pin in that slice. That is expected, not a conflict: the pin
records content, and the content will change once more.

## Verification

```sh
ctest --preset dev -R '^application_composition_contract$' --output-on-failure
```

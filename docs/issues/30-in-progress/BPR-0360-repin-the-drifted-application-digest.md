---
id: BPR-0360
title: Re-pin the application digests that drifted without notice
status: In Progress
rfc: RFC-BPR-0008
depends_on: []
---

# Re-pin the application digests that drifted without notice

`application_review_round_trip_composition_contract` and half of
`application_composition_contract` fail on a stale pin:

```
full LF-normalized github_application.cpp digest drifted: expected
857636faf3d9b198a35cc6762183db6903392aee8fada9e9357e43c75149a175, found
bf9e5863d73c0fab966338697a6cf5f4c5c066ac26768994d35270c3cce0cfce
```

Found while running the full `dev` matrix for [[BPR-0348]].

## It was already stale before that slice

Measured rather than assumed, hashing the blob at each commit the same way
`bpr_verify_full_application_digest` does:

| Point | Digest |
|---|---|
| `605aa56`, the BPR-0348 base | `2e6427b0…` |
| `a111e1f`, before the format repair | `2e6427b0…` |
| `ce087be`, after it | `bf9e5863…` |
| pinned expectation | `857636fa…` |

The pin matched none of them, including the base. So this is not fallout from
[[BPR-0359]]'s whitespace repair; that repair moved an already-wrong number.
The last commit to change `github_application.cpp` before this session was
`27e4bed` ([[BPR-0341]]), which re-pinned nothing.

An earlier reading of this attributed the drift to BPR-0359, from a broken
measurement — `git cat-file` on unquoted short refs silently resolved to other
objects and returned 2.8 KB and 31 KB for a 382 KB file. Recorded because the
wrong conclusion is the easy one to reach here.

## The pattern, which is the same one BPR-0359 records

`27e4bed` left both the format gate and this digest red, and neither was
noticed until a later slice ran the wider gates. Three digests and a file list
are pinned by hand in
`tests/end_to_end/application_review_round_trip_composition_contract.cmake`
(`:42`, `:135`, `:445`), with no regeneration command; the precedent is
`6d7a945`, which re-pinned them manually. A pin nothing recomputes is a pin
that goes stale silently.

## Required final state

- All three pinned digests and the file list agree with the committed source.
- `application_review_round_trip_composition_contract` passes.
- The `application_composition_contract` subtest that fails on this digest
  passes. Its other failure is the offline dependency cache and is not this
  card.
- How the numbers were recomputed is written down next to them, so the next
  re-pin is not archaeology.

## Not this card

The offline dependency-cache failures (`dependency_modernization_contract`,
and `test_tests_off_release_graph_omits_public_path_fixture` needing Qt 6.10.3
and KSyntaxHighlighting to resolve) are environmental. The
`platform_configuration_contract` mismatch is Windows work: the preset has said
`Visual Studio 18 2026` since `7cf8356` while the test still expects
`Visual Studio 17 2022`. [[BPR-0362]] records it.

---
schema: "implement-rfc.card.v1"
id: "BPR-0371"
title: "The architecture-boundary fixtures are written with translated newlines"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The architecture-boundary fixtures are written with translated newlines

`architecture_boundary_contract` failed on Windows with the same message
[[BPR-0359]] fixed for the checked-in tree, and against the same file:

```
FAIL: test_designated_live_canary_rejects_host_authority
AssertionError: 1 != 0 : CMake Error at cmake/VerifyArchitectureBoundaries.cmake:1152
  Designated live canary surface file is not exact:
  designated_live_mutation_canary.cpp
```

[[BPR-0359]] listed this among the failures it could **not** show were its
defect, because the file the checker reads here is not the checked-in one — the
test writes its own copy into a scratch tree first. It is the same defect
reached through a different door, and this card is where that gets confirmed
rather than assumed.

`tests/scripts/architecture_boundary_tests.py:1369` staged every fixture with:

```python
path.write_text(contents, encoding="utf-8")
```

Python's text mode translates `\n` to `\r\n` on Windows. The canary surface
literals in the test are LF, `VerifyArchitectureBoundaries.cmake` hashes what it
finds on disk, and its four pins plus the combined
`fb77b7ca2a80bea0…` digest are taken over the LF form. So the fixture could
never be exact on Windows, and two cases failed for a reason unrelated to what
they were testing — one of them a case whose whole subject is that the canary
rejects host authority.

## Resolution

`newline=""`, so the literal bytes reach the file.

That is the correct semantics regardless of platform: these fixtures stand in
for checked-in files whose exact bytes are pinned, and a fixture that is
silently rewritten on the way to disk is not standing in for them. Nothing about
the pins, the checker, or the test's own literals changes.

## Result

```
py -3 tests/scripts/architecture_boundary_tests.py
Ran 119 tests in 161.930s
OK
```

The counterfactual is the failure itself: the same 119 cases, with the same
literals, failed twice before this one-line change and pass after it.

## Note

The staging loop writes every fixture, not only the canary surface, so the fix
covers the whole suite rather than the two cases that happened to notice. Any
future check in `VerifyArchitectureBoundaries.cmake` that compares bytes will
now see what the test wrote.

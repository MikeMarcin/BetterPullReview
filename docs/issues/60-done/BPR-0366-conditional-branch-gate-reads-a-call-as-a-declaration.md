---
schema: "implement-rfc.card.v1"
id: "BPR-0366"
title: "The conditional-branch gate reads a call as a declaration"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The conditional-branch gate reads a call as a declaration

`conditional_branch_identifiers` failed on Windows against source that is
correct:

```
tests/network_isolation/designated_live_mutation_canary.cpp:25:
  `read` is used where the declared name is `_read`
tests/network_isolation/designated_live_mutation_canary.cpp:37:
  `write` is used where the declared name is `_write`
```

The canary is right. It calls `_read` under `#if defined(_WIN32)` and `read`
under `#else`, which is what those two platforms each provide. Two correct
spellings of two different functions were being reported as one rename.

## Two things combined

`DECLARATION` matches `<type> <name>` followed by `= ( { ;`, and nothing stopped
the type position from being a statement keyword. So

```cpp
return _read(aDescriptor, aBuffer, aSize);
```

parsed as a declaration of `_read`. `spelling_key` then normalises by stripping
underscores and lowercasing, so `_read` and `read` share the key `read`, and the
`read` in the branch this host does not compile resolved to the `_read` the gate
believed was declared beside it.

Neither half is wrong alone. Stripping underscores is exactly what catches
`expected_bytes` beside `aExpectedBytes`, which is the rename class
[[BPR-0155]] and [[BPR-0336]] were built to find. The parse is the defect.

## Why only Windows saw it

The two branches are not written symmetrically:

```cpp
return _read(aDescriptor, aBuffer, aSize);                     // _WIN32
return static_cast<int>(read(aDescriptor, aBuffer, aSize));    // #else
```

`return static_cast<int>(read(` has no `<type> <space> <name>` shape, so the
POSIX branch never produced a false declaration. Only the Windows spelling did,
and only a host that treats the `#else` branch as inactive would then look for
`read` in it. Measured:

```
'return _read(aDescriptor, aBuffer, aSize);'                  -> ['_read']
'return static_cast<int>(read(aDescriptor, aBuffer, aSize));' -> []
```

## Resolution

A negative lookahead keeps statement keywords out of the type position:
`return`, `else`, `do`, `try`, `case`, `goto`, `throw`, `delete`, and the
coroutine forms.

`using` and `typedef` are deliberately **not** in that list. `using reason =
int;` really does declare `reason`, and excluding it broke `src/app/main.cpp:750`
and `:787` immediately — the first attempt traded two false positives for two
different ones, which is how the boundary of the fix was found rather than
guessed.

The pinned canary source is untouched. Making its two branches symmetric would
also have silenced the gate, and would have changed a file whose exact bytes are
part of the designated-live approval identity ([[BPR-0359]]), to work around a
defect that is not in it.

## Result

```
py -3 scripts/verify_conditional_branches.py --root .   CONDITIONAL_BRANCH_IDENTIFIERS_OK:376
```

The gate had no tests at all, which is how a parser defect survived in a checker
whose whole subject is source no compiler will look at. It has four now, in
`tests/scripts/conditional_branch_identifier_tests.py`, registered as
`conditional_branch_identifier_contract`:

- a renamed parameter in an inactive branch is still caught;
- an inactive branch using the current name is silent;
- a platform-alias call is not a declaration — the canary's exact shape, checked
  from both the Windows and the POSIX side;
- a `using` alias declared inside an inactive branch still declares its name —
  `src/app/main.cpp`'s exact shape.

Counterfactual, which is the part that matters: with the parser change reverted
and the tests unchanged, **2 of the 4 fail**. With it, 4 pass.

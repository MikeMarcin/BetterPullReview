---
id: BPR-0355
title: Repair the format gate that fails at HEAD
status: Done
rfc: RFC-BPR-0011
depends_on: []
---

# Repair the format gate that fails at HEAD

`coding-format-check` does not pass on a clean checkout. It reports five
violations in `tests/adapters_qt/review_landing_model_tests.cpp:70-74`, all of
them aligned-initializer spacing in one local struct:

```sh
cmake --build --preset dev --target coding-format-check
# tests/adapters_qt/review_landing_model_tests.cpp:70:6: error: code should be
# clang-formatted [-Wclang-format-violations]
```

The violation is committed, not a working-tree artifact. `git show
HEAD:tests/adapters_qt/review_landing_model_tests.cpp` fails the same check, and
`git log` attributes the file's current state to `82c7d1c`, the [[BPR-0340]]
slice that bound the signal recorder's connections to its lifetime.

[[BPR-0333]] found this while running its own format gate. That card formatted
only the three files it touched and deliberately left this one alone: sweeping
another card's file into an unrelated slice is exactly what the concurrent-session
rule forbids, and the fix belongs to whoever owns the surrounding work.

## Required final state

- `cmake --build --preset dev --target coding-format-check` passes with no
  violations.
- The repair is whitespace only. If `clang-format` wants a substantive change to
  that struct, that is a separate question and does not belong here.
- Whether the gate can regress this way unnoticed is answered. A format
  violation reaching `main` means the slice that introduced it either did not run
  the gate or ran it before its last edit.

## Verification

```sh
cmake --build --preset dev --target coding-format-check
```

## Result — 2026-08-17

`coding-format-check` passes: 374 files checked, no violations.

The repair is whitespace only, and smaller than the error count suggested. All
five diagnostics came from one alignment group: `QObject Context;` sits above
five `int` members, and the configured `AlignConsecutiveDeclarations` wants the
member names aligned across the whole run, not only the initializers. The five
`int` declarations gained four spaces each. Nothing else in the file moved.

`qt_review_landing_model` was rebuilt from the formatted source — not merely
re-run against the existing binary — and passed.

### How it reached main

The gate was not run against the final state of the [[BPR-0340]] slice. The
violation is in the same struct that slice edited, so a format check at any
point after its last edit would have caught it. This is the failure mode the
board's own verification rule is meant to prevent: the rule requires
`clang-format` on the files a card touched before it moves to `60-done/`, and
that step produces exactly this diagnostic in under a second.

Nothing structural prevents a recurrence. The gate is a build target rather than
a commit hook, so it catches this only when someone runs it, and the next card to
run it inherits the failure — which is how [[BPR-0333]] found it. Whether that
should become a hook is a repository-policy question rather than a defect, and
this card does not decide it.

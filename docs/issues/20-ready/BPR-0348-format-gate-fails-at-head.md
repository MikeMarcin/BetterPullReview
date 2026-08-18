---
id: BPR-0348
title: Repair the format gate that fails at HEAD
status: Ready
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

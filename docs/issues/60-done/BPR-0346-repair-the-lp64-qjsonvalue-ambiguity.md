---
schema: "implement-rfc.card.v1"
id: "BPR-0346"
title: "Repair the LP64 QJsonValue ambiguity no macOS compiler can see"
status: "Done"
rfcs: ["RFC-BPR-0006"]
depends_on: []
completed: "2026-08-17"
---

# Repair the LP64 QJsonValue ambiguity no macOS compiler can see

The first Linux `dev` build since BPR-0050 failed on exactly one translation
unit. 670 of 671 edges compiled; `src/app/src/github_application.cpp` did not.

```
github_application.cpp:8209:49: error: conversion from 'const int64_t'
{aka 'const long int'} to 'const QJsonValue' is ambiguous
```

## Why no local gate could see it

`QJsonValue` declares constructors taking `bool`, `int`, `double`, and
`qint64`. `qint64` is `long long` on every platform this project targets.

`std::int64_t` is not a distinct type; it is a typedef for whichever type is
64 bits wide, and the choice differs by data model:

| Target | `std::int64_t` | Binds `QJsonValue(qint64)` exactly? |
|---|---|---|
| macOS arm64 (LP64) | `long long` | yes |
| Linux x86_64 (LP64) | `long` | **no** |

On macOS the argument is an exact match and overload resolution ends there.
On Linux `long` matches none of the four constructors exactly, so all four
become equally-ranked integral or floating conversions and the call is
ambiguous. The failure is a property of the platform typedef, not of the
value, so it is invisible to every compiler running on macOS — the same
structural reason BPR-0155 gave for the `__linux__` identifier drift and
BPR-0336 gave for the `_WIN32` branches.

This one is narrower than either. It is not inside a conditional branch. It is
ordinary unconditional code that macOS happens to accept.

`Pixels.MedianContrastHundredths` is declared `std::int64_t` at line 6842.
Every sibling entry in the same initializer list already writes
`static_cast<qint64>(...)`; this single entry did not. The three further
diagnostics at 8255, 8357, and 8362 were cascades — one ambiguous member
invalidates the whole braced-init-list — so correcting the one entry compiles
the unit with zero diagnostics.

`scripts/verify_conditional_branches.py` cannot catch this class. It reads
branches that are false for the host running it, and this line is in no
branch at all.

## Result

The repair is one `static_cast<qint64>`, matching the convention already used
by every adjacent entry. The complete Qt application graph then builds on
Ubuntu 26.04 x86_64 with GCC 15.2.0 under `-Wall -Wextra -Wpedantic
-Wconversion -Wsign-conversion -Werror`: 671 of 671 edges, and
`src/app/better_pull_review` links.

A source sweep for other unguarded 64-bit values reaching a `QJsonValue`
found no second instance; the remaining bare entries in that file are `bool`,
`int` literals, or `double`.

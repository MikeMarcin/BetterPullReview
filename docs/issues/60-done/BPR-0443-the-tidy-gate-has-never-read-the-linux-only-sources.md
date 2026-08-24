---
id: BPR-0443
title: The tidy gate has never read the Linux-only sources
status: Done
rfc: RFC-BPR-0002
depends_on: []
completed: 2026-08-22
---

# The tidy gate has never read the Linux-only sources

`coding-tidy` is an RFC-closeout gate. On the Linux `dev` tree it did not run,
and the Linux-only sources it would cover had therefore never been inspected by
it on any host.

## Two independent halves

**The gate could not analyse a GCC build tree.** `compile_commands.json` from
the `dev` preset carries the flags GCC was invoked with, and `clang-tidy`
rejected three of them outright:

```
error: unknown argument: '-fmodules-ts'                      [clang-diagnostic-error]
error: unknown argument: '-fmodule-mapper=...modmap'         [clang-diagnostic-error]
error: unknown argument: '-fdeps-format=p1689r5'             [clang-diagnostic-error]
```

`tidy.py` read as macOS-first — `host_extra_args()` returns early unless
`sys.platform == "darwin"`, and its only platform branch adds an `-isysroot`
from `xcrun`. Nothing there anticipated a database written by a different
compiler family.

**The Linux-only sources were never inspected.** With the gate working,
`src/adapters/github/src/linux_secret_service.cpp` reported 129
`readability-identifier-naming` findings. A macOS pass never compiles that
file, so it is absent from that host's compilation database; a Linux pass
compiles it but could not analyse it. It had been outside the gate in both
directions since it was written.

## Result — 2026-08-22

**The gate runs.** `tidy.py` now rewrites the compilation database without the
flags the analyser cannot accept, into a temporary copy, and reports what it
removed. On the `dev` tree that is 230 flag occurrences across three kinds.
`clang-diagnostic-error` went from three per translation unit to **zero**.

Filtering was chosen over a second Clang-configured build tree because this
tree declares no module — no `export module`, no `import` — so those flags
exist only because CMake scans for modules under C++20 with GCC. Dropping them
for the analyser removes nothing it would have seen.

**The naming findings are accepted, not renamed.** Renaming was tried first and
reverted, because it is not safe here.
`cmake/VerifyArchitectureBoundaries.cmake` pins this adapter's *source text* to
guarantee the exact, binary, noninteractive credential lifecycle, and several
pinned patterns name parameters directly — `prompt`, `actual_name`,
`canonical_service_bus_name`. Renaming them to the `aPrompt` convention leaves
behaviour identical and fails `architecture_boundaries`, which is the stronger
authority: a reviewed security invariant against a style. Proven both ways —
the renamed tree failed that test, and restoring the spellings made it pass.

So the file carries `NOLINTBEGIN/NOLINTEND(readability-identifier-naming)` with
the reason written at the top, scoped to naming only. Every other check still
reads it.

| Claim | Command | Result |
|---|---|---|
| the gate runs on a GCC tree | `tidy.py --build-dir build/dev --warnings-as-errors` | zero `clang-diagnostic-error`, was three per TU |
| naming findings no longer reported | `clang-tidy --checks=-*,readability-identifier-naming --warnings-as-errors=*` | exit 0, was 129 |
| the pinned lifecycle invariant still holds | `ctest -R architecture_boundaries` | Passed |
| behaviour preserved | `ctest -R linux_secret_service_contract` | Passed |
| format | `format_check.py --clang-format /usr/bin/clang-format` | passed, 388 of 388 |

### Three reasons the automatic fixer is not usable on this file

Found by building and testing the renamed tree, not by reading the diff.

It mangles any identifier already beginning with `a`: the convention is prefix
`a` plus CamelCase and the fixer strips what it reads as an existing prefix
first, so `authority` became `aUthority` and `attributes` became `aTtributes`.

It renamed the GObject type `NonPromptingSecretService` to
`non_prompting_secret_service`, which does not compile — `G_DEFINE_TYPE` pastes
that spelling to generate `NonPromptingSecretServiceClass` and the `g_type`
name.

And, decisively, it broke `architecture_boundaries`. The first two were
fixable; the third says the file's spellings are load-bearing.

### Not fixed here

Nine pre-existing non-naming findings the working gate now exposes, split to
[[BPR-0444]]: eight `bugprone-suspicious-stringview-data-usage` and one
`bugprone-branch-clone`.

### Also observed

The native Secret Service fixtures fail on this host. Proven not to be this
card's doing: `git stash`, rebuild, and the same three fail identically at
unmodified `HEAD`. Recorded on [[BPR-0310]], which owns the Linux vault rows.

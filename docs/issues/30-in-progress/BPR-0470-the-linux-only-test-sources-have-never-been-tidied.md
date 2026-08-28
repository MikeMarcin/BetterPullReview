---
id: BPR-0470
title: The Linux-only test sources have never been read by the tidy gate
status: In Progress
rfc: RFC-BPR-0002
depends_on: [BPR-0444]
---

# The Linux-only test sources have never been read by the tidy gate

[[BPR-0443]] found that `coding-tidy` had never analysed
`src/adapters/github/src/linux_secret_service.cpp` on any host, and fixed the
gate so it could. It resolved that file's 129 identifier-naming findings by an
explained file-scoped suppression, and split its nine non-naming findings to
[[BPR-0444]].

The two Linux-only *test* sources were in exactly the same position and were
not looked at. `tests/` is one of the tidy gate's default roots, so the gate
covers them; both files are registered only under `if(CMAKE_SYSTEM_NAME
STREQUAL "Linux")` in `tests/github_contract/CMakeLists.txt`, so no other host
has a compilation database entry for either.

| Count | Check | File |
|---|---|---|
| 145 | `readability-identifier-naming` | `tests/github_contract/linux_secret_service_contract_tests.cpp` |
| 165 | `readability-identifier-naming` | `tests/github_contract/linux_secret_service_native_tests.cpp` |

Every one of the 310 is `readability-identifier-naming`. There is no
`bugprone-` or `clang-analyzer-` finding in either file, and a portable test
file in the same directory — `github_read_contract_tests.cpp` — reports zero,
so this is not a repo-wide condition that the naming convention itself causes.

## Why this is not simply BPR-0443 again

BPR-0443 declined to rename in the adapter, and its reason does not transfer.
`cmake/VerifyArchitectureBoundaries.cmake` pins that adapter's *source text*,
including several parameter spellings, so renaming there failed
`architecture_boundaries` — a reviewed security invariant outranking a style.
No pin reads either test file. Renaming is therefore on the table here, and a
file-scoped suppression needs a reason that does not borrow the adapter's.

The automatic fixer is still not usable: BPR-0443 recorded that it mangles
identifiers already beginning with `a`, turning `authority` into `aUthority`.
That failure mode is about the convention and the tool, not about the adapter,
so it applies to these files too.

## Required final state

- Each of the 310 is renamed, or suppressed with a written reason that stands
  on its own rather than citing the adapter's pinned-text argument.
- `linux_secret_service_contract` and the native trio still pass afterwards,
  proving the renames are spelling and not behaviour.
- `coding-tidy` is clean on both files.

## Not this card

The adapter source, which [[BPR-0443]] and [[BPR-0444]] finished, and the
native Secret Service fixture failures recorded on [[BPR-0310]].

## Ready, and ready on Linux — 2026-08-27

The lane is Linux for the same reason BPR-0444's was: neither file appears in
a macOS or Windows compilation database, so "clean afterwards" can only be
shown here. Judging the names is portable reading, but the split rule takes no
card whose portable half is unverifiable here.

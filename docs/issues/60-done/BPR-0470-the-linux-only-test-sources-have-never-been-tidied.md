---
schema: "implement-rfc.card.v1"
id: "BPR-0470"
title: "The Linux-only test sources have never been read by the tidy gate"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0444"]
completed: "2026-08-27"
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

## Result — 2026-08-27

Renamed, not suppressed. 167 distinct identifiers, 841 occurrences, across the
two test sources and the support header they share — the header was in scope
and unnamed above, and it is Linux-only for the same reason: nothing else
includes it.

The suppression the adapter got was not available here and the card said so.
BPR-0443's reason was that `VerifyArchitectureBoundaries.cmake` pins the
adapter's source text, so renaming there broke a reviewed security invariant.
No pin reads these files. Without that reason there was no honest one, since
`github_read_contract_tests.cpp` sits in the same directory under the same
convention and reports zero.

### The fixer, used with its known defect handled

`clang-tidy --fix` did the mechanical work and mangled exactly what BPR-0443
said it would. The convention is prefix `a` plus CamelCase, and the fixer
strips what it reads as an existing prefix first, so the five parameters whose
names already began with `a` came out wrong:

| Original | Fixer | Corrected |
|---|---|---|
| `account` | `aCcount` | `aAccount` |
| `actions` | `aCtions` | `aActions` |
| `actual` | `aCtual` | `aActual` |
| `aggregate` | `aGgregate` | `aAggregate` |
| `authority_value` | `aUthorityValue` | `aAuthorityValue` |

24 occurrences, repaired before anything was built. The other two failure
modes BPR-0443 recorded do not arise: neither file uses `G_DEFINE_TYPE` or any
token-pasting macro, and no pin reads them.

The fixer also rewrote every `{ }` to `{}`, which is the exact difference the
project's `clang-format` gate exists to hold. `format.py` put them back.

### Proving it is a rename

A test suite passing shows the renamed code still works. It does not show that
only names changed. That is checked directly at the source level: with string
and character literals extracted in order and every identifier erased, all
three files are byte-identical to their pre-rename selves, and each file has
the same identifier token count before and after.

| File | Literals | Skeleton | Identifier tokens |
|---|---|---|---|
| `linux_secret_service_contract_tests.cpp` | 86 identical | identical | 3,580 -> 3,580 |
| `linux_secret_service_native_tests.cpp` | 125 identical | identical | 4,211 -> 4,211 |
| `linux_secret_service_test_support.hpp` | 6 identical | identical | 247 -> 247 |

That matters more than usual here. These files assert an exact 574-character
authority encoding and exact stored byte counts; a rename that silently
touched a literal would be a vault test agreeing with itself about the wrong
value.

### Correcting this card's own criterion

The required final state above asked for "the native trio still pass
afterwards". That bar was wrong when it was written, on this card, by the
session that wrote it. The trio does not pass on this host and did not pass
before the rename either — the default collection is locked and the fixture
refuses prompts by construction, which [[BPR-0310]] owns and [[BPR-0444]]
re-proved at this head. Asking a card to restore a condition that never held
during its own work makes it unclosable for a reason that has nothing to do
with it.

What that bullet was for was behavioural evidence from the *native* binary,
the file with 89 of the renames. That is supplied instead by the three direct
native checks `docs/development/build-and-test.md` documents, which need a
real Secret Service but not an unlocked collection, and which were run both
before and after so that "still" is measured rather than assumed:

| Direct check | Pre-rename | Post-rename |
|---|---|---|
| `--expect-unavailable`, session bus and display unset | `unavailable:pass` | `unavailable:pass` |
| `--expect-locked` | `locked:pass` | `locked:pass` |
| `--expect-bus-redirect-rejected`, hostile `SECRET_SERVICE_BUS_NAME` | `bus-redirect-rejected:pass` | `bus-redirect-rejected:pass` |

These are not registered CTest cases, so as far as this board records they had
never been run on this host. They exercise real libsecret through the renamed
binary, including the dependency-level redirect rejection.

| Claim | Command | Result |
|---|---|---|
| both files are clean | `tidy.py --build-dir build/dev --warnings-as-errors --paths tests/github_contract/linux_secret_service_contract_tests.cpp tests/github_contract/linux_secret_service_native_tests.cpp` | clean, was 145 and 165 |
| only names changed | literal and skeleton equivalence over all three files | identical, 217 literals |
| the renamed code behaves | `ctest --preset dev -R '^linux_secret_service_contract$'` | Passed |
| the renamed native binary behaves | the three direct checks above | 3 of 3, matching baseline |
| format | `format_check.py --clang-format /usr/bin/clang-format` | passed, 425 of 425 |

### What is still not covered

The registered native trio. It needs the designated desktop session
`docs/development/build-and-test.md` describes — Xvfb, a session D-Bus, a
dedicated user, and an unlocked collection whose password may not appear in
arguments, environment, or output. That setup is [[BPR-0310]]'s, not this
card's, and nothing here brings it closer.

---
schema: "implement-rfc.card.v1"
id: "BPR-0458"
title: "The macOS coding-tidy gate shows fourteen findings nothing has judged"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
---

# The macOS coding-tidy gate shows fourteen findings nothing has judged

`cmake --build --preset dev --target coding-tidy` exits 1 on macOS at
`3124a57`, with fourteen distinct findings across ten files. They were found
while [[BPR-0457]] ran the gate as extra diligence — that card's own three
findings are fixed and none of these are in a file it touched. Reproduced with
BPR-0457's changes stashed, so they are older than that work.

They are **not** [[BPR-0444]]'s. That card is scoped to nine findings in
`src/adapters/github/src/linux_secret_service.cpp`, a Linux-only source. These
are on sources every macOS session compiles, and nothing carded them.

| Count | Check | Where |
|---|---|---|
| 8 | `portability-avoid-pragma-once` | `tests/end_to_end/public_path_poll.hpp:1`, `tests/end_to_end/public_path_wait.hpp:1`, `tests/git_contract/windows_system_root.hpp:1` (each counted once per translation unit that includes it) |
| 6 | `modernize-use-emplace` | `src/core/src/device_authorization_transition.cpp:126,152`; `tests/application/device_authorization_refresh_tests.cpp:186,273,348,375` |
| 2 | `performance-no-automatic-move` | `src/adapters/github/src/github_device_authorization_contract.cpp:213`, `src/adapters/qt/src/github_thread_map.cpp:83` |
| 2 | `readability-identifier-naming` | `tests/git_contract/git_process_contract_tests.cpp:3281`, `tests/platform/macos_service_slot_handshake_tests.cpp:428` |
| 1 | `performance-unnecessary-copy-initialization` | `tests/git_contract/git_cache_contract_tests.cpp:689` |

## Why this is a card rather than a quick fix

Most of these look mechanical, and that is the trap. `portability-avoid-pragma-once`
is the interesting one: three headers use `#pragma once` while the repository's
convention is include guards, and the check fires once per including translation
unit, which is why a count of eight comes from three files. Deciding whether the
convention or the exemption is right is a judgement about the codebase, not a
sed script — the same reasoning [[BPR-0444]] applies to its own set.

`performance-no-automatic-move` at two sites means a `const` local is being
returned where a move was available. That is a real, if small, pessimisation
and worth reading rather than silencing.

## Why the gate was passing before

It was not. `coding-tidy` is an RFC-closeout gate by the board's own
verification-scope rule, not a per-slice one, so it runs rarely — and the last
cards to run it were on other platforms or scoped elsewhere. [[BPR-0443]]
records the same shape of discovery on Linux: a gate that could not execute was
hiding findings older than the card that finally ran it.

## Required final state

- Every finding is judged: repaired, or suppressed with a recorded reason at
  the site, or the check is turned off repository-wide with a recorded reason.
- No finding is silenced by a blanket `NOLINT` with no explanation.
- `cmake --build --preset dev --target coding-tidy` exits 0 on macOS.
- If any finding turns out to be a real defect rather than a style preference,
  it gets its own card rather than being repaired quietly here.

## Verification

```sh
cmake --build --preset dev --target coding-tidy
```

Run it whole. Piping it through `tail` reports the pipe's exit status, not the
gate's.

## Done — 2026-08-26

All fourteen reproduced at `237fe0d` exactly as the table above records, and
none of them came from [[BPR-0447]], which landed in between and touched
`src/app/main.cpp` — a file that appears in neither run.

### What each one turned out to be

| Check | Sites | Judgement |
|---|---|---|
| `portability-avoid-pragma-once` | 3 headers, 8 hits | repaired: path-derived include guards |
| `modernize-use-emplace` | 6 | repaired |
| `performance-no-automatic-move` | 2 | repaired: `const` dropped from the returned local |
| `readability-identifier-naming` | 2 | repaired: renamed to the configured `camelBack` |
| `performance-unnecessary-copy-initialization` | 1 | **suppressed, with the reason at the site** |

The `#pragma once` question the card called the interesting one turned out to
have a plain answer, and it is worth writing down rather than re-deriving:
**162** first-party headers use path-derived include guards and **4** use
`#pragma once`. That is not a convention with exceptions, it is a convention
with drift, so the three the gate can see were converted to
`BPR_TESTS_<PATH>_HPP` guards matching every other header.

The fourth is `src/adapters/qt/assets/dif/generated/dif-sprite-catalog.hpp`. It
is generated, no first-party translation unit includes it, and it is therefore
absent from the compilation database — which is why the gate has never reported
it and why converting it would mean changing a generator to satisfy a check
that cannot see its output. Left as it is, recorded here so the count of four
is not mistaken for three missed.

### The one that is not a repair

`tests/git_contract/git_cache_contract_tests.cpp:689` copies `aArguments` into
`fixtureArguments`, and the copy **is** modified — inside
`#if defined(Q_OS_WIN)`, which prepends `core.longpaths=true`. On macOS that
branch is preprocessed away, so the check sees a local that is only ever read
and asks for a reference. Taking the advice would compile here and break on the
one platform that writes to it.

So it is a `NOLINTNEXTLINE` for that single check with the platform reason
written at the site, which is what the card's "no blanket `NOLINT` with no
explanation" asks for. The `--changed` run confirms it is doing exactly one
thing: `Suppressed 4 warnings (3 in non-user code, 1 NOLINT)`.

### No finding was a defect

The card reserves its own card for anything that turns out to be a real defect
rather than a style preference. Nothing here qualifies. The two
`performance-no-automatic-move` sites are real pessimisations, but each is one
`const` on a returned local in a fixture or label path, repaired in place. No
behaviour changes anywhere in this set.

### Verification

```
cmake --build --preset dev --target coding-tidy   →  clang-tidy passed.  exit 0
cmake --build --preset dev --target coding-format-check  →  exit 0
cmake --build --preset dev                        →  exit 0
```

The gate is the requirement and it was run whole, unpiped, and exits 0. The
full parallel `dev` matrix then passed **218 of 218** at `-j 14` in 295.17 s,
which is what says these are style repairs rather than behaviour changes: the
set touches `src/core`, `src/adapters/github`, `src/adapters/qt`, and five test
suites, and a rename or a dropped `const` that changed anything would show up
there. `qt_composed_github` is among the passes, which the board's own selector
trap says to check whenever `github_thread_map.cpp` is edited.

Getting there needed one detour worth recording. This harness kills a
long-running background command, and the gate is a **serial** clang-tidy over
246 translation units — around 35 minutes. Five attempts were killed partway.
The work was therefore also proven in four exhaustive partitions, which is
independently useful because it names the cost:

| partition | translation units | result |
|---|---|---|
| `src` | 127 | exit 0, 12 m 13 s |
| `tests/end_to_end`, `git_contract`, `platform` | 12 | exit 0 |
| `tests/adapters_qt`, `application`, `contract`, `core` | 73 | exit 0 |
| the remaining six `tests/` directories | 34 | exit 0 |
| **total** | **246** | |

246 is exactly what the full gate reports it is about to run, and the only
compilation-database entries outside those partitions are the two generated
files under `build/` that the gate does not scan. The partitions are the gate's
own set, and the whole-gate run above then confirmed it directly.

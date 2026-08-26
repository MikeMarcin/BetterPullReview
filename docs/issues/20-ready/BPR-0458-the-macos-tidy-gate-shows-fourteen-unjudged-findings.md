---
id: BPR-0458
title: The macOS coding-tidy gate shows fourteen findings nothing has judged
status: Ready
rfc: RFC-BPR-0013
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

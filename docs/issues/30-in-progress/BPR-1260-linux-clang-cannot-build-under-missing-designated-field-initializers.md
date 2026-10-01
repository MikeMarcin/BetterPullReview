---
schema: "implement-rfc.card.v1"
id: "BPR-1260"
title: "Linux Clang cannot build the dev graph or the fuzz path: -Werror turns -Wmissing-designated-field-initializers into errors"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["linux", "mac"]
metadata: {"found_by":"bpr-main-linux, BPR-0970's optional Linux Clang run, 2026-10-01","reservation":"docs/issues/_reservations/BPR-1260--BPR-1260--bb361cf7eed8.json","related_cards":["BPR-0970","BPR-0816"]}
---

# BPR-1260: Linux Clang cannot build the dev graph or the fuzz path: -Werror turns -Wmissing-designated-field-initializers into errors

Found by bpr-main-linux on 2026-10-01, at `a9a486698`, while running
[[BPR-0970]]'s optional fuzz corpus on Linux.

## Observation

The configure command was `cmake --preset dev -B build/fuzz
-DBPR_BUILD_FUZZERS=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`,
with Ubuntu clang 21.1.8. `cmake --build build/fuzz --target parser-fuzzers`
stops at 200 of 657 steps. It reports 64 errors of the form `missing field '…'
initializer [-Werror,-Wmissing-designated-field-initializers]`, in eight core
sources: `appearance_contrast`, `diff_compute`, `diff_semantics`,
`epoch_transition`, `github_window_read`, `review_composition`, `review_cycle`
and `review_cycle_comparison`. These are ninja's first failures, not an
inventory.

The policy treats the two compilers differently:

- `cmake/Warnings.cmake` turns off GCC's counterpart,
  `-Wno-missing-field-initializers` for GNU, but does nothing for Clang.
- `_coding-standard/scripts/tidy.py` already passes
  `-Wno-missing-designated-field-initializers` to clang-tidy.
- `_coding-standard/STANDARD.md` has no rule that designated initializers must
  name every field.

The code relies on default member initializers for the fields it leaves out.
So the documented Linux Clang fuzz path, "Linux Clang needs no runtime
argument" in `docs/development/build-and-test.md`, does not build.

## Scope

- Match GCC's policy for Clang in `cmake/Warnings.cmake`, consistent with
  `tidy.py`. Add the flag only where the compiler knows the warning, because
  an unknown `-Wno-` option is itself an error under `-Werror` on an older
  Apple clang.
- MSVC is unaffected: its branch returns before these options.

## Acceptance

- Linux Clang builds `parser-fuzzers`, and the eight `bpr_fuzz_corpus` rows
  pass.
- The GCC `build/dev` graph and its flags are unchanged.
- The Apple clang build is unaffected, or the Mac station runs it.

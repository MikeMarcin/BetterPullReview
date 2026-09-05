---
schema: "implement-rfc.card.v1"
id: "BPR-0518"
title: "The type-read structured-binding idiom must carry maybe_unused, and a gate says so"
status: "Done"
rfcs: []
depends_on: ["BPR-0512"]
---

# The type-read structured-binding idiom must carry `maybe_unused`, and a gate says so

## Why

[[BPR-0512]] fixed one instance and stated that no other structured binding
in the RFC-0012 tests was used only in an unevaluated context. That was true
when measured; [[BPR-0517]] then added one
(`tests/core/agent_context_supplement_tests.cpp:432`), and `bpr-win`'s next
merge failed the whole Windows Debug build on it (C2220/C4189) with two
more instances of the same shape found beside it. The idiom — a structured
binding over a value-initialised record whose names appear only inside
`decltype` in a `static_assert` — is house style for reading a record's
member types; MSVC is the only compiler that counts that as unused, and the
Windows lane is the only place it is caught, so it will keep arriving faster
than the lane runs unless the tree refuses it first.

## Required final state

- Every instance in `src/` and `tests/` carries `[[maybe_unused]]`.
- A registered contract finds the idiom by shape and fails by file and line
  on any instance without the attribute, so a slice that adds one fails on
  macOS and Linux before it reaches the Windows lane.
- MSVC builds the tree with the three instances attributed.

## Repair — 2026-09-02

The three instances the shape finds
(`agent_context_supplement_tests.cpp:316` and `:432`,
`agent_chat_scope_tests.cpp:705`) carry the attribute;
`tests/scripts/type_read_binding_tests.py` is registered as
`type_read_binding_contract` and refuses the shape without it.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The gate recognises the shape with and without the attribute and finds no unattributed instance | `python3 tests/scripts/type_read_binding_tests.py` | 2/2 | this commit |
| The three files still compile for Clang with the tree's flags | the `build/dev` compile commands with `-fsyntax-only` | exit 0 | this commit |
| MSVC builds the tree | `bpr-win`'s windows-dev Debug build from a checkout of this commit | Recorded below | `37018b7c` |

## Target-native result — 2026-09-02, station `bpr-win` at `37018b7c`

`cmake --build --preset windows-dev --config Debug` exits **0**, with **137
projects linked, zero `error C` lines and zero `warning C` lines** in the whole
log. The `C2220`/`C4189` pair that stopped the build at `5b5f0d65` is gone from
every translation unit, including the third instance this card found that no
station had reported — `agent_chat_scope_tests.cpp:705`.

The gate itself runs here too:

| Claim | Command | Result |
|---|---|---|
| MSVC builds the tree | `cmake --build --preset windows-dev --config Debug` | exit 0, 137 projects, 0 errors, 0 warnings |
| The gate is registered and passes on Windows | `ctest --test-dir build/windows-dev -C Debug -R '^type_read_binding_contract$' --output-on-failure` | **Passed, 0.28 s** |

The gate is the part that matters more than the three fixes. This station
reported the same defect shape twice from two different cards — [[BPR-0512]]
for `agent_chat_scope_tests.cpp` and then `agent_context_supplement_tests.cpp`
from [[BPR-0517]] — because the idiom is house style, MSVC is the only compiler
that objects to it, and this lane was the only place it was caught. A fourth
instance now fails on the author's own machine instead of arriving here a week
later.

The card is terminal.

---
id: BPR-0518
title: The type-read structured-binding idiom must carry maybe_unused, and a gate says so
status: Ready
rfc: none
depends_on: [BPR-0512]
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
| MSVC builds the tree | `bpr-win`'s windows-dev Debug build from a checkout of this commit | **Pending** — target-native, the closing row | Pending |

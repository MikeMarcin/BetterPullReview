---
id: BPR-0512
title: An RFC-0012 test does not compile under MSVC because a structured binding is used only in decltype
status: Ready
rfc: RFC-BPR-0012
depends_on: []
---

# An RFC-0012 test does not compile under MSVC because a structured binding is used only in `decltype`

## Why

`bpr-win` merged `main` at `8948d61a` into `windows-lane` on 2026-09-02 to
rerun [[BPR-0311]]'s ledger command and the Debug tree failed to build, on
exactly one error in the whole tree:

```text
tests\core\agent_chat_scope_tests.cpp(389,37): error C2220: the following warning is treated as an error
tests\core\agent_chat_scope_tests.cpp(389,37): warning C4189: '<structured binding>': local variable is initialized but not referenced
```

The binding, landed by [[BPR-0499]], is named only inside a `decltype` in a
`static_assert` — an unevaluated context. Clang and GCC count that as a use
and MSVC does not, so a test that reads its types off a record compiles on
macOS and Linux and cannot compile on Windows under warnings-as-errors. The
course then fails at `build-failed:windows-dev` thirteen minutes in, before
any row it exists to produce.

## Required final state

- `tests/core/agent_chat_scope_tests.cpp` compiles under MSVC with
  warnings-as-errors, with the type assertion kept rather than removed.
- The same test still compiles and passes on macOS and Linux.
- [[BPR-0311]]'s ledger command gets past `build-failed:windows-dev` on the
  Windows station from a checkout of the fix.

## Repair — 2026-09-02

`[[maybe_unused]]` on the binding, with a comment saying which compilers
disagree and why. No other structured binding in the RFC-0012 tests is used
only in an unevaluated context; the station's whole-tree build found this one
error.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The fix is well-formed for Clang with the tree's own flags | the `build/dev` compile command for the file, run with `-fsyntax-only` | exit 0 | this commit |
| Format | `clang-format --dry-run --Werror tests/core/agent_chat_scope_tests.cpp` | clean | this commit |
| The test still passes on macOS | `ctest --preset dev -R '^core_agent_chat_scope$' --output-on-failure` | Pending on this host until the tree frees; the change cannot alter behaviour | Pending |
| MSVC builds the tree and the ledger command passes the build step | `bpr-win`'s rerun of BPR-0311's exact command from a checkout of this commit | **Pending** — target-native, the card's closing row | Pending |

---
id: BPR-0478
title: Remove the orphaned github_window test source
status: Done
rfc: RFC-BPR-0005
depends_on: []
completed: 2026-08-28
---

# Remove the orphaned github_window test source

Found by BPR-0101's independent closeout review. When BPR-0191 deleted
`github_window` and carried its 2,074 lines of assertions into
`composed_github_tests.cpp`, the carry was a copy, not a move: the original
`tests/adapters_qt/github_window_tests.cpp` stayed in the tree, includes the
deleted `bpr/qt/github_window.hpp`, and is referenced by no CMake file — an
uncompilable edit trap that any grep-driven change could touch believing it
tests something. The ledger's "moved to `composed_github_tests.cpp`" wording
is corrected in the same slice.

The file is removed. Evidence: no build file references it (`grep -rn
github_window_tests` over `tests/` and CMake sources returns only the file
itself before removal and nothing after), the dev build completes without it,
and the Qt selector set covering the carried assertions passes unchanged.

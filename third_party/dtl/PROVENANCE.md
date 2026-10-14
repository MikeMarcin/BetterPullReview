dtl 1.21

Source: https://codeload.github.com/cubicdaiya/dtl/tar.gz/refs/tags/v1.21

SHA256: 90ed2dbf4e6d687737fe25f118bbcb6aed778cecc3f2115d191a032bf8643dbd

Archive size: 26321 bytes, the release tag RFC-BPR-0082's evaluation
([[BPR-1466]], candidate `c-onp-intraline`) measured. dtl (Diff Template
Library) is a header-only C++ library of the O(NP) sequence comparison of Wu,
Manber and Myers. `cmake/BprDtlProvider.cmake` extracts only the `dtl/`
headers and `COPYING` from the verified archive and puts them on the include
path; no upstream build, tests or examples run. It is one of the
owner-approved pure core libraries (RFC-BPR-0082 Q2, 2026-10-13), approved for
per-line intraline comparison only. The diff engine's intraline ranges call
only `dtl::Diff` over one similar line pair's Unicode scalars, in
`src/core/src/diff_intraline.cpp` ([[BPR-1481]]); dtl never compares a file's
lines.

dtl 1.21 restarts past 2,000,000 recorded path points on a path that can join
unequal elements, and allocates with a plain `new[]` that an exception
unwinding through it leaks. The core compares only pairs whose
(left + 1) x (right + 1) grid is below that bound, so no restart occurs, checks
every common scalar before use, and never lets a stop unwind through dtl.

BSD-3-Clause license retained alongside this record (`COPYING` is upstream
`COPYING`, verbatim).

License SHA256: 7331165008d2c296d7cd0fc65ed752cd169fd7e4604a6413f9a1eef8c0e14fec.

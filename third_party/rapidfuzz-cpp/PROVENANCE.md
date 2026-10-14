rapidfuzz-cpp 3.3.4

Source: https://codeload.github.com/rapidfuzz/rapidfuzz-cpp/tar.gz/refs/tags/v3.3.4

SHA256: a0dd2ef361cac165e12076696e7c7e8d069a2908abd9599ad4bd190de33f9881

Archive size: 314599 bytes, the release tag RFC-BPR-0082's evaluation
([[BPR-1466]], candidate `c-rapidfuzz`) measured. rapidfuzz-cpp is a
header-only C++ library of string similarity measures.
`cmake/BprRapidFuzzProvider.cmake` extracts only the `rapidfuzz/` headers and
`LICENSE` from the verified archive and puts them on the include path; no
upstream build, tests, benchmarks, fuzzers or install rules run. It is one of
the owner-approved pure core libraries (RFC-BPR-0082 Q2, 2026-10-13). The diff
engine's line-pair similarity calls only `rapidfuzz::CachedIndel` from
`rapidfuzz/distance/Indel.hpp`, in `src/core/src/diff_compute.cpp`
([[BPR-1480]]).

MIT license retained alongside this record (`LICENSE` is upstream `LICENSE`,
verbatim).

License SHA256: 529ac8f069bd6aa633a2da2bea8de4299f43dec428aa407585f2b460dd98944f.

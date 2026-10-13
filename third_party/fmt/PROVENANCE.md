{fmt} 12.2.0

Source: https://github.com/fmtlib/fmt/releases/download/12.2.0/fmt-12.2.0.zip

SHA256: a2f4a8d51178f954e4c339007f77edd76ba0cb2e36f87a48e5a5403d9be5878f

Archive size: 1486029 bytes. The digest matches the one GitHub publishes for
the 12.2.0 release asset (published 2026-06-16). `cmake/BprFmtProvider.cmake`
uses it header-only (`FMT_HEADER_ONLY`); no upstream build, tests, install
rules or `os.cc` file I/O run. It is one of the owner-approved pure core
libraries (2026-10-13), chosen by the owner over `std::format`, and first
formats the GitHub publication's RFC 3339 commit dates ([[BPR-1439]]). Quill's
bundled fmt is a separate copy in its own namespace (`third_party/quill/`).
MIT license retained alongside this record.

License SHA256: a65fcc5a1095fc33cc37e4fff69366e49d7e92f613558a2b95ed1b70e33758b4.

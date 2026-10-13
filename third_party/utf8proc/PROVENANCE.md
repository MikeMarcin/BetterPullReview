utf8proc 2.12.0

Source: https://codeload.github.com/JuliaStrings/utf8proc/tar.gz/refs/tags/v2.12.0

SHA256: f564011d38b2888d583d510b08e69ffa15aa117155db1b9b49ef1dfe1fa25111

Archive size: 207976 bytes. `cmake/BprUtf8procProvider.cmake` compiles only
`utf8proc.c` (which includes its generated Unicode data) as a static library
with `UTF8PROC_STATIC`; no upstream build, tests or install rules run. It is
one of the owner-approved pure core libraries (2026-10-13) and serves the
functional core's UTF-8 decoding, canonical decomposition and case mapping
through `bpr/core/utf8.hpp` ([[BPR-1434]]). Unicode data version 18.0.0.
MIT license (with the Unicode data notice) retained alongside this record.

License SHA256: 3b510150d34f248a221bb88e1d811238d6c6c18b51231822c42974c39bb07256.

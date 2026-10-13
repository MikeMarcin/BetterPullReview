zlib 1.3.2

Source: https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz

SHA256: bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16

Archive size: 1502830 bytes. `cmake/BprZlibProvider.cmake` compiles only the
raw-inflate and checksum sources (adler32, crc32, inffast, inflate, inftrees,
zutil) as a static library with `Z_PREFIX`, so its symbols are `z_*` and never
collide with a system zlib. No gz* file I/O, deflate or upstream build runs.
It is one of the owner-approved pure core libraries (2026-10-13) and serves the
functional core's review-bundle reader ([[BPR-1435]]). zlib license retained
alongside this record.

License SHA256: e32ff4e00d9d94930537635291da39e7e612703334bf6fde8c7f1686fe8a45a2.

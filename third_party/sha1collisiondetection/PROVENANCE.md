sha1collisiondetection a06f1adbe5522f3c939cd6a3f13d5ec7f4c1a988

Source: https://codeload.github.com/cr-marcstevens/sha1collisiondetection/tar.gz/a06f1adbe5522f3c939cd6a3f13d5ec7f4c1a988

SHA256: b9c89e19a1b02cb623d81515b5a281781a8c3f2296a2548854f5e1455b1b1729

Archive size: 787039 bytes, the upstream master commit of 2026-10-13 that
RFC-BPR-0080's evaluation ([[BPR-1446]]) measured. It is the SHA-1 with
collision detection that Git and GitHub use. `cmake/BprSha1dcProvider.cmake`
compiles only `lib/sha1.c` and `lib/ubc_check.c` as a static library, as Git
configures them: `SHA1DC_NO_STANDARD_INCLUDES`,
`SHA1DC_INIT_SAFE_HASH_DEFAULT=0` (plain SHA-1 output, never a substituted
digest), and `SHA1DC_CUSTOM_INCLUDE_SHA1_C` and
`SHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C` naming `sha1dc_includes.h` beside this
record (memory functions, fixed-width integers and `abort`). Collision
detection and unavoidable-bit-condition checks stay on; reduced-round
detection stays off. No upstream build, tests or command-line tool run. It is
one of the owner-approved pure core libraries (RFC-BPR-0080 Q1, 2026-10-13)
and computes every SHA-1 Git object identity in the product, refusing an input
it detects as part of a collision attack ([[BPR-1450]]). MIT license retained
alongside this record (`LICENSE` is upstream `LICENSE.txt`, verbatim).

License SHA256: 9daac99c665ddc38fde6d7c91b9173c112d5e6c6e7e5f7d8bb8747c630efc83d.

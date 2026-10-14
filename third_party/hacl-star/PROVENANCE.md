HACL* 504c2987452f87fe44bce9b9f12e19d6e051761f

Source: https://codeload.github.com/hacl-star/hacl-star/tar.gz/504c2987452f87fe44bce9b9f12e19d6e051761f

SHA256: e32f7add802f44e12c3f347824ddfd074abfe5cd5ccb291a942786a8c36af52c

Archive size: 10228408 bytes, the upstream main commit of 2026-10-13 that
RFC-BPR-0080's evaluation ([[BPR-1446]]) measured. HACL* is a formally
verified cryptographic library written in F* and extracted to C by KaRaMeL.
`cmake/BprHaclSha2Provider.cmake` extracts only the generated `dist/`
directory and compiles only `dist/gcc-compatible/Hacl_Hash_SHA2.c`
(`dist/msvc-compatible/Hacl_Hash_SHA2.c` under MSVC, the flavour HACL*
generates for that compiler) as a static library, with `dist/<flavour>`,
`dist/karamel/include` and `dist/karamel/krmllib/dist/minimal` as include
directories. No F* or KaRaMeL toolchain, upstream build or tests run. It is one
of the owner-approved pure core libraries (RFC-BPR-0080 Q2, 2026-10-13) and
computes every SHA-256 the core computes, through the block-level
`Hacl_Hash_SHA2_sha256_init`, `_update_nblocks`, `_update_last` and `_finish`
calls only; the allocating streaming API in the same file is never called
([[BPR-1451]]).

The generated C carries, and is distributed under, the MIT license in
`dist/LICENSE.txt`, retained alongside this record (`LICENSE` is upstream
`dist/LICENSE.txt`, verbatim). The repository's top-level Apache-2.0 `LICENSE`
(SHA256 c5accbbd8546e94c34aed24afe689a617627d18eed5a6c48277e48db57c23851)
covers the F* sources, none of which is compiled or distributed.

License SHA256: 998ce04fb8ad9dedb0bc1b44938f8c3dcf1089780fa105ce1c8c30fb5554d78c.

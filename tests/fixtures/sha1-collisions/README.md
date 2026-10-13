# SHA-1 collision inputs

[[BPR-1450]]. Public SHA-1 collision pairs, used by `core_git_objects` to
show that the core SHA-1 (sha1collisiondetection, as Git builds it) refuses
them as raw byte streams instead of giving their digest (RFC-BPR-0080 AC03).

Source: the `test/` directory of sha1collisiondetection commit
`a06f1adbe5522f3c939cd6a3f13d5ec7f4c1a988`, archive
`https://codeload.github.com/cr-marcstevens/sha1collisiondetection/tar.gz/a06f1adbe5522f3c939cd6a3f13d5ec7f4c1a988`
(SHA-256 `b9c89e19a1b02cb623d81515b5a281781a8c3f2296a2548854f5e1455b1b1729`,
787039 bytes), the archive `cmake/BprSha1dcProvider.cmake` pins. MIT
(`third_party/sha1collisiondetection/LICENSE`).

| File | Bytes | Taken from | SHA-256 | Plain SHA-1 |
|---|---|---|---|---|
| `shattered-1-prefix-320.bin` | 320 | first 320 bytes of `test/shattered-1.pdf` | `cac8644dba1a9aef70cc268f3794036a2be5b5107109ad742247858fd1a36990` | `f92d74e3874587aaf443d1db961d4e26dde13e9c` |
| `shattered-2-prefix-320.bin` | 320 | first 320 bytes of `test/shattered-2.pdf` | `842a2c7d2f85b25998d5e43fcced0ba3ca570ee0d36bedb23a815d79e614f646` | `f92d74e3874587aaf443d1db961d4e26dde13e9c` |
| `sha-mbles-1.bin` | 640 | `test/sha-mbles-1.bin`, whole | `3ead211681cec93d265c8ac123dd062e105408cebf82fa6e2b126f4f40bcb88c` | `8ac60ba76f1999a1ab70223f225aefdc78d4ddc0` |
| `sha-mbles-2.bin` | 640 | `test/sha-mbles-2.bin`, whole | `208feafe1c6a95c73f662514ac48761f25e1f3b74922521a98d9ce287f4a2197` | `8ac60ba76f1999a1ab70223f225aefdc78d4ddc0` |

SHAttered's two PDFs (Stevens, Bursztein, Karpman, Albertini, Markov, 2017;
SHA-256 `2bb787a7…` and `d4488775…`, both with plain SHA-1
`38762cf7f55934b34d179ae6a4c80cadccbb7f0a`) differ only inside their first
320 bytes, which end on a block boundary with equal internal state, so the
prefixes collide on their own. SHA-mbles is the chosen-prefix collision of
Leurent and Peyrin (2020). The plain SHA-1 column is the control: macOS
`shasum` (a SHA-1 without collision detection) gave it on 2026-10-13, and
`hash_evaluation_collisions` recomputes the same collision with the
evaluation's plain SHA-1 arm.

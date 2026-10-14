Git 2.56.0 xdiff

Source: https://mirrors.edge.kernel.org/pub/software/scm/git/git-2.56.0.tar.xz

SHA256: 26c56c296b38c0695b26fa95f475f1d01704d2d38e73465ca30b0b2f5dc789d3

Archive size: 8489656 bytes, the Git release RFC-BPR-0082's evaluation
measured as candidate `l-myers-indent` (Myers with the indent heuristic).
`cmake/BprXdiffProvider.cmake` re-extracts only `xdiff/`, `LGPL-2.1` and
`COPYING` from the verified archive and compiles `xdiffi.c`, `xemit.c`,
`xhistogram.c`, `xmerge.c`, `xpatience.c`, `xprepare.c` and `xutils.c` as
**its own shared library**, `bpr_xdiff` (`libbpr_xdiff.dylib`,
`bpr_xdiff.dll`, `libbpr_xdiff.so`). No other part of Git is compiled, and no
static or object copy exists: the owner accepted LGPL xdiff only "if xdiff is
dynamically linked as its own shared library, as Qt is" (RFC-BPR-0082 Q1,
2026-10-13). Only the xdiff adapter, `src/adapters/xdiff`, links it, and
`architecture_boundaries` enforces both ([[BPR-1485]]). The product calls it
for Myers with the indent heuristic; `xhistogram.c` is compiled only because
`xdiffi.c` refers to it, and histogram is never selected.

## Per-file licences

Confirmed against the headers of every file in the pinned archive's `xdiff/`:

| Files | Copyright | Licence (SPDX) |
|---|---|---|
| `xdiff.h`, `xdiffi.c`, `xdiffi.h`, `xemit.c`, `xemit.h`, `xinclude.h`, `xmacros.h`, `xprepare.c`, `xprepare.h`, `xtypes.h`, `xutils.c`, `xutils.h` | (C) 2003 Davide Libenzi | `LGPL-2.1-or-later` |
| `xmerge.c` | (C) 2003-2006 Davide Libenzi, Johannes E. Schindelin | `LGPL-2.1-or-later` |
| `xpatience.c` | (C) 2003-2016 Davide Libenzi, Johannes E. Schindelin | `LGPL-2.1-or-later` |
| `xhistogram.c` | (C) 2010 Google Inc. and other copyright owners as documented in JGit's IP log | Eclipse Distribution License 1.0, `BSD-3-Clause` |

`xhistogram.c` is the derivation from JGit's HistogramDiff that RFC-BPR-0082
flagged. Its whole header is the Eclipse Distribution License 1.0, whose terms
are the three-clause BSD licence; SPDX has no separate EDL-1.0 identifier and
matches it as `BSD-3-Clause`. The library's declared licence is therefore
`LGPL-2.1-or-later AND BSD-3-Clause` (`packaging/dependencies.json`, and the
release SPDX package `SPDXRef-git-xdiff`).

The rest of the archive is Git, mostly `GPL-2.0-only` (`COPYING`). None of it
is compiled or linked; the archive is carried whole only as corresponding
source.

## Retained texts

| File | What it is | SHA256 |
|---|---|---|
| `LGPL-2.1` | upstream `LGPL-2.1`, verbatim: Git's note that `xdiff/` is LGPL-2.1-or-later, then the licence | `1922f45d2c49e390032c9c0ba6d7cac904087f7cec51af30c2b2ad022ce0e76a` |
| `xhistogram-EDL-1.0.txt` | the leading comment of upstream `xdiff/xhistogram.c`, verbatim | `68185dde26b293f5390f2bfb4485ee26e26ec3a539b1ab7064b5c8af22ba2c3c` |
| `NOTICE` | BetterPullReview's notice for the library: copyright lines, licences, the replaceable shared library and where its source is | - |

Upstream `COPYING` SHA256, which the provider also checks:
`5b2198d1645f767585e8a88ac0499b04472164c0d2da22e75ecf97ef443ab32e`.

`NOTICE`, `LGPL-2.1` and `xhistogram-EDL-1.0.txt` are carried whole into every
installed `THIRD-PARTY-NOTICES.txt` (`NOTICE_FILES` in
`packaging/package_runtime.py`, which the release security audit checks too).

## BetterPullReview's build inputs

`git-compat-util.h` and `bpr-xdiff-regex.h` beside this record replace Git's
own compatibility header for this build. They are part of the library's
corresponding source and licensed like it, `LGPL-2.1-or-later`. With
`cmake/BprXdiffProvider.cmake`, the script that compiles the library, they are
pinned by SHA-256 as the `buildInputs` of the `git-xdiff` entry in
`packaging/dependencies.json`, and every release's corresponding-source asset
carries them under `git-xdiff-2.56.0-build/` beside the upstream archive.
Installing the library is copying that one file into place.

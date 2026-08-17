# KSyntaxHighlighting provenance

RFC-BPR-0006 mandates syntax highlighting through KSyntaxHighlighting's
low-level API, so the library is not optional. Only its acquisition route was
open, and the reasoning is recorded in the
[RFC-BPR-0006 ledger](../../docs/verification/RFC-BPR-0006.md).

Caller-installed discovery was investigated first, because the library is
Qt-dependent and the dependency boundary at that time treated Qt as a
caller-installed package that is never fetched. Qt has since moved to its own
pinned provider, which does not change the outcome recorded here. No Homebrew
formula exists for
KSyntaxHighlighting or for the KDE Frameworks on the development host, so
requiring a caller-installed package would have made the RFC unimplementable
rather than deferred. Verified CPM source acquisition, following the SQLite
precedent, is therefore the only route that satisfies the RFC.

## Pinned inputs

| Input | Version | Upstream |
|---|---|---|
| KSyntaxHighlighting | 6.28.1 | `https://download.kde.org/stable/frameworks/6.28/syntax-highlighting-6.28.1.tar.xz` |
| extra-cmake-modules | 6.28.0 | `https://download.kde.org/stable/frameworks/6.28/extra-cmake-modules-6.28.0.tar.xz` |

extra-cmake-modules is not a product dependency. It is a transitive *build*
input that KSyntaxHighlighting's CMake project requires, and it carries its own
pin, digest, and record here because an unpinned build input is as much a
supply-chain surface as an unpinned library.

## Digests

| Input | Archive SHA-256 | Extracted-source SHA-256 |
|---|---|---|
| KSyntaxHighlighting 6.28.1 | `fe0d4133af62c6b9c0cf7728928c64d2deb55fe808a264a5de871f4b6bc86f65` | `b60bfc55ecf48aeddffa1c2c6cc36afc4ba72c9030483c242af551904459ff85` |
| extra-cmake-modules 6.28.0 | `a32e24b267e8528d0253bc8df18bdc00e676560a43b796533e1b1406f4eef4db` | `ec6fc8bf205d88517b354ee54941cfe83d45eab195f9d8bad8a6539ff4f49d3a` |

The extracted-source digest is computed over a sorted manifest of every file's
own digest, so it detects a changed file, an added file, and a removed one. The
provider rejects a symbolic link anywhere in the extracted tree rather than
following it.

## Toolchain requirement

KSyntaxHighlighting 6.28.1 requires Qt 6.9 or newer, which is
above the BetterPullReview floor. The provider states that minimum itself and
fails with an explicit message naming both versions, rather than letting the
nested configure fail somewhere less legible.

## What acquisition does not change

Runtime behaviour is unchanged by how the library is obtained. RFC-BPR-0006's
non-goals forbid downloading grammars or themes: definitions and approved
themes remain packaged as resources, and standard-path discovery and downloads
stay disabled in the production build. Build-time source acquisition and
runtime grammar downloading are different things, and only the second is
prohibited.

The Qt-free `core-only` preset does not acquire this dependency and must not.
Its cache contains no reference to it, and it continues to build and test
without Qt.

# Rebuild the separately replaceable xdiff library

This standalone project needs CMake 3.21 or newer and a native C compiler. It
uses only this source asset: no BetterPullReview checkout, CPM, Qt, downloads,
or shared dependency cache. Keep the extracted asset's directory layout.
`cmake/BprXdiffProvider.cmake` records the application's original acquisition
and build; use this standalone entry point to rebuild the recipient's copy.

First verify the asset and its installed `source-provenance.json` against the
release checksums. Inside the asset, `git-xdiff-2.56.0.tar.xz` is the unmodified
Git release (8,489,656 bytes), with SHA-256:

```text
26c56c296b38c0695b26fa95f475f1d01704d2d38e73465ca30b0b2f5dc789d3
```

`cmake -E sha256sum /absolute/path/to/git-xdiff-2.56.0.tar.xz` prints the
digest on all three platforms. Stop if it differs. Extract that archive into
an owned working directory with your archive tool (or `cmake -E tar xf` from
that directory). It creates `git-2.56.0/`. You may then edit that extracted
source and the local compatibility headers; rebuilding intentionally permits
modified LGPL source. Verification of the original distributed archive does
not prohibit modifications to your working copy.

From any working directory, substitute absolute paths in these commands:

```text
cmake -S /absolute/asset/git-xdiff-2.56.0-build/third_party/git-xdiff/rebuild -B /absolute/owned/xdiff-build -DBPR_XDIFF_SOURCE_DIR=/absolute/owned/git-2.56.0 -DCMAKE_BUILD_TYPE=Release
cmake --build /absolute/owned/xdiff-build --config Release
```

On macOS add `-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64` (quote that whole
argument in a shell) and `-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0` for the universal
release. On Windows run the commands in the supported x64 MSVC developer
environment; with a Visual Studio generator also select `-A x64`. Keep the
default dynamic MSVC runtime. On Linux use the package's declared Ubuntu
26.04 x86_64 toolchain/runtime baseline. Different toolchains can produce
different bytes; matching source does not recreate a publisher signature.

The build compiles exactly `xdiffi.c`, `xemit.c`, `xhistogram.c`, `xmerge.c`,
`xpatience.c`, `xprepare.c`, and `xutils.c`, as C11 into a shared library.
It uses the shipped `git-compat-util.h` and `bpr-xdiff-regex.h`, and the
self-contained `cmake/Hardening.cmake` helper in this asset. That preserves
the original build's output name, default C visibility, Windows automatic
exports, position-independent code and platform hardening options. Git's
top-level compatibility header and all other Git code are excluded.

The output is `libbpr_xdiff.dylib`, `bpr_xdiff.dll`, or `libbpr_xdiff.so`;
multi-configuration generators place it in the `Release` subdirectory.
Keep the `xdiff/xdiff.h` C interface and target architecture compatible.
Relink your consumer against this shared library (and, on Windows, its
generated import library). For the application, work on an owned copy of the
installation and replace only its matching private xdiff library:

- macOS: `Contents/Frameworks/libbpr_xdiff.dylib`.
- Windows: `bpr_xdiff.dll` beside `better_pull_review.exe`.
- Linux: `libbpr_xdiff.so` beside the application in `lib/betterpullreview`.

On macOS seal only your modified bundle copy locally with
`codesign --force --deep --sign - --timestamp=none /absolute/owned/BetterPullReview.app`.
No publisher credentials are required. A locally modified copy has no claim
to the original publisher's trust or updater eligibility. The installed
`SOURCE-OFFERS.txt` retains the full replacement, source-access and retention
terms. License texts are in the original archive and installed notices.

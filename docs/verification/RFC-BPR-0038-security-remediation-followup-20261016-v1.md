# RFC-BPR-0038 security remediation follow-up — 2026-10-16, v1

**The requested PCRE2 and Windows zlib fixes remain blocked on an updated
official Qt release.** The owner explicitly requires that route and rejects
maintaining a Qt fork. Qt 6.12.0 does not contain both required fixes, so this
follow-up changes no dependency pin or delivered package.

The additional investigation supports the three D-Bus applicability proposals
and reproduces the Apple zlib negative-length failure through exported aliases.
The Apple row remains pending. BPR-0962 remains **Blocked**, with AC1/AC2 pending
and AC3 failed.

This is an engineering follow-up by task
`01a14646-bfaa-7fd3-bc0b-a152e9ea9900`, starting at
`af4eef944c94ca850ccf5dbd241cc1c7a6444f70`. It preserves the
[independent v1 review](RFC-BPR-0038-independent-security-review-20261016-v1.md),
packet, queue, proposals and raw reports. The
[machine-readable follow-up](RFC-BPR-0038-security-remediation-followup-20261016-v1.json)
binds their hashes, all 64 row identities, exact artifacts, new observations,
source identities and reproducible bounded probes. It proposes no
security-owner acceptance, and its new engineering conclusions still need
independent review.

## Official Qt is the permitted remediation route

The owner's constraint is: “If they come with Qt then I will only accept updated
Qt releases, I'm not maintaining a fork of Qt”. Both affected components are
bundled in the inspected QtCore: PCRE2 on both platforms and native zlib on
Windows. Apple libz is a separate OS dependency on macOS.

The official release indexes list [Qt 6.11.2](https://download.qt.io/official_releases/qt/6.11/)
and [Qt 6.12.0](https://download.qt.io/official_releases/qt/6.12/) as of this check.
The newer QtBase tag resolves to
`025bdad181de241e81bf853c8a2d7bf3d19261a9`.

| Component | Current Qt 6.11.2 | Checked Qt 6.12.0 | Remaining requirement |
|---|---|---|---|
| PCRE2 | 10.47 | 10.48 | An official Qt release containing the applicable fixes, including the large-JIT-stack fix in PCRE2 10.49 |
| Windows native zlib | 1.3.2 | 1.3.2, identical `gzwrite.c` | An official Qt release containing the failed-write state-reset fix |

This conclusion checks code as well as version labels. Qt 6.12.0's
[JIT source](https://github.com/qt/qtbase/blob/025bdad181de241e81bf853c8a2d7bf3d19261a9/src/3rdparty/pcre2/src/pcre2_jit_compile.c#L14088)
still uses the fixed growth step without the large-allocation handling in
[PCRE2's correction](https://github.com/PCRE2Project/pcre2/commit/2b4038298072684b0fae29b15bedfb1a75bda46d).
Its [failed-write branch](https://github.com/qt/qtbase/blob/025bdad181de241e81bf853c8a2d7bf3d19261a9/src/3rdparty/zlib/src/gzwrite.c#L245)
lacks the two resets in the
[zlib correction](https://github.com/madler/zlib/commit/df84af25dc1942490e1d1c899a07619152a46148).
The complete `gzwrite.c` hash is identical in both Qt releases:
`2ce783294c688330c716bfe106f43167d3dae5be84994176bc304d205d4f5b04`.

**MAC-001/007 and WIN-001/007** retain the partial-JIT-compilation question.
The outside-feed **GHSA-r9hj-j2rw-4q3m** remains pending; upstream now also
identifies it as **CVE-2026-103111**. That alias adds no scanner row.
**WIN-013/026/027** retain `remediate` decisions. Neither a private patch nor
disabling JIT or suppressing a scanner result is recorded as a fix.

The next eligible dependency change requires a suitable official Qt release,
then aligned provider/source pins and exact-package qualification through the
existing release tools. New source, binaries, SPDX, notices and corresponding
source must agree; fresh unsuppressed scans and relevant package/public-path
checks must run. Historical package acceptance cannot transfer to new bytes.

## D-Bus applicability gaps

**MAC-025–027** now have a supported, narrow applicability proposal. QtGui
contains Unix desktop-service code and links QtDBus, but the shipped Cocoa
integration constructs
[QCocoaServices](https://github.com/qt/qtbase/blob/ef55f427f2c8b410d34f8a7681020a3000cf6866/src/plugins/platforms/cocoa/qcocoaintegration.mm#L417),
which derives directly from `QPlatformServices` and uses Apple services.
Qt's Mac build excludes the generic Unix theme/menu/tray consumers;
the bound package contains no Xcb, Wayland, VNC or external platform-theme
plugin. The source audit found no first-party, QtNetwork, QtWidgets or shipped
Cocoa/style/network/TLS plugin D-Bus caller.

QtDBus loads external libdbus lazily. A standalone process loaded the exact
packaged QtWidgets framework and all four shipped plugins on arm64 and Rosetta
x86_64. Each run observed **one QtDBus image and zero external libdbus images**.
A deliberate load of a harmless marker library then produced one external
image, verifying that the detector can observe such a load.

The source path and initialization evidence support excluding the external
D-Bus parser from this packaged Cocoa composition. This is an applicability
inference, not an active GUI trace or a statement that every installed D-Bus
library is patched. Changed consumers, plugins, platform selection, Qt build
options or package bytes require a new assessment. Independent review and
security-owner acceptance remain pending.

## Apple zlib aliases and vendor mapping

**MAC-032 remains pending.** The prior observation that `*64` symbols are
absent does not exclude the bug. On macOS **26.6.2 (25G83)**, both arm64 and
Rosetta x86_64 export `crc32_combine` and `crc32_combine_gen` from
`/usr/lib/libz.1.dylib`. All four calls with length **-1** timed out at two
seconds; their owned processes were killed and reaped. Lengths 0 and 1 returned
promptly. The two `*64` entry points were absent on both architectures.

Apple's published distribution tags map 26 OS releases to eight zlib source
revisions. The inspected macOS 13 source uses an older nonpositive-length
guard; the inspected macOS 14, 15 and 26 revisions lack the relevant guard.
The [exported aliases reach the affected implementation](https://github.com/apple-oss-distributions/zlib/blob/3dbd11404a18ac1d6ed3ca9c1d6a01d049bf8674/zlib/crc32.c#L1113).
These source mappings do not assert that every corresponding OS binary was
executed, or map unpublished 26.6.2 source to its binary.

All **14 shipped Mach-O images / 28 slices** import none of the CRC-combine
family. The exact first-party producer sources, Qt consumers,
KSyntaxHighlighting and LibArchive contain no caller. The inspected Apple
`crc32_z` internal helper receives a nonnegative remaining-block count.
These observations narrow applicability but do not establish the required
complete delivered closure, including proprietary OS-framework internals,
across the supported native matrix. The standalone timeout is not a reproduced
BPR exploit. No Apple runtime-fix claim follows from its ABI version, and a Qt
update would not replace this OS-owned library.

## Coverage and validation

The independent v1 decisions remain **53 accept / 8 insufficient-evidence /
3 remediate**. This follow-up adds evidence supporting acceptance of the three
D-Bus proposals. If independently accepted, the combined technical count would
be **56 / 5 / 3**. Both outside-feed entries retain their prior decisions, and
every security-owner decision remains pending. Raw Mac/Windows scans remain
**33/31 matches, both exit 2**.

The two guarded probe courses preserved the live project configuration and Qt
provider bytes. The source/import audit passed its 13 checks and 28 slice
checks. Record consistency, all 64 inspected local Markdown links and all
**67 documentation unit tests passed**. The full documentation gate failed on
the existing issue-counter drift: checkout `BPR-1558` is below shared historical
high water `BPR-1566`. No counter override or shared registration change was
made. Exact commands and results are in the JSON's `validation` field. A
successful harness exit does not turn the observed Apple failure into a pass.
No application build is claimed for this evidence-only change.

Recheck before the next release, on an official Qt update or changed
source/package/platform/build-option/OS/advisory input, or by **2026-11-15**,
whichever occurs first. No background monitoring is scheduled. The
[continuation checkpoint](../issues/_checkpoints/RFC-BPR-0038/security-remediation-continuation-20261016.json)
preserves the exact remaining work.

---
schema: "implement-rfc.card.v1"
id: "BPR-2360"
title: "Upgrade the pinned Qt runtime to 6.12.1"
status: "Done"
rfcs: []
depends_on: ["BPR-1665"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner, 2026-11-23","mandate_revision":"qt-6-12-1-r1","assigned_implementer":"/root/qt_6_12_1","reservation":"docs/issues/_reservations/BPR-2360--BPR-2360--2ce2f43e9d3b.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ba5ee12b737997299b0d98c06c25a074a78e1103","evidence":{"kind":"observation","detail":"Fresh and offline macOS dev configure select official Qt 6.12.1 with Svg and WebP. Exact caller root accepted. Prior 6.12.0, missing WebP and empty offline cache refused. Pinned installers resolve the three desktop layouts, with matching Windows and Linux artifacts inspected."}},{"criterion":"AC2","outcome":"pass","revision":"ba5ee12b737997299b0d98c06c25a074a78e1103","evidence":{"kind":"observation","detail":"All three corresponding source SHA-256 pins match two Qt-listed mirrors. Qt source checksum URLs were unavailable. Linux ICU archive matches Qt-published SHA-256, raw trio matches prior pins, and QtCore ELF and producer SPDX bind ICU 73.2. Packaging, release source assets and release security contracts passed."}},{"criterion":"AC3","outcome":"pass","revision":"ba5ee12b737997299b0d98c06c25a074a78e1103","evidence":{"kind":"command","detail":"cmake --preset dev and cmake --build --preset dev with the app and two focused Qt targets succeeded on macOS arm64. Six offscreen CTest rows passed and an instrumented repeat loaded QtCore 6.12.1. Architecture and documentation checks passed. Native Windows and Linux execution and release qualification were outside this patch milestone."}}]
---

# BPR-2360: Upgrade the pinned Qt runtime to 6.12.1

The owner requested “Upgrade to Qt 6.12.1”. This patch upgrade follows
[RFC-BPR-0088](../../rfcs/RFC-BPR-0088-qt-6-12.md), which qualified 6.12.0.

## Acceptance

- AC1. The provider selects official Qt 6.12.1 and the existing modules.
  The Windows MSVC, Linux GCC and macOS Clang package layouts resolve.
- AC2. The corresponding-source archives and Linux ICU companion archive
  have verified SHA-256 digests, including the three redistributed ICU
  binaries. Focused packaging and source checks pass.
- AC3. The public configure, build and offscreen application path uses
  Qt 6.12.1 on the available Mac. Active documentation reflects the new
  pins, and the documentation gate passes.

## Plan

Query the official repositories with the pinned acquisition tools. Check
the source and companion archive bytes before updating the CMake pin, release
source inventory, ICU provenance and current build documentation. Configure
with the default provider, run a focused build and app self-test, and record
the platform coverage. Native Windows and Linux release qualification and
publication belong to subsequent release work.

## Verification

Built candidate `ba5ee12b737997299b0d98c06c25a074a78e1103` on macOS arm64
with CMake 4.4.4 and Apple Clang 21.0.0. The `dev` preset acquired the SDK
from a fresh Qt cache and configured again offline. An explicit 6.12.1 prefix
worked. The provider refused a 6.12.0 prefix, a prefix missing WebP, and an
empty offline cache. Both architectures of QtCore declare macOS 14.4.

The pinned installers resolved Mac Clang, Windows MSVC 2022 x64 and Linux GCC
x64. The Windows and Linux QtBase, Svg and ImageFormats archives matched
Qt's published hashes and contained the expected binaries. The Linux QtCore
ELF needs ICU data, i18n and uc with SONAME 73. The Linux ICU archive matched
Qt's published SHA-256 and its three redistributed files matched the existing
pins byte for byte. Both Qt-listed source mirrors served matching bytes for
QtBase, QtSvg and QtImageFormats. Qt's source checksum URLs returned 404 at
verification time, so the source pins bind the SHA-256 computed from both
downloads. All 55 Qt source notices were readable and seven binaries matched
the macOS SDK's SPDX inventory.

The app and two Markdown targets built with `cmake --build --preset dev`.
The app's load commands and a separate contained loader trace identified
Qt 6.12.1. The two application startup paths, three Markdown scale courses
and image decoder passed in CTest. The architecture, packaging runtime,
release source assets and release security audit contracts also passed. The
package runtime tests retained two native-only skips for Linux CPack and
Windows WiX. Native Windows and Linux builds and release qualification are
outside this patch's evidence.

The four Qt cases from `dependency_modernization_contract` passed under the
registered network boundary. They covered the default path with a stale
caller-supplied Qt config, caller-installed discovery, provider validation,
and the pinned Windows installer. The documentation gate and its 82 checks
passed.

---
schema: "implement-rfc.card.v1"
id: "BPR-1854"
title: "Publish 0.4.2 previews and full release"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"external_state":"/Users/jaeger/.codex/release-runs/v0.4.2-20261104","baseline":"9d279d1fc","authority":"Owner requested immediate Mac and Windows prerelease, then full release on 2026-11-04"}
---

# Publish 0.4.2 previews and full release

- Prepare one committed source, actual native observations, authoritative previous MSI floor and frozen global cohort.
- Publish signed/notarized Mac and signed Windows prerelease through the maintained publisher before comprehensive qualification; verify anonymous downloads and native trust/source bindings.
- Complete all-platform compilation barrier, comprehensive native Release and Debug functional qualification, retaining actual failures and the eight standing Debug timing omissions.
- Produce Ubuntu packages, audit corresponding source/SPDX/signatures/security, resolve actual blockers and publish the full stable release under the owner's current authorization. Verify downloaded bytes and native trust on each producer.

No prior release failure, security disposition or native acceptance transfers without its exact input and authority checks. Existing releases and assets remain immutable.

## Native registration correction

The seven new Windows Release courses pass directly. The avatar helper's
allocator probe also passes in the zero-capability AppContainer; remove its
exemption. Record the six other courses' exact observed refusals and the missing
draft-state pin, without guessing a filesystem or OS cause. See the
[native measurement record](../../verification/release-v042-windows-containment.md).
The registration correction needs a successor frozen source; comprehensive
qualification and publication remain in progress. Preserve the earlier source,
signed artifacts and actual interrupted-build receipts.

## Optimized Linux fixture initialization

Build 10's Ubuntu Release compilation stopped in `github_provider_tests.cpp`:
GCC 15 reported the activity-poll session's nested repository strings as
possibly uninitialized under `-O3 -Werror`. Construct the named session with
its defaults and assign its populated fields individually, following the test
guide's existing GCC initialization rule. Keep the repository context empty and
preserve the poll's authority and assertions. Native compilation and the
provider course must verify the correction before the successor freezes;
build 10's signed artifacts and failed compilation receipt remain immutable.

The corrected provider target compiles with native GCC 15, and both provider
courses pass on Ubuntu and macOS. Ubuntu's full Release graph also compiles;
Debug and distribution compilation remain in progress.

## Published Windows preview

The Windows build 9 prerelease is public at tag `v0.4.2-build.9`, release
434387749. Its seven anonymous downloads match the native producer bytes and
retain passing native publisher/timestamp checks. Latest remains 0.4.1.
Advance the authoritative Windows floor to that observed per-machine MSI's
`0.4.9` identity before allocating the successor. Mac publication still needs
the existing notarization credential recovery; no Mac asset is published yet.

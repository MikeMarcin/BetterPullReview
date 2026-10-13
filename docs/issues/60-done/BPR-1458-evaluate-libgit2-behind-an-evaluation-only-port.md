---
schema: "implement-rfc.card.v1"
id: "BPR-1458"
title: "Evaluate libgit2 for the Git metadata components behind an evaluation-only option on macOS"
status: "Done"
rfcs: ["RFC-BPR-0083"]
depends_on: ["BPR-1454"]
priority: "P2"
platforms: ["mac"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
acceptance: [{"criterion":"BPR-1458:AC1","outcome":"pass","revision":"f3a351729","evidence":{"kind":"observation","detail":"BPR_EVALUATE_LIBGIT2 defaults off: with it off the configure log, build.ninja and _bpr hold no libgit2 and app_bpr links no git2 (link command hash unchanged); with it on cmake/BprLibgit2Provider.cmake verifies libgit2 1.9.7 (SHA-256 1a4fbe75...75e7, 7,769,162 bytes, COPYING e0938121...fae71) through bpr_fetch_verified_archive and builds bpr_libgit2_candidate (no HTTPS/SSH/NTLM/GSSAPI/CLI), which answers C1-C6 for the harness through the standard-type candidate interface.","path":"docs/issues/60-done/BPR-1458-evaluate-libgit2-behind-an-evaluation-only-port.md"}},{"criterion":"BPR-1458:AC2","outcome":"pass","revision":"f3a351729","evidence":{"kind":"observation","detail":"git_metadata_harness with the option on (macOS, Git 2.50.1) runs L beside K with unclassified=0: refusals (shipped-build SHA-256, reftable, compatObjectFormat, split/sparse index) and silent divergences (v0 refStorage, a config Git refuses, symbolic depth past Git's, the literal-prefix ** rule, directory-only and quoted attribute patterns) are classified, and the known-exponential adversarial ignore case is recorded as L's H6 failure (4.8 s at 14 stars). The canary course records L failing containment for every component (outside include FIFO opened; C2 include, C5/C6 symlinked rule files followed), with no execution and no writes; the shim and source citations are in the ledger.","path":"docs/issues/60-done/BPR-1458-evaluate-libgit2-behind-an-evaluation-only-port.md"}},{"criterion":"BPR-1458:AC3","outcome":"pass","revision":"f3a351729","evidence":{"kind":"observation","detail":"L's macOS cost record in docs/verification/RFC-BPR-0083.md: SPDX expression (GPL-2.0-only WITH GCC-exception-2.0 AND LGPL-2.1-or-later AND ...), trial packaging through release.verify (RELEASE_SOURCE_ASSET_OK with a negative control), measured build time (Release 28.5 s -j1 / 3.8 s -j10) and stripped size delta (+1,249,264 bytes Release), toolchain needs, release and advisory cadence from the archive, SHA-256 (experimental only) and reftable (none) status, and integration size (102+38+298 lines). Windows and Linux deltas are owed by BPR-1462/BPR-1463.","path":"docs/issues/60-done/BPR-1458-evaluate-libgit2-behind-an-evaluation-only-port.md"}}]
---

# BPR-1458: Evaluate libgit2 for the Git metadata components behind an evaluation-only option on macOS

Candidate L of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md). libgit2 is acquired only through
the verified-archive pattern (SHA-256, size, licence digest, re-extraction under
`build/_bpr`) behind an option that is off by default, so the shipping
configuration acquires nothing new. It is built without HTTPS, SSH, NTLM or its
CLI. No libgit2 type crosses its port.

Its provider and adapter need only [[BPR-1454]]; AC2 runs through the harness of [[BPR-1455]] and the canary course of [[BPR-1456]] once they land.

## Acceptance criteria

- **BPR-1458:AC1:** With the option off, no target acquires or links libgit2; with it on, the pinned archive is verified and an evaluation adapter answers the C1–C6 questions for the harness.
- **BPR-1458:AC2:** The harness classifies every L divergence under H1 per component, and the canary course records containment, no-execution and no-write results for every L component evaluated, on macOS.
- **BPR-1458:AC3:** L's cost record lists its license and SPDX expression, a trial `dependencies.json` entry run through `verify_release_sources.py`, the measured macOS stripped binary-size and build-time deltas, its SHA-256 and reftable status, toolchain needs, release and advisory cadence, and integration size.

## Continuation

Done on `f3a351729` (macOS); evidence in the
[verification ledger](../../verification/RFC-BPR-0083.md#bpr-1458-candidate-l-libgit2). Windows and Linux measurements are [[BPR-1462]] and [[BPR-1463]]. Adopting L would also wait for Q3, but L passes no component, so Q3 is not asked.

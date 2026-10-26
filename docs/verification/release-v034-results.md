# v0.3.4 release results

Published [stable v0.3.4](https://github.com/MikeMarcin/BetterPullReview/releases/tag/v0.3.4)
on 2026-10-26 from `be4fe79d8abbc8f438b25bbb76bbd16ab6d0a795`.
The annotated tag peels to that commit. The public release is Latest, neither
a draft nor a prerelease. GitHub Actions remains disabled.

All 28 public assets were downloaded without authentication and matched the
final producer bytes. Fresh native audits of those downloads passed on macOS,
Windows 11 x64 and Ubuntu 26.04 x86_64 under WSL2/WSLg. The
[asset inventory](release-v034-assets.json) records every name, size and SHA-256.
It contains eight macOS, seven Windows and thirteen Ubuntu assets, including
separate exact-version corresponding source for each platform.

## Source and qualification

The requested chat's changes are included: `69d7447c4` supplies wrapper/setup,
reset and diagnostics fixes; `a8a5d1855` supplies direct requested-review search,
batched membership queries and diagnostics. Both are ancestors of the release.
The release also includes the pinned mimalloc 3.5.3 allocator.

Full native suites retain their original source identity,
`62bfc09f6210233d31de5edb3379aa3381a7315f`. Exactly four tracked paths changed
between that source and the final release: the two agent-skill MIT license
files, the thin-package license lint, and the profile-tour hook expectations.
Application, core, host, adapter, CMake, dependency and toolchain sources are
identical. The three corrected owning CTests passed on final source. All three
final-source graphs—Release tests, Debug tests and tests-disabled distribution—
configured and built on every selected native platform before PC production.
The complete suites were not repeated for this licensing/test-only bridge.

| Native configuration | Actual retained result and reconciliation |
|---|---|
| macOS Release | 863 unique rows: 861 PASS, two updater disk-space guard failures. After recovering generated-build disk space, both exact same-source updater checks passed. Original failures remain recorded. |
| macOS Debug | 883 unique rows: 874 PASS, eight FAIL, one configured cache-byte SKIP. Three stale license/hook expectations were repaired and their final owning checks passed. Five pure timing assertion failures remain nongating under the standing owner policy. |
| Windows Release | 832 PASS; complete selected inventory. |
| Windows Debug | 832 unique rows: 825 PASS, six FAIL, one configured cache-byte SKIP. Four actual timeouts retain the carried v0.3.3 exception. The Insights fixture's exact-one-slow-turn expectation is a nongating timing assertion. Threaded reply passed a fresh semantic completion course for GitHub.com and GHES with only its bounded watchdog increased from 40 to 240 seconds; the original failure remains. |
| Ubuntu Release | 840 PASS; complete inventory, no failure or skip. |
| Ubuntu Debug | 831 PASS and one configured cache-byte SKIP; all 832 selected names accounted. Eight explicitly named dedicated performance courses were omitted under the standing policy, from 840 configured names. They receive no Debug passing or functional coverage credit. |

The five macOS timing failures were history, launch, responsiveness, active
work and pathological workloads. Exact measurements and both built-in attempts
remain in the raw reports and waiver bindings. Unreached checks receive no
credit. There is no blanket all-tests-PASS claim.

Two wrapper errors remain distinct from product results: the old Windows suite
wrapper misclassified CTest's failure exit 8 after a complete Debug suite, and
one final Windows build command named a nonexistent directory before any compiler
ran. Reviewed qualification records preserve the former; the latter was recovered
using the actual Release/Debug build presets, preserving its passing configure
and failed invocation.

The owner explicitly skipped native Intel Mac qualification. The signed
universal package still contains arm64 and x86_64. Universal compilation is not
native Intel execution evidence.

## Artifact and security scope

Native producer and downloaded-byte audits passed package/runtime equivalence,
SPDX, installed notices, separate corresponding source and provenance binding.
macOS passed Developer ID, hardened runtime, notarization, stapling and
Gatekeeper checks. Windows passed native Authenticode publisher and timestamp
checks plus MSI administrative extraction versus runtime ZIP equivalence.
Ubuntu passed DEB/user-archive file, mode and link equivalence, six SHA-256
OpenPGP signatures from the approved full primary fingerprint, a public-only
verification course and the expected tamper rejection. Extraction is not native
ordinary-user installation qualification.

Fresh unfiltered Grype 0.120.1 scans used the valid database built
`2026-10-26T06:38:44Z`. Scanner and binding gates remain FAIL/exit 2:
macOS 21 matches/21 advisories, Windows 19 matches/17 advisories, Ubuntu
21 matches/21 advisories. Exact finding row sets and every affected dependency's
SPDX package object match the prior published 0.3.3 evidence. Fresh source and
native import observations support only the existing narrow factual premises;
they do not establish whole-runtime immunity. Original dispositions retain their
scope and 2026-11-19 or earlier relevant-change expiry. The
[Qt known-issue decision](../security/release-policy.md#owner-decision-on-known-upstream-qt-issues-2026-10-16)
retains its official-upstream-fix end condition. No new independent or accountable
security/legal approval is claimed.

The newly selected allocator has incomplete advisory mapping and fuzz coverage.
Its absent scanner matches and bounded upstream published-advisory lookup are
not complete security qualification. Fresh exact-source secret scanning retained
65 reviewed synthetic occurrences, zero delta and zero unclassified findings.
Two bounded ASan/UBSan/libFuzzer courses each completed 100,000 iterations;
these cover selected production wrappers and pinned dependencies, not the whole
application or allocator.

RFC-BPR-0038 remains open. M8/M9 installation, authentication, vault,
accessibility, upgrade/removal, resident-material crash privacy, external
hardening and complete vulnerability/fuzz gaps remain explicit. Profile purge
excludes shared updater/registry state and is not uninstall.

## Standing Debug policy and evidence

The owner permanently made Debug performance timings nongating on 2026-10-26.
The [release guide](../development/release-test-profiles.md#standing-debug-performance-policy),
repository agent instructions and
[decision record](../issues/_checkpoints/RFC-BPR-0038/owner-debug-performance-policy-20261026.json)
carry that policy without a release expiry or repeated approval requirement.
Release timings, Debug functional checks, bounded hang detection, resource
acceptance, compilation, artifact/source/legal, security and native trust remain
separate. No historical failure is rewritten and no Debug performance repair
is claimed.

Raw local evidence is retained under
`/Users/jaeger/Documents/GitHub/BetterPullReview/.mask/sessions/release-v0.3.4/`.
The final source bridge, compilation barrier, per-platform qualification
acceptances, exact asset plan, fresh scan/applicability records, publisher logs
and `public-verification-complete.json` bind the actual receipts by SHA-256.
The Windows and Ubuntu producer roots remain available on JaegAW with original
build maps, byte-identical producer images, native reports and downloaded assets.
No signing credentials or private keys are public assets or tracked evidence.

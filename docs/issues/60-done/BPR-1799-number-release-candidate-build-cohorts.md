---
schema: "implement-rfc.card.v1"
id: "BPR-1799"
title: "Number release candidate build cohorts"
status: "Done"
rfcs: ["RFC-BPR-0038", "RFC-BPR-0035"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "owner-release-build-number-20261028", "milestone_plan": {"id": "release-candidate-build-numbering", "path": "docs/rfcs/plans/release-candidate-build-numbering/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0941326aee0f51002e205784a1b220a01ecba384","evidence":{"kind":"observation","detail":"Actual serializedallocation1/freeze094 consumesone durable productnumber acrossallthree producerobservations; unchanged retry/recovery/misuse14cases andactualnativecohort accepted. LocalbuildsremainN0; source/configchangecannotreuseN1.","refs":["numbering-allocation-tests-r3.log","numbered-controller-r1/cohort-r1.json","mac-numbered-native-r2/receipt.json"]}},{"criterion":"AC2","outcome":"pass","revision":"0941326aee0f51002e205784a1b220a01ecba384","evidence":{"kind":"observation","detail":"Actualproduction About/copy/diagnostic andnativeCFBundle0.4.0/1, PE0.4.0.1, MSI0.4.1/newProductCode/retainedupgradeidentity, DEB0.4.0+build.1 bindsource094/cohort4bfd. Nativeproduction/signature/source audits pass; nativeuserM8acceptance staysseparate.","refs":["mac-native-about-diagnostics-r2/receipt.json","mac-numbered-package-r1/receipt.json","windows-numbered-final-r1/receipt.json","linux-numbered-final-r1/audit/receipt.json"]}},{"criterion":"AC3","outcome":"pass","revision":"0941326aee0f51002e205784a1b220a01ecba384","evidence":{"kind":"observation","detail":"All30finalassets/source/SPDX/provenance/manifests/nativeidentities bindfrozen094/cohort1; byteidenticallegacyZIP retained. Originalv04015immutable, RC17downloaded/nativeverified; Linux13preparedunpublished. FinalnativePID/source/oldN12459+assetchecks pass.","refs":["final-local-30-assets-unchanged-r1.json","final-native-numbered-reconciliation-r1.log","stable-copy-tool-source-bridge-r1.json"]}},{"criterion":"AC4","outcome":"pass","revision":"0941326aee0f51002e205784a1b220a01ecba384","evidence":{"kind":"observation","detail":"PrescribedWindows/Linux19Release+19Debug repair closure, nativeSource/isolationbothconfigs andindependenttests-offcompositions pass viaexplicitTEST-ONLYoraclebridges26ae/ca1, unchangedproduct/configbytes. Originalsixfullreports/91failedrows, Debug8omissions/skips andmatchingwaivers retained. Old034 source-derivedparser/staging/nativeAPIreceiptcompatibilityproofpasses withGUIlaunchsuppressed. Tools58PASS1nativeWindows skip andactualappend/copydryrunsPASS. Stableauthorization/publication/BPR1797/RFC0038 remainopen; noauthinferred.","refs":["windows-repaired-public-r4-receipt.json","linux-repaired-public-r5-complete/receipt.json","linux-discovery-fixed-r1-native/receipt.json","linux-discovery-wait-diagnostic-r1/predicate-correction-control.json","full-N-to-repair-inventory-r1.json","stable-updater-proof-r1/receipt.json","stable-copy-native-release-tests-r4.log","stable-append-dryrun-r1.log","stable-copy-dryrun-r1.log"]}}]
---

# Number release candidate build cohorts

The owner requested a sequential, product-specific build number for the
publish-release workflow. Ordinary local builds do not allocate numbers.
One candidate cohort carries the same number on every platform. An unchanged
retry retains its allocation; changed source or declared candidate inputs
requires another identity. This is BPR-only work.

## Acceptance

- AC1: A durable, serialized allocation records source and complete declared
  cross-platform configuration inputs, survives interruption, prevents reuse
  across different inputs, and reuses an unchanged candidate on retry. The
  source binding avoids embedding a commit's own hash in that commit.
- AC2: Numbered native builds expose the marketing version and cohort number
  in About, copied version information and diagnostics. macOS bundle, Windows
  image/MSI and Linux package identities use valid, checked native mappings.
  Local builds remain unnumbered and consume nothing.
- AC3: Binary, corresponding source, SPDX, provenance and release manifests
  bind one exact source/configuration/cohort. Canonical download names include
  the build number. The guarded publisher rejects mixed cohorts and occupied
  releases; the legacy updater ZIP alias, when emitted, is byte-identical.
- AC4: The existing stable updater and persisted settings remain compatible
  with base-version tags. Focused public-path, allocation/recovery, package,
  publisher and native identity proofs cover positive and misuse cases.
  Documentation and actual remaining native/security gates stay explicit.

## Design and publication boundary

See [the design](../../development/release-build-identity.md). The already
published `v0.4.0` tag and its N144de source/assets remain immutable. The corrected
source may retain marketing version 0.4.0 for multiple distinct numbered
prerelease tags, per the owner’s 2026-10-29 UTC clarification. A future stable
base-version publication/promotion policy remains separate. AgentSkills source
publication and the project PR-tour pin used their separate direct approval. BPR-1797 qualification continues with its original
reports, waivers and native command ownership.

## Implementation checkpoint, 2026-10-29 UTC

The allocation/freeze slice is committed at `2ef1011dd`. The integrated product
slice now projects the cohort through CMake, About and Copy version info, local
diagnostics, native mappings, package/source/SPDX provenance, canonical assets
and the guarded publisher. It retains the exact legacy updater ZIP alias and
base-version ordering. The required frontend-design skill was loaded; About
keeps its existing native palette, fonts and layout.

Focused evidence so far: ten allocation/process/recovery/public-command cases;
three actual About composition/1×/2× courses; diagnostics host and native macOS
ZIP staging courses; 25 source/publication controls; 45 native release/tool
cases (one native Windows skip); 32 runtime cases (one native Linux skip); and
14 package-audit controls pass. Documentation structure and its 67 cases pass.
These observations do not yet supply a real numbered producer composition,
actual Windows MSI/PE and Linux DEB observations, or final native acceptance.
The initial incorrect build target, old fixture refusals and corrected checks
remain in the external request evidence rather than becoming passing claims.

The initial one-criterion 0.4.1 proposal is now obsolete and withdrawn, as
recorded below. No native requirement forces that patch bump. Separate functional follow-ups remain open: the five
Windows Debug agent observations reproduced their precondition failures, and
the bounded threaded-reply recovery still did not complete. Ubuntu's original
source qualification is running. This card and BPR-1797 remain In Progress.

## Owner clarification, 2026-10-29 UTC

Several RCs retain marketing version 0.4.0 and publish under distinct
`v0.4.0-build.N` prerelease tags. Native mappings do not force a patch bump.
The unanswered 0.4.1 proposal was withdrawn after checking actual intake; its
session is waiting/unprotected at consumed cursor 5 and its scoped transport
was stopped. No human verdict was invented. Installed stable updaters ignore
these prereleases, while the source URL and manifests bind the actual RC tag.
Build-tag stable promotion is refused; stable policy remains a future decision.
The current implementation is being amended to this clarified mandate.

The clarified RC guards now derive the actual numbered prerelease tag and source
URL, refuse stable promotion, and isolate numbered build/artifact directories.
The committed authoritative Windows floor binds the verified published MSI digest;
allocation rejects unknown, caller-selected older and rolled-back baselines.
Thirteen allocation cases pass and include global numbering across a marketing patch and
legacy 0.4.7 refusing fresh N=1 before any reservation. The native/tool course records
49 passes and one native Windows skip, and 27 source/publication controls pass,
including unchanged legacy/local directories and exact RC tag/source offers.
The original 15 public asset digests and eight original Mac producer file digests
were rechecked unchanged. Actual numbered native positives remain pending.

A real CMake module composition now configures/builds an ordinary tiny native
probe as build 0 without allocation, then consumes a frozen fixture cohort as
build 1, reuses that allocation unchanged and refuses changed source before
compiling. It caught duplicate observed option names; the module now deduplicates
them before emitting JSON. All 14 allocation/module cases pass. This tiny probe
is module evidence, not the pending full numbered application/package/native
acceptance. The publisher retry control also refuses an already-created RC
without another remote mutation; all 27 source/publication cases pass.

## Actual build 1 and completed engineering, 2026-10-29 UTC

The frozen producer is `0941326aee0f51002e205784a1b220a01ecba384`,
cohort `4bfd1b5ab2b0c07fbad0b74692ba085f2f67931c68babc1a3875571f57841249`.
All three producers consume allocation 1. Actual native observations verify
macOS short version 0.4.0/build 1, Windows PE 0.4.0.1 and MSI 0.4.1 with its
new ProductCode and retained upgrade identity, and Debian 0.4.0+build.1.
The production About/copy/diagnostic observation and source/SPDX/native audits
bind these identities to the frozen artifacts. The full set has 30 assets:
Mac 10 and Windows 7 are on the distinct `v0.4.0-build.1` prerelease, with
anonymous downloads and native trust verified; Linux 13 are signed and audited
but remain unpublished. The original `v0.4.0` source and 15 assets are unchanged.

The remaining repaired public paths now have Windows Release 19 and Debug 19,
and Linux Release 19 and Debug 19, through explicitly recorded supplemental
proofs. Windows Release's three platform-warning density failures are retained;
verification-only commit `26ae7e213` excludes the exact warning widget while
preserving its complete text, the offer assertions and duplicate/missing-label
controls. The 30 Release product images are unchanged. Linux Debug's discovery
settlement failure is also retained. Actual snapshots admitted scheduled or
in-flight preparation as settled; test-only commit `ca1cc054f` adds those
existing preparation guards. The original 20-second helper/deadline is unchanged.
Its affected Release and Debug cases pass against unchanged product images,
configuration cache bytes and all other source inputs. Exact failed interleaving
was not reproduced by the instrumented observation.

Current native source/isolation controls pass in both configurations and each
platform's prescribed independent tests-off composition passes. Earlier Mac
22/22 Release and Debug repair evidence, BPR-1800's native closure and all five
bounded Windows agent semantic recoveries retain their exact source/input
bindings. All six original comprehensive reports remain failed; their 91 failed
rows have separate repair or carried-waiver dispositions. The eight dedicated
Debug timing omissions and configured skips gain no passing coverage.

The source-derived 0.3.4 updater proof accepts the byte-identical base ZIP under
bare tag `0.4.0`, rejects the metadata-tag alternative and verifies native
transaction/startup-receipt APIs on the signed candidate. Its GUI launch effect
was suppressed; it supplies no full installed-user or M8 acceptance. A guarded
publisher-only extension is prepared at `6630ee394` (integrated as `63fd5ed18`):
59 tool cases, 58 passing and one native Windows-only skip; actual append and
stable-copy dry runs pass with no external mutation. The dry runs explicitly
retain 13 missing source-RC assets and absent publication authority.

BPR-1799's numbered-cohort scope is complete. BPR-1797 still owns publication:
first append and anonymously/native verify Linux's 13 RC assets, then copy the
same complete 30 files to a new bare `0.4.0` stable release after specific owner
authorization. No source offer, existing tag or asset is rewritten. HarfBuzz's
direct deferral covers this exact build 1 and all three platforms; raw scanner
exits 2 (23 Mac, 21 Windows, 23 Linux), matching historical waivers and M8/M9,
Intel, vault/authentication, accessibility, crash/privacy, allocator/fuzz and
independent-review limits remain explicit. No whole-release or RFC closeout is
claimed by this card.

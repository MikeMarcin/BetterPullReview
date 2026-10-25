---
schema: "implement-rfc.card.v1"
id: "BPR-1759"
title: "Verify process-wide mimalloc on native macOS"
status: "In Progress"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758"]
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC03 universal signed local trust and installed closure","outcome":"pass","revision":"0f42035a1758a33c729317d66d3aaaac0e771fb1","evidence":{"kind":"observation","detail":"All 19 images universal Developer-ID/hardened/timestamped with no entitlements; app and DMG notarized/stapled/Gatekeeper accepted; matching private relocated install; package/source/checksums/negative controls/MCP/headless render pass. Exact ledger signed local macOS r9."}},{"criterion":"AC03 delivered native Cocoa behavior","outcome":"pending","revision":"0f42035a1758a33c729317d66d3aaaac0e771fb1","evidence":{"kind":"artifact","detail":"Fresh session 3a0425eb3756874560105a4cff2b0b64; candidate BPR-1759-signed-cocoa-r9 fingerprint a68fe8b3cd8c62f9287c0a8db30529a765947f8bf757641bf7fbab2fe93d5bd5; external state /Users/jaeger/.codex/attended-review/bpr-mimalloc-signed-r9-k9f02pev; initial consumed cursor 3. Owner judges delivered Cocoa through local-file review/reopen/Quit; no human verdict supplied here."}},{"criterion":"AC03/AC06 unchanged implementation native pointer and failure evidence","outcome":"pass","revision":"df6e8be48ff17271e2258f0040e4ca87795f44bd","evidence":{"kind":"observation","detail":"All production/provider/package/version paths match this prior implementation. Its prior r4 native pointer/cross-module/OOM and CLI records retain exact original producer identities. Signed initialization diagnostics are not new pointer proof; no release qualification inferred."}}]
---

# Verify process-wide mimalloc on native macOS

## Scope and acceptance

Implement and observe the macOS override in the selected desktop, CLI,
update helper and other helpers (AC03). Probe actual C, C++, Qt/loaded-library allocation and
cross-module release on native Apple Silicon. Verify universal arm64/x86_64
slices and static override evidence; native Intel execution is not required or
claimed under owner amendment r5. Check the signed installed candidate, startup
and shutdown. Include allocator-boundary stress needed by AC06; a linked binary
or stats report alone is insufficient.

## Current signed result — authorized round r9

The local Developer-ID signed/notarized universal candidate is now produced and
verified at `0f42035a1758a33c729317d66d3aaaac0e771fb1`. The
[exact ledger](../../verification/RFC-BPR-0093.md#signed-local-macos-acceptance--authorized-round-r9)
binds all 19 images, both Apple accepts, strict signatures/hardened runtime,
staples, Gatekeeper, matching ZIP/DMG/private installation, full package/source
audit, allocator negative controls and the actual packaged MCP course. The
headless Home render passes without changing delivery. Broad release suites
were skipped for this local validation only; no version/push/publication change.

Required delivered native Cocoa behavior remains **human-pending**. Fresh session
`3a0425eb3756874560105a4cff2b0b64`, external state
`/Users/jaeger/.codex/attended-review/bpr-mimalloc-signed-r9-k9f02pev`, candidate
`BPR-1759-signed-cocoa-r9`, initial consumed cursor 3, supplies the frozen signed
app and one Accept/Reject question. Its launcher checks payload/signature and
isolates actual macOS Qt profile paths with `HOME` plus `CFFIXED_USER_HOME`;
no agent opens or captures a Cocoa window. Previous pointer evidence retains
its exact unchanged-implementation producer; signed runtime diagnostics are not
relabeled as a new pointer course. Intel execution stays waived under r5.
Earlier signed/trust-pending statements below are historical and superseded
only by this exact local result. The coordinator consumes review in the next
engineering round; this card is not Done.

## Historical acceptance — mandate r5, 2026-10-24

The owner explicitly answered “Amend Intel criterion”. The verified universal
app/CLI and native arm64 observations satisfy the amended architecture coverage;
no native Intel run is required or reported. The current app producer remains
`75baca275aa8f052997a231a83ebc7d77d27ab6d`, the current CLI/product source
`df6e8be48ff17271e2258f0040e4ca87795f44bd`. Both slices' dependencies and allocator
interpose sections were checked read-only again in r5; the
[ledger](../../verification/RFC-BPR-0093.md#intel-acceptance-amendment--mandate-r5)
binds exact identities and the owner decision.

This card remains **Blocked** on the actual Developer-ID signed-installed course
and delivered native Cocoa observations. The offline ad-hoc seal and pre-main
arm64 observer do not complete those requirements. No new product candidate,
signing, release, Keychain or owner-desktop operation belongs to this round.

The r1–r4 checkpoints below are historical. Their Intel-pending statements are
superseded by r5; their candidate identities and observations are preserved.

## Candidate prerequisite amendment, 2026-10-24

The coordinator authorized moving only the package prerequisites needed for
AC03 into this milestone. This card therefore owns the macOS runtime
producer/deployment declaration, retained MIT notice and source-inventory entry
needed to prepare a tests-disabled installed candidate through `tools/release.sh`.
The source basis is `packaging/Install.cmake`'s fixed private-library producer
map, `packaging/package_runtime.py`'s closed runtime/notice inventory and the
release script's dependency, signing and relocated-launch checks. Without those
entries the installed allocator cannot load or pass the package audit.
Final AC07 for every platform, standalone CLI/source closure, negative package
qualification and release acceptance remain on [[BPR-1762]]. No criterion or
milestone exit is waived; signed installed and native Intel evidence stay
required. `--prepare-only` is an ad-hoc candidate, never signed-install acceptance.

## Native evidence and current block

Product source candidate `aadf2d91d` on macOS 26.6.2 arm64, AppleClang 21 and
Qt 6.12.0: all five actual tests-enabled executables pass C/C++, loaded-module,
Qt, cross-module, cross-thread, allocation-failure and one-runtime ownership
checks. The provider/missing-override controls also pass (7/7, 15.90 seconds).
The prior 17-row integration passes; unchanged product behavior reuses that
record. Package/runtime/source tests pass (30 with one native-Linux skip, 9,
and 24 respectively). See [the ledger](../../verification/RFC-BPR-0093.md).

`tools/release.sh --version 0.3.2 --prepare-only --skip-tests --output-dir
build/mimalloc-m1/local-candidate` refused at the clean-source guard before a
build, deployment, signing, Keychain lookup or Apple submission. Unrelated
`docs/issues/NEXT_ID` and reservation
`docs/issues/_reservations/BPR-1765--BPR-1765--65cc7e762a78.json` appeared
concurrently; neither belongs to this milestone and both are preserved.
No installed candidate was produced. After that writer reconciles its files,
retry the same local preparation from clean committed source. The resulting
ad-hoc candidate still cannot satisfy signed-installed AC03. Developer ID and
native Intel evidence require their actual authorized course; this arm64 host
supplies neither. Do not mark this card or the milestone complete.

## Clean-worktree result

Source `eb28c0187a49143f5470e4400c2f1b3d0cb03de0` in branch `codex/mimalloc-macos-candidate` at
`/Users/jaeger/.codex/worktrees/mimalloc-macos-candidate/BetterPullReview` produced the credential-free local candidate successfully.
The dirty-source blocker is cleared in this worktree; main's allocation remains
untouched. Full package audit passes (52 files, 19 binaries, 519 first-party
units) using scoped LLVM 22; source-sidecar binding and public missing/extra
allocator controls pass. The relocated offscreen assessment passed and left the
delivered bundle unchanged. See the ledger's clean-worktree continuation for
artifact names, hashes, exact commands, initial tool/harness failures and limits.

This is a tests-disabled, universal, ad-hoc-sealed local bundle. It does not
supply Developer ID, notarization, Gatekeeper, delivered native Cocoa or Intel
execution evidence. This card remains Blocked on those actual native/signed
observations; no criterion is waived. The branch/worktree and candidate are
retained, all own commands are quiescent, and no review server is running.

## Current r4 result

The fresh universal app candidate is `75baca275aa8f052997a231a83ebc7d77d27ab6d` (current product/package code
`df6e8be48ff17271e2258f0040e4ca87795f44bd`), prepared cleanly in `/Users/jaeger/.codex/worktrees/mimalloc-macos-r4/BetterPullReview` on
`codex/mimalloc-macos-r4`. Full package/source audits, relocated offscreen public
assessment and missing/extra allocator controls pass. An external non-shipped
pre-main observer now passes C/C++/packaged Qt ownership, one allocator,
cross-module/thread frees and allocation failures in every exact staged app
image. It also passes on the retained current CLI and supervisor, with each
private origin asserted and all payload hashes unchanged. The packaged MCP
public stdio/diagnostic course and safe startup refusals pass. See the ledger's
current r4 section for exact artifact hashes, scope and reproducible evidence.

No product source changed; earlier candidates and main's unrelated dirty files
are preserved. The worktree/build/candidate remain retained, all own commands
are quiescent, and no review session/server exists. This card stays Blocked:
Intel remains a real criterion despite r4 permitting work without that machine;
Developer ID/signed-installed and delivered native Cocoa acceptance remain
pending. No Keychain, release-signing, publication or desktop operation occurred.

## Authorized signed continuation — r9

The owner now authorizes local Developer-ID signing and notarization of the
current committed universal candidate, without a version change, push, tag or
publication. The clean task branch is `codex/mimalloc-macos-signed-r9`.
The preflight caught a stale production-manifest pin. Source review of
`6c998babb..d2b7f6a32` finds only `df6e8be48` changed that manifest's files:
`installed_runtime.cpp/.hpp` add standalone CLI private plugin directories
and clarify the comment. The canonical-directory check, ambient-selector
clearing and forbidden code-loading arguments remain enforced; the desktop
path, full application digest and mutation-host digest are unchanged. Other
app-scope commits in the range are `df7078277`, `06697d93d`, `f6adc378a` and
`1e269fdd3`; their CMake edits lie outside the manifest. The source review
reseals this one stale pin and retains the initial failure. Its hostile
controls must pass before the signed build. Native Cocoa judgment remains
pending; no agent drives or captures the owner's desktop.

---
schema: "implement-rfc.card.v1"
id: "BPR-1797"
title: "Prepare, qualify and publish v0.4.0"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate": "release-v040-owner-request-r1-20261028", "baseline": "7dc90f2d8e7a6ef9c852a1db93f6c20e2c1570ae", "execution_strategy": "coordinator-implementer", "external_state": "/Users/jaeger/.codex/release-runs/v0.4.0-20261028", "milestone_plan": {"id": "release-v040", "path": "docs/rfcs/plans/release-v040/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0941326aee0f51002e205784a1b220a01ecba384","evidence":{"kind":"observation","detail":"Clean common source 0941326a and build1 cohort 4bfd1b5a; grounded notes and all producers bound. Existing history preserved.","externalRoot":"/Users/jaeger/.codex/release-runs/v0.4.0-20261028"}},{"criterion":"AC2","outcome":"pass","revision":"0941326aee0f51002e205784a1b220a01ecba384","evidence":{"kind":"observation","detail":"Actual native compilation and six comprehensive reports reconciled by full-N-to-repair-inventory-r1.json; all original 91 failed rows retain exact repairs or owner-carried waivers, Debug omissions/skips/unreached preserved. No complete-suite PASS inferred.","externalRoot":"/Users/jaeger/.codex/release-runs/v0.4.0-20261028"}},{"criterion":"AC3","outcome":"pass","revision":"0941326aee0f51002e205784a1b220a01ecba384","evidence":{"kind":"observation","detail":"All three final artifact/source/SPDX/provenance and native signature/source audits pass; fresh raw Grype 23/21/23 exit2 retained with exact owner HarfBuzz/CBOR/matching waivers and native/M8M9 limits.","externalRoot":"/Users/jaeger/.codex/release-runs/v0.4.0-20261028"}},{"criterion":"AC4","outcome":"pass","revision":"0941326aee0f51002e205784a1b220a01ecba384","evidence":{"kind":"observation","detail":"Actual human authorized publication complete. publication-final-verification-r1.json confirms all 30 stable anonymous hashes + Mac native trust, Windows39 and Ubuntu77 native commands; immutable older releases and stableLatest verified. Documentation and ownership updated.","externalRoot":"/Users/jaeger/.codex/release-runs/v0.4.0-20261028"}}]
---

# Prepare, qualify and publish v0.4.0

The owner requested `$publish-release v0.4.0` with an implement-rfc coordinator and one implementer for needed fixes. The owner also amended acceptance: “Anything that comes up for this release that was previously waived is waived again, carry it forward.” The request authorizes the existing full macOS universal, Windows x64 and Ubuntu 26.04 WSL x64 release route, signing and publication.

## Acceptance

- AC1: Prepare version 0.4.0 and grounded user-facing notes; freeze one clean common committed candidate while preserving unrelated work.
- AC2: Reconcile existing native jobs, pass the all-platform Release/Debug/distribution compilation barrier, and run comprehensive native Release plus Debug functional selection. Preserve actual failures/skips, fix new blockers in explicit slices, and record carried matching historical waivers separately.
- AC3: Produce and audit the complete signed binaries, exact corresponding source, SPDX, checksums and provenance through the existing native tools. Run fresh source/security and native trust checks; retain known M8/M9, Intel, allocator/advisory/fuzz and other matching historical limitations under the owner amendment.
- AC4: Preflight and publish the qualified authorized candidate through the existing publisher. Verify remote state, all uploaded hashes and native trust after download; complete tracker/docs and durable command/evidence upkeep.

## Initial reconciliation

Entry source is clean `7dc90f2d8`; MASK registration changes no tracked declarations. Broad changes since v0.3.4 require comprehensive qualification. The standing Debug policy omits exactly the eight named dedicated performance courses and retains Debug functional checks. Old observations and failures keep their original source identity. Mac capacity is initially 5 GiB, below the roughly 11 GiB graph footprint before temporary/nested/package work. Native producers and signer state are under inspection. No release/tag mutation has occurred.

## Prepublication UTF-8 correction

The Windows focused course passed five checks and failed both tour publication
progress-stage checks. A native MSVC/Qt probe confirmed that an ordinary narrow
smart-quote literal becomes code-page byte 93, which Qt decodes as replacement
efbfbd; explicit UTF-8 bytes decode correctly as e2809c. The application
stage formatter and its owning expectations now name explicit UTF-8 bytes.
No compiler flags change. Focused native verification and fresh package/source
binding remain required before publishing the corrected candidate. The signed
previous candidate is retained externally and has never been published.

The owner subsequently requested Mac-first prerelease, then Windows assets on
the same prerelease, with comprehensive qualification afterward. Stable
promotion and Ubuntu artifacts retain their complete gates. The exact external
amendment and new HarfBuzz decision packet are in the recorded runtime root;
no passing qualification or security decision is inferred.

## Post-preview qualification checkpoint, 2026-10-29 UTC

The public v0.4.0 prerelease remains bound to immutable source
`144de719a8d972162f92ec04f98ba11293ae0a8f`: eight Mac and seven Windows assets,
all previously downloaded, hash matched and natively trust checked. No tag or
asset is replaced. Comprehensive qualification followed the owner's preview-first
amendment: Mac Release 865/871 pass, six fail; Debug 875/883 pass, seven fail,
one optimized-only skip. Windows Release 825/847 pass, 22 fail; Debug 805/839
pass, 33 fail, one optimized-only skip. The eight dedicated Debug performance
omissions have no pass or functional coverage credit. Raw failures, skips and
unreached checks remain in the original external request evidence.

The [paired Windows containment observations](../../verification/release-v040-windows-contained-observations.md)
supply actual Release/Debug baselines and contained refusals for ten exact
courses. Their narrow registration/pin correction changes no functional
assertion, deadline, capability or product permission. Its 73 contract cases
and Mac manifest pass; new-source native Windows verification remains pending.
The five newly failed Debug agent courses reproduce the same completion
precondition failures under exact original commands. The bounded threaded-reply
recovery also expires at 240 seconds while awaiting a resolution completion;
GitHub.com's final contract and the second host remain unaccepted. Neither is
waived as timing or converted to a pass. Ubuntu's original-source comprehensive
run is active, serialized after the Windows follow-ups.

BPR-1799 implements the separately requested numbered candidate identity. The
existing Find-ring owner fix and guarded publisher are deliberately integrated
with the qualification repairs on the engineering branch. The owner clarified that corrected numbered RCs retain marketing version 0.4.0
and use distinct prerelease tags. The unanswered patch-bump proposal was
withdrawn; future stable base-version publication is a separate decision.
The owner separately approved the prepared PR-tour source publication and
project pin update directly. AgentSkills main now contains exact safe candidate
`677128ae334a4cbb9d9fdee0028aec698f351d48` by normal fast-forward. This checkout’s
PR-tour portable pin/deploy is `ae21489d0`; all seven payload files match the
authoritative package, every other pin is unchanged, and the previously failed
`skill_pr_tour_package` plus all three bootstrap checks pass. The actual attended
UI had no verdict; its cursor 5 and direct approval are recorded separately,
protections released and only its own transport stopped. M8/M9/native Intel/vault/crash/privacy,
allocator/fuzz and final exact-artifact security dispositions remain open; the
preview-specific HarfBuzz and v0.4.0 CBOR decisions do not automatically accept
a future corrected version or stable/Ubuntu distribution.

## Numbered candidate qualification continuation, 2026-10-29 UTC

BPR-1799 completes the numbered-cohort engineering at frozen source
`0941326aee0f51002e205784a1b220a01ecba384`, build 1/cohort `4bfd1b5a…`.
Mac 10 and Windows 7 assets on `v0.4.0-build.1` are downloaded and natively
verified; Linux's 13 final signed/audited assets remain unpublished. The original
`v0.4.0` source `144de719…` and its 15 assets remain immutable.

The prescribed supplemental closure completes Windows and Linux 19 Release
and 19 Debug public paths, both configurations' source/isolation controls and
one independent tests-off composition per platform. Verification-only warning
and discovery-settlement corrections preserve the frozen product/configuration
bytes. Earlier Mac 22/22 in both configurations, BPR-1800 closure and five
bounded Windows agent recoveries are reused under their recorded bindings.
All six original comprehensive reports remain failed. The external
`full-N-to-repair-inventory-r1.json` maps every original failed row to its actual
supplemental proof or matching carried waiver and retains Debug omissions,
configured skips, original failures and unreached checks.

The owner directly deferred HarfBuzz for this exact build 1, including stable
and Ubuntu; the raw 23/21/23 scanner findings still exit 2. Matching historical
waivers and native/M8/M9, Intel, vault/authentication, accessibility, crash/privacy,
allocator/fuzz and independent-review limits remain explicit. No additional
security verdict or independent review was invented or requested.

At the preparation checkpoint, the authorization boundary permitted qualification and preparation only.
The reviewed operation is: append Linux's 13 files to the RC, anonymously
download/native verify them, then create a distinct bare `0.4.0` tag/release at
source `0941326a…` with the same complete 30 files and make it stable/Latest
only after its complete asset set verifies. Existing `v0.4.0` and the RC are
preserved. Installed source offers require the Linux source on the RC first.
Old 0.3.4 updater code accepts the exact alias and bare tag in native staging;
same-base 0.4.0 previews still require manual installation. The guarded tool's
actual dry runs list the 13 missing RC assets and absent publication authority.
Specific authorization and subsequent publication/download verification remain
pending. This card and RFC-BPR-0038 remain open.

## Authorized stable publication and remaining native audits, 2026-10-29 UTC

The owner accepted candidate `BPR-1797-publish-build1-stable-r1` in short session
`aa41ee505768731907ac3ff66223d84d`: human decision
`15ef6885-8661-425e-aadd-6dbc5d298c07`, consumed handback cursor 6. This
supersedes the earlier preparation-only hold. The guarded publisher appended
Ubuntu's thirteen files, which passed anonymous hashes and a 77-command native
audit, then created new annotated bare `0.4.0` at frozen source
`0941326aee0f51002e205784a1b220a01ecba384`. Release `428060158` is stable/Latest
with the complete same thirty files. An observed draft-tag API 404 was recovered
through a tested numeric-ID lookup and exact inspected draft resume; no tag,
release or file was replaced.

[Release results](../../verification/release-v040-results.md) and the
[asset inventory](../../verification/release-v040-assets.json) record all thirty
anonymous hash/size matches, passing Mac stable native trust/source checks and
unchanged original preview/RC identities. Stable Windows native verification subsequently passed 39 commands after all
seven anonymous downloads. The stable Ubuntu native audit remains pending, so
AC4 and this card remain open. Existing producer and
RC native evidence supplies no fresh stable native acceptance.

Under the owner's sequencing amendment, a fresh v0.4.1 owner starts from the
integrated latest main now and adopts the remaining Ubuntu native audit before new PC producer work. The external runtime root retains prepared guarded
commands, the safely refused starts, physical-host observations and the durable
handoff. No foreign process was stopped. Close this card only after actual
stable Ubuntu native receipt and final reconciliation pass. All
original failures, source bridges, matching owner-carried waivers and RFC-0038
limitations remain as recorded above.

## Final publication reconciliation, 2026-10-29 UTC

The owner accepted publication in short session `aa41ee505768731907ac3ff66223d84d`, decision `15ef6885-8661-425e-aadd-6dbc5d298c07`, consumed cursor 6. The approved Ubuntu append and distinct stable copy are complete: bare `0.4.0`, annotated tag `6a3f9b851fd246d1cbe828668258b123a7135091`, release `428060158`, source `0941326aee0f51002e205784a1b220a01ecba384`, build 1/cohort `4bfd1b5a…`, is stable Latest with exactly 30 files. Legacy `v0.4.0` and the RC's original 17 files remain immutable; the RC stays prerelease and has its approved additional 13 Ubuntu files.

All 30 stable files were anonymously downloaded and SHA-256 matched. Downloaded Mac trust/source commands, Windows 39 native commands, and the final Ubuntu 77 native commands pass. Ubuntu's 13-file evidence archive SHA-256 is `68e6ec78a03ce352a1a77794b5b980c13d5b217c8ba5d3e479141d30d5aee0c9`. `publication-final-verification-r1.json` under the external run root records the complete remote/native reconciliation. No new PC production began before that inherited audit finished. No foreign process was stopped.

The six original comprehensive reports remain failed, with every original failed row mapped to its recorded repair proof or matching carried owner waiver. Grype remains raw exit 2 with 23/21/23 findings. This release closes under its actual owner waivers, including HarfBuzz all-platform build 1 and CBOR, rather than a clean scanner or full M8/M9 claim. RFC-BPR-0038 and its separately owned native/security gaps remain open.

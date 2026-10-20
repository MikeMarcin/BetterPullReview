---
schema: "implement-rfc.card.v1"
id: "BPR-1663"
title: "Publish v0.3.1 and shorten the release procedure"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"mandate":"Owner requested publication of v0.3.1 and a faster procedure; confirmed full macOS, Windows and Ubuntu 26.04 packages","baseline":"5aff89be6","milestone_plan":{"id":"release-v031","path":"docs/rfcs/plans/release-v031/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"2ed7e5f98641d75678f861dad0edbe658b5f5d22","evidence":{"kind":"observation","detail":"Frozen clean source binds all three producers, accurate final published notes and public platform download/minimum links.","records":["docs/verification/release-v031-publication.json","docs/verification/release-v031-engineering.md","docs/verification/release-v031-procedure-findings.md"]}},{"criterion":"AC2","outcome":"pass","revision":"2ed7e5f98641d75678f861dad0edbe658b5f5d22","evidence":{"kind":"observation","detail":"All2392 normal native rows:2389PASS+3configuredDebugSKIP; optimizedMac/WindowsPASS; Ubuntu originalstrict8 and supplement1 remain FAIL under only explicitexisting216MiB sampled/lifetimeRSS exception. All28 signed audited runtime/source/SPDX assets, exact material accepts and bounded independent security dispositions supplied. BroaderRFC0038 qualifications remain open.","records":["docs/verification/release-v031-publication.json","docs/verification/release-v031-engineering.md","docs/verification/release-v031-procedure-findings.md"]}},{"criterion":"AC3","outcome":"pass","revision":"2ed7e5f98641d75678f861dad0edbe658b5f5d22","evidence":{"kind":"observation","detail":"Public stableLatest/nonDraft/nonPrerelease actualrelease418732973/tag728e6e75 peels2ed7; all28anonymousdownload hashes/source bindings; nativeMac,Windows5+11Qt/independent5/MSI29fileequality,Ubuntu6+tamperPASS. Source/tag/digests/snapshot retainedreadonlyindefinitely with ongoingaccess obligations.","records":["docs/verification/release-v031-publication.json","docs/verification/release-v031-engineering.md","docs/verification/release-v031-procedure-findings.md"]}},{"criterion":"AC4","outcome":"pass","revision":"2ed7e5f98641d75678f861dad0edbe658b5f5d22","evidence":{"kind":"observation","detail":"Measured phase times, retries/wrappers/collector failures, human waits/handoffs and boundedreuse preserved; safe laterprocedure proposals recorded, per ownerfinishcurrentworkflowunchanged steering. No newworkflow/claimed speedup.","records":["docs/verification/release-v031-publication.json","docs/verification/release-v031-engineering.md","docs/verification/release-v031-procedure-findings.md"]}}]
---

# BPR-1663: Publish v0.3.1 and shorten the release procedure

## Acceptance

- AC1: Prepare one clean committed 0.3.1 source with accurate public release notes.
- AC2: Run native normal gates, package and sign the three selected platforms, audit actual runtime/source/SPDX and final native trust, retaining failures and skips.
- AC3: Publish through the existing guarded public publisher only after selected artifacts and required dispositions are ready; verify downloaded bytes, source binding, native trust and release/tag state.
- AC4: Record measured time and safe procedure improvements, reusing unchanged evidence and producer caches without weakening gates.

The owner explicitly extended the existing Ubuntu RSS-growth exception to
v0.3.1 on 2026-10-19, requesting an implementer pursue its repair after publication.
Only the unchanged 216 MiB process RSS predicate is excepted; its result stays failed. RFC-BPR-0038's
remaining native/security/source obligations remain explicit.

## Release integration findings

The Qt6.12 candidate `cecded819` completed its normal macOS gate with 813 passes,
one configured Debug cache-course skip and one failure. The Discuss-to-Review
course exposed a real race: durable read completion replaced an already-open
reader, destroyed its reply menu and disturbed an immediate reply. The bounded
release repair updates read marks within the current card and preserves the
visible post's position when the New divider changes. It keeps the existing
semantic read command and host-owned persistence.

Exact final regressions fail with the four old production presentation files
and pass after byte-checked restoration. Immediate menu navigation, Reply text
and caret, and mixed-divider viewport continuity pass at 100%, 125% and 200%;
the focused composed integration and strict touched-unit clang-tidy 22 pass.
See [release engineering evidence](../../verification/release-v031-engineering.md).
The original Windows gate completed 784 rows: 781 passed, two failed and one
configured Debug cache-byte course was skipped. Its diff-zoom failure matches
the reader/menu race. Key-free controls prove the Home oracle also sampled an
earlier menu command before its tab finished updating. The fixture now waits
for that command's actual displayed/core next-file result before reopening
Compose, retaining the original Home/full-tab assertion and draft round trip.
The exact fixture passes the Windows 4×3×3 course and four macOS public courses,
with strict format/tidy checks. No product navigation change is made.

## Publication closeout, 2026-10-20

Assignment `/root/release031_publish` completed the post-attended publication
round of mandate `release-v031-qt612-scroll-r1`, with the authored
`release-v031/publish-three-platforms` milestone unchanged. The
[publication record](../../verification/release-v031-publication.json) binds
all final names, sizes, SHA-256 values, acceptance identities and receipt hashes.

- Source and notes: Clean source `2ed7e5f98641d75678f861dad0edbe658b5f5d22` binds all
  producers. The tracked v0.3.1 notes now match the actual published notes;
  README links the verified installers and macOS 14.4 minimum.
- Native gates and assets: Full exact-source native gates complete all 2,392 rows: 2,389 pass and
  three configured Debug cache-course skips. macOS/Windows optimized courses
  pass. Ubuntu's strict optimized course remains FAIL 8 and its supplemental
  observation FAIL 1 under only the owner's existing sampled/lifetime 216 MiB
  RSS exception. All 28 signed assets, actual runtime/source/SPDX audits,
  independent bounded factual reviews and exact current material accepts are
  supplied. Broader RFC0038 qualification remains open.
- Public verification: [Public stable v0.3.1](https://github.com/MikeMarcin/BetterPullReview/releases/tag/v0.3.1)
  is Latest, not draft/prerelease. Annotated tag
  `728e6e75ad4df34a45b7801551272106bde456d5` peels to the frozen source.
  All 28 anonymous downloads match producer hashes; fresh macOS, Windows and
  Ubuntu native trust/source checks pass, including independent Windows
  five-file trust, downloaded MSI/runtime equality and Ubuntu tamper rejection.
  All three source assets have equivalent public access at no extra charge;
  source, snapshot, digests and tag are retained read-only indefinitely.
- Procedure record: [Measured procedure findings](../../verification/release-v031-procedure-findings.md)
  preserve test/producer/publication timings, retries, collector/dispatch
  failures, human wait and bounded evidence reuse. Safe improvements are
  recorded for the requested later retrospective; no workflow/gate change or
  speedup is claimed for this release.

The [final engineering record](../../verification/release-v031-engineering.md#published-coordinated-031-2026-10-20)
retains the failed earlier candidates, exact source/material/security limits and
command handoff. All release-owned commands completed; its finished native
trust task was removed after zero status and process absence. The coordinator
stopped both completed review servers without releasing candidate protections.
No producer revision, published asset or tag changed during administrative
closeout. BPR-1653's dedicated causal repair is the next separate scope;
RFC0038 and its wider native/security cards remain open.

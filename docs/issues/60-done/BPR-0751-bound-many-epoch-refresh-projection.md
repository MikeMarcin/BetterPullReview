---
schema: "implement-rfc.card.v1"
id: "BPR-0751"
title: "Bound many-epoch refresh projection on the public path"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 matched refresh attribution and owner-disposed isolated source overrun","outcome":"pass","revision":"f378da20","evidence":{"kind":"artifact","detail":"The matched 929-read pre-BPR-0635 product replay reproduces 228.745–265.034-ms refresh gaps (build/rfc0013-d3-pre0635-history.log), and build/rfc0013-d3-attribution.json shows identical pre/post fixture-commit product blobs. D2 main-thread/card/overview traces and repair identify the repeatable projection work. The separate 2,151.817-ms epoch-25 source event failed test 279 in unchanged build/bpr0635-composed-final.log. It has no contemporaneous stage trace, did not recur in the matched pre-commit or repaired public courses, and is closed as cannot reproduce under the owner’s r2 instruction. Its cause and pre-commit origin remain unknown; no retroactive pass or 2s-limit waiver is claimed."}},{"criterion":"AC2 unchanged 32-head public history limits and exact state","outcome":"pass","revision":"4a2be1b191a9854ef9c6a78f682415a4551b411b","evidence":{"kind":"artifact","detail":"build/rfc0013-d2-history-indexed-overview.log: network-isolated Mac 929-read course 1/1; all 31 refresh gaps 64.617–91.147ms <100ms, refresh <8.422s <12s, all 32 source selections <0.952s <2s; 32 exact heads and retained Git/catalog pairs, 31 rapid selections (30 during outstanding work), zero history-model resets."}},{"criterion":"AC3 focused regression, documentation, diff and native limits","outcome":"pass","revision":"f378da20","evidence":{"kind":"artifact","detail":"D2 focused Qt 8/8 and overview 3/3, preserved public courses 3/3, formatting and scoped tidy passed on unchanged source. D4 python3 scripts/verify_documentation.py --root . reports DOCUMENTATION_OK; documentation behavior tests 64/64, RFC-0013 board validation and git diff --check pass. Evidence is local M4 Pro/48-GiB Mac, macOS 26.6.2 arm64, Qt 6.10.3 offscreen/Fusion Debug; no Windows/Linux or universal responsiveness claim."}}]
---

# Bound many-epoch refresh projection on the public path

BPR-0635's 32-head synthetic Mac course passes exact provider reads, source,
Git/catalog identity and cache-byte eviction, but the existing history
regression fails its unchanged 100-ms UI timer-interval limit on every refresh:
237–255 ms. One source selection takes 2.152s against its 2s limit. The
current-source 25s/5ms sample in `build/bpr0635-post-lock-sample.txt` places
the main-thread work in accepted pull-request handoff, workbench observation
and publication, and Qt conversation projection; historical fixture script
compilation appears on a worker thread. BPR-0635's first fixture prerequisite
failed before timing, so whether the product projection delay predates that
candidate remains unproven. The failed run is
`build/bpr0635-composed-final.log`, test 279. This card splits the wider
product-path diagnosis from the synthetic byte-pressure fixture/course.

## Scope

- Compare an unchanged baseline and the BPR-0635 candidate under the same
  32-head synthetic, network-isolated Mac history course. Attribute the
  refresh and source-selection intervals before changing product behavior.
- If product publication is responsible, bound main-thread handoff, workbench
  and conversation projection work while preserving exact selected-file,
  semantic-change, comment and stale-result behavior. Keep core decisions in
  core, effects in application, and Qt presentation in the adapter.
- Do not lower the existing 100-ms UI interval, 12s refresh or 2s
  source-selection limits to obtain a pass. No real pull-request mutation,
  production credentials or remote service use.

## Acceptance

- The matched baseline/candidate comparison and measured main-thread
  attribution identify the cause of the 237–255-ms refresh pauses and whether
  that product pause predates BPR-0635. Preserve the isolated 2.152s
  source-selection failure and its available samples. Under the owner's r2
  decision, record that isolated event as cannot reproduce, with its stage,
  cause and pre-BPR-0635 origin unknown; do not retroactively pass the failed
  run or relax the unchanged 2s source-selection limit.
- The network-isolated 32-head public history course passes its existing Mac
  timing limits with all exact heads, 929 account reads, source selections,
  supersession and retained Git/catalog pairs intact.
- Focused host/Qt/composed regressions, documentation gate and diff checks
  pass. Record hardware and remaining native-platform limits; no universal
  responsiveness claim follows from one Mac run.

The authored plan groups this repair with BPR-0635's still-open preserved
history check. The prior BPR-0635 binding and its failed preservation check
remain historical; acceptance requires a renewed binding for both cards.

## D4 owner disposition — cannot reproduce the isolated source overrun

The owner's exact r2 instruction is "mark it as can not reproduce and close that
issue." It resolves only the first acceptance bullet's demand to identify the
historical 2.152s source-selection event's stage, cause and pre-BPR-0635
origin. The original demand and D3's finding that it was unsupported remain
preserved in the historical record below. The event failed the existing
2,000-ms limit at 2,151.817 ms in `build/bpr0635-composed-final.log`, test 279;
that failed log remains unchanged. Its 312 sampled 5-ms pulses with a diff job
outstanding, versus 112–113 in adjacent selections, are insufficient to name
the worker stage or scheduling cause. No contemporaneous stage trace exists.

The controlled 929-read pre-BPR-0635 product replay reproduced the refresh
pause but not the isolated source overrun: epoch 25 took 959.528 ms and all 32
source selections stayed below 1.018s
(`build/rfc0013-d3-pre0635-history.log`). The repaired 929-read public course
also did not reproduce it: all 32 source selections stayed below 0.952s and
the 31 refresh timer maxima stayed within the original 100-ms limit
(`build/rfc0013-d2-history-indexed-overview.log`). Those observations support
the owner's *cannot reproduce* disposition, not a claim that the historical
failure was false or that all future selections satisfy the 2s limit. The
refresh projection cause, product-baseline comparison, exact-data course and
focused checks remain independently required by the other acceptance clauses
and the milestone exits.

## D2 execution and verification record

The BPR-0632 older-fixture history course reported a 71.247-ms worst refresh
interval and 897 account reads. BPR-0635's 929-read fixture first failed a
historical replay prerequisite, so there is no exact numeric pre-BPR-0635 run
under the same course. The hot GitHub handoff, workbench and Qt conversation
source files were byte-identical before and after BPR-0635's fixture/course
commit `1681994a`; the costly product path existed before that commit, while
the exact 237–255-ms duration was first observable in its changed fixture.
The lone 2.152s source-selection overrun had an active diff job for 312 sampled
5-ms pulses versus about 113 on adjacent selections, with a 66-ms maximum UI
gap. Its deeper scheduling cause is unproven; no subsequent d2 source selection
exceeded 1.011s. These distinctions limit attribution to observed work rather
than assigning the isolated worker delay to Qt.

A 25s/5ms main-thread sample (`build/bpr0635-post-lock-sample.txt`) located
accepted PR handoff, synchronous workbench publication and conversation-card
layout. A bounded test-only card trace
(`build/rfc0013-d2-card-diagnostic-command.log`) showed 2,000 retained
conversation nodes widening from two selected-file cards to 1,000 whole-review
cards on refresh, then narrowing. The widened adoption missed 998 cached text
measurements and took about 32 ms; width changes invalidated the measurements
before the next refresh. A guarded visual anchor keeps the selected-file view
through the newer-generation observation gap and its engaged, fileless terminal
only when the exact file remains in the returned file list. Semantic selection
still clears; explicit same-generation clear, removed/empty results, failure
and GitHub authority clear release the visual hold. The real sequence was
captured in `build/rfc0013-d2-guard-diagnostic-command.log` and
`build/rfc0013-d2-terminal-diagnostic-command.log`; the first guard alone did
not meet the timer limit, so neither diagnostic is acceptance evidence.

The remaining GUI sample (`build/rfc0013-d2-post-terminal-sample.txt`) led to
overview attribution: all 25 sampled overview projections after the first
were exactly equal while each 515-file publication took about 38–45 ms,
including roughly 6–10 ms of receipt reads, 8 ms of pure projection and 23 ms
of Qt publication (`build/rfc0013-d2-overview-identity-command.log`). Qt now
suppresses equal overview page/tree rebuilds but restores file facts after a
workbench row clear and resets its comparison on authority clear. The pure
overview projection indexes thread facts by borrowed snapshot path for the
duration of one call rather than linearly searching every path. Focused Qt
tests pass 8/8, overview tests pass 3/3, the formatting check passes, and
independent read-only review found no demonstrated blocker in these changes.

The final uninstrumented, network-isolated Mac history course passes 1/1 in
307.47s (`build/rfc0013-d2-history-indexed-overview.log`, report
`build/dev/tests/end_to_end/large-review-history.json`). All 31 refresh timer
interval maxima are 64.617–91.147 ms under the unchanged 100-ms limit; refresh
elapsed time stays under 8.422s against 12s. The 32 source selections stay
under 0.952s against 2s. It reaches all 32 exact heads, 929 account reads,
32 retained Git/catalog pairs, 31 rapid selections (30 while work is
outstanding), and zero history-model resets. This is one M4 Pro/48-GiB Mac,
macOS 26.6.2 arm64, Qt 6.10.3, offscreen/Fusion Debug observation; it does
not establish a Windows/Linux or universal responsiveness bound. No production
account, live pull request, remote mutation, credential or push was used.

## D3 baseline and source-delay audit

The exact pre-BPR-0635 product tree is `1681994a^` (`d4da9428`). A disposable
checkout at that revision, overlaid with only the six fixture/test files from
`1681994a`, built and ran the network-isolated 929-read history course. It
reached 32 exact heads and 929 reads; all 31 refresh UI-interval maxima were
228.745–265.034 ms against the unchanged 100-ms limit
(`build/rfc0013-d3-pre0635-history.log`). Epoch 25 source selection took
959.528 ms and all 32 source selections stayed below 1.018s. The CTest failed
only on the refresh budget. This is a controlled pre-commit *product* baseline
under the later fixture, not a claim that the unmodified pre-commit test itself
could have produced 929 reads. The same checkout rebuilt with its original
897-read fixture reported `EPOCH_EFFECT_FAILED:revision-prepare:0` before
history timing (`build/rfc0013-d3-pre0635-original-history.log`); the diagnostic
was stopped after that precondition was clear. The earlier BPR-0632 897-read
pass at `75d1494e` remains a different product revision and workload.

Git blob IDs for the conversation-card view, conversation panel, workbench
window and pure overview projection are identical at `1681994a^` and
`1681994a` (`build/rfc0013-d3-attribution.json`). The BPR-0635 commit changed
only fixture/test files, adding one recorded timeline operation per epoch.
The 929-read pause therefore reproduces on product code that predates that
commit; its exact numerical duration on the *unmodified* 897-read pre-commit
course is unavailable because that course never reached timing. The 2.152s
source overrun is likewise absent in the controlled replay and in the other
929-read runs (epoch 25 about 0.94–1.00s). Its recorded 312 sampled pulses with
an outstanding diff job, versus 112–113 adjacent, localize the extra dwell to
the diff completion path; those samples do not distinguish effect-queue delay,
blob I/O, computation, indexing or completion delivery. No contemporaneous
stage trace exists for the isolated event. The underlying cause and whether it
predated BPR-0635 remain unproven. Independent read-only D3 review confirmed
that the first acceptance criterion stays open; another passing run cannot
establish the historical delay's cause without a captured recurrence or stage
trace.

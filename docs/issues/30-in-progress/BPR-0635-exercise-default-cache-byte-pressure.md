---
schema: "implement-rfc.card.v1"
id: "BPR-0635"
title: "Exercise default cache byte pressure"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0634","BPR-0612"]
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
---

# Exercise default cache byte pressure

BPR-0634 covers the default entry ceiling. Small eight-line results cannot
establish pressure against the independent default 64-MiB cache byte ceiling.

## Bounded acceptance and ownership

- Use the existing large-review stored heads and real public refresh/epoch/file
  selectors to admit enough distinct 20,000-line comparison keys to exceed the
  unchanged default 64-MiB retained diff/index budget before the entry ceiling.
  Do not lower limits, synthesize cached values or bypass installed Git.
- Verify exact source/anchor identities, bounded accounted cache bytes, actual
  eviction and weak raw/index ownership release. Publicly revisit an evicted
  key and verify its rematerialized exact source and stale-selection safety.
- Record hardware/workload, native memory, timings/input and bounded job/cache
  metrics before justifying any new workload-specific local thresholds. Exercise
  close/drain/destruction after pressure and preserve durable Git/catalog data.
- Reuse the existing benchmark/fixture configuration and inspection seams.
  Split wider defects if evidence requires different ownership. Preserve the
  entry-pressure course and earlier public acceptance; broader RFC and native
  platform closure remains separate.

RFC base `d4d9e61d9224ef869ca4de9725c2bf52c5df701a`; closeout BPR-0617.
Local Mac, synthetic and network-denied only. No Actions, dispatch, push, paid
CI, credentials or production account/process operations. Earlier human/live
and Windows/Linux gates and deferred work remain open.

## Sequencing

Owner steering prioritizes the full Glaze migration in BPR-0612 before further
performance work. Resume this course after that migration so the measurements
exercise the intended parser implementation.

## Verification and execution record

The network-isolated Mac public application uses the stored 515-file,
1,000-thread, 32-head large-review fixture after completed BPR-0612. Its
synthetic account replay now includes the independently paged empty timeline
request at each head. The acquired historical Git transport rejects a substituted
disconnected head and an additional disconnected want at its actual `fetch()`
boundary before delivering any bytes; the normal installed-Git fetch succeeds.
The composed course exhausts 929 exact account reads, refreshes all 32 heads,
loads retained history, and selects the 20,000-line comparison for distinct
public epoch anchors. Every selected key has the fixture's exact base/head OIDs,
blob identities and 20,000 displayed source lines.

The unchanged default limits are 64 entries and 64 MiB. Each large immutable
diff/index key accounts for 3,517,688 bytes. The twentieth public selection
raises cumulative admitted bytes to 70,353,760, above 64 MiB while below the
entry ceiling; the cache retains 19 large keys / 66,836,072 bytes and both weak
owners of the first raw diff and prepared index expire. During a new request for
that evicted key, the course selects small file B while work is outstanding,
holds B until diff, workbench, epoch and classification work drains, watches
eight synchronous source changes for a stale A publication, and verifies B's
exact key and eight displayed source lines. A later public revisit rematerializes
the original A key and its exact large source. Overall peak accounting after
the intervening small file is 20 entries / 66,839,408 bytes, still below both
default ceilings; no fixture diff helper is invoked.

On the recorded M4 Pro/48-GiB Mac, macOS 26.6.2 arm64, Qt 6.10.3,
offscreen/Fusion Debug build, the final corrected byte course passes in 359.25s
(`build/bpr0635-composed-final.log`, test 275). There are 1,718 native
measurement batches capped at 512 lines and 1,717 accepted input callbacks;
their maximum/p95 gap is 13.976/4.407 ms. Sampled peak jobs are one diff,
one workbench request and three epoch jobs. Refresh phases take about 8.9s,
large selections about 3.1s, and the final revisit 1.521s. Whole-phase timer
intervals include values above 100 ms, distinct from the native measurement
callback gaps; RFC-wide responsiveness remains open.

Physical footprint is 4.735 MiB before application construction, 157.142 MiB
before loaded close, 156.673 MiB after shutdown drain, and 45.173 MiB after
application destruction. The byte-course-only 160-MiB after-drain sampled
growth budget passes at 151.938 MiB; the prior 96-MiB destruction-growth
budget remains unchanged and passes at 40.438 MiB. These are sampled local
values, not unsampled peaks or other-platform budgets. RSS remains 248.469 MiB
after destruction, so object-release and physical-footprint evidence should not
be read as a guarantee that the allocator returns all resident pages. Public
window close drains cache/jobs to zero; destruction releases watched raw diff,
prepared index, window and native source documents. All 32 exact retained
Git refs/catalog pairs remain present with zero prune/delete action. The final
1600×1000 window render shows the selected comparison and 20 semantic changes;
the dense 32-node update-strip labels overlap and remain outside this resource
card's acceptance.

The earlier entry-pressure, loaded-close, prune/delete resource and direct replay
courses passed together before the independent fixture review. That review found
two acceptance-evidence gaps: historical Git fetch could bypass exact-head
validation through the disconnected-pack branch, and A→B→A could hide a transient
stale A publication. Both were corrected in the fixture/benchmark course above.
The final corrected byte course and the deterministic S1→S2 fixture session
interleaving and direct replay regressions pass (`build/bpr0635-composed-final.log`
and `build/bpr0635-session-race-direct-final.log`). The earlier corrected
32-epoch history regression reached all exact heads, sources,
929 account reads and retained Git/catalog pairs, but failed its existing
100-ms Mac UI-interval budget after the fixture changes: refresh pulses reach
about 247 ms (`build/bpr0635-reviewed-byte-history.log`, test 278). A new
unconditional status read on every readiness tick was guarded to the byte
course, yet the affected history-only rerun still fails at 241–249 ms
(`build/bpr0635-history-readiness-final.log`). Timing attribution shows the
GUI readiness timer waiting on the fixture state mutex while the next large
historical script compiles under that lock. Fixture-only compilation outside
the shared lock with checked publication is in progress; the pre-existing
history limit is not waived. Independent critical review of the unlocked
fixture compilation found a combined history/session-restart race: an old
authenticated request could succeed against a rewound new-session replay.
A typed post-relock authentication refusal and focused actual-transport
negative now pass, and independent critical re-review finds no remaining
S1→S2 blocker. The fresh five-course serial regression passes byte pressure,
entry pressure, loaded close and resources, but the unchanged 32-epoch history
course still fails its 100-ms Mac UI-interval limit: all 32 exact heads,
929 reads, sources and retained storage pass, while every refresh shows a
237–255-ms timer gap (`build/bpr0635-composed-final.log`, test 279). The out-of-lock
fixture compilation removed a measured GUI evidence-mutex wait, but did not
remove that stall. One source-selection phase also took 2.152s against its
existing 2s limit. A current-source 25s/5ms sample places the remaining GUI
work in public handoff, workbench publication and Qt conversation projection;
BPR-0751 tracks baseline comparison and the wider product-path repair. The
independent reviewer found that the failing history check prevents BPR-0635
and this bound milestone from passing even though the byte course passes;
neither limit is waived. No completion or RFC-wide closeout is inferred until
corrected history, documentation and board acceptance are recorded. No
production account, live PR or remote mutation was used.

Source/CTest registration checkpoint: `1681994afab3ad9075616ce641dad49544a25d1c`.
The final byte/entry/loaded-close/resources run and focused race/direct replay
checks passed as above; the same serial run's history failure remains binding.
The documentation gate (`build/bpr0635-documentation-final.log`), its 64
behavior tests (`build/bpr0635-documentation-tests-final.log`), network-isolation
manifest (`build/bpr0635-isolation-final.log`), project formatting check
(`build/bpr0635-format-final.log`) and scoped `git diff --check` pass. BPR-0635
stays In Progress with no approved Done acceptance decision.

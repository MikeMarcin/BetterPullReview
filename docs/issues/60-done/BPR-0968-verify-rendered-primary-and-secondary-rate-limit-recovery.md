---
schema: "implement-rfc.card.v1"
id: "BPR-0968"
title: "Verify rendered primary and secondary rate-limit recovery"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0968-rate-limit","path":"docs/rfcs/plans/bpr0968-rate-limit/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7c71c570d1a0391f011a480456fc058210db88b9","evidence":{"kind":"observation","detail":"Real macOS composition over denied-network recorded transport displays distinct primary/secondary refusal in Account Details and enforces shared admission. 515 files, 1000 threads, real Git 20000-line comparison. Inspected Settings and recovered-reading rasters; integration log and hashes in docs/rfcs/plans/bpr0968-rate-limit/evidence.json."}},{"criterion":"AC2","outcome":"pass","revision":"7c71c570d1a0391f011a480456fc058210db88b9","evidence":{"kind":"command","detail":"core_github_read_backoff, application_github_window_read_host and application_rate_limit_render_recovery pass. Shared multi-window Open/Refresh, Retry-After/reset precedence, 60-900s fallback cap, zero automatic rate retries, three transient attempts with re-admission, authority retirement/cancel/stale completion and exact selection verified. Public course blocked 61 gestures, intervals 2468622/2498662/5785721us; selected line100/scroll33333 exact after work settles; actual held provider cancelled on close."}},{"criterion":"AC3","outcome":"pass","revision":"7c71c570d1a0391f011a480456fc058210db88b9","evidence":{"kind":"artifact","detail":"Final public course: 119 ordinary account requests, eight repository starts, three refusals, zero mutations, 33053ms. Backoff-removal and LastProblem-projection-removal each rebuild and fail intended public assertion (CTest exit8); exact source digests restored, then final integration 8/8. Candidate, inputs, timings, hashes, baseline failures and reproduction commands recorded in evidence.json and HANDOFF-2026-09-25.md."}}]
---

# Verify rendered primary and secondary rate-limit recovery

## Transferred obligation

Original RFC0013 acceptance row 4's full rendered backoff course.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. At transfer this card was unfinished; no observation was supplied by
the transfer and it creates no RFC0014 release qualification.

Provider/pager and BPR-0161 scheduler evidence covers bounded backoff, Retry-After, cancellation, exact session identity and typed states. It does not establish the complete rendered primary/secondary rate-limit workflow for a large review. RFC0013 r4 accepts that bounded component evidence and transfers this unobserved public course without inventing a pass.

## Acceptance

- AC1: Drive primary exhaustion and secondary throttling through the real composition using a safe local provider/transport fixture; observe useful distinct visible states and bounded scheduling, without contacting a production account.
- AC2: Verify Retry-After, capped retries, explicit retry/cancel, authority replacement and recovery preserve exact selection and do not produce request storms or stale publication.
- AC3: Record the exact candidate, input/request counts, rendered state and timing evidence; demonstrate the course fails if backoff or state projection is removed.

## Continuation

AC1–AC3 are accepted for the safe macOS composition at the full-card candidate
below. No card obligation remains. The coordinator owns publication of the
commits; Windows/Linux remain deferred by the user. This card supplies no live
account, native appearance or release qualification.

## Verification and execution record

## macOS characterization — 2026-09-25

Assigned baseline `7c07a1cb`, standalone mandate r4; candidate `34c86e0b`.
The new `application_rate_limit_render_recovery` course loads the real
515-file/1,000-thread composition over denied-network recorded transport.
Primary and secondary HTTP 403 responses are normalized by the real provider,
with separate rate causes and `Retry-After: 2`.

A reproduced presentation omission was repaired: an authenticated PR refresh
failure kept its diagnostic in the session model but dropped **Last problem**
from the connected server card. Account → Details now renders both distinct
messages and retry advice. The full Settings rasters were inspected locally.
A later explicit Refresh restores the exact repository, viewer, PR/head, files
and thread records. This is no claim about file selection, scroll position,
native appearance or a production account.

### Acceptance state at the characterization candidate

- AC1 is partial: primary/secondary rendered states and safe composition pass;
  bounded scheduling does not. Default CTest intentionally names only rendering
  and recovery, not full rate-limit acceptance.
- AC2 fails the existing public path. Each immediate second Refresh reaches the
  transport: the requirement run recorded 86,831 microseconds for primary and
  29,886 microseconds for secondary, both below the advertised 2,000,000
  microseconds. It made 88 ordinary account requests plus four refusals and zero
  mutation submissions. `--require-backoff` exits 1 at the explicit admission
  assertion. Retry caps, explicit cancellation, authority replacement and exact
  file-selection retention still require the full course.
- AC3 has exact workload/count/timing and a projection-removal negative control;
  full scheduling acceptance remains unsatisfied. See the milestone handoff for
  the final negative-control and restoration results.

### Engineering frontier at the characterization candidate

`github_window_read_host::dispatch` admits Open/Refresh directly into the core
window transition; that lane owns no retry deadline or retry queue.
`queue_refresh_scheduler` records a 15-second exponential delay capped at 900
seconds, but the composed queue refresh port replays an already accepted
snapshot and does not gate the PR read. Personal discovery separately owns a
clock/deadline. Reusing the queue component tests would therefore miss this
public failure.

The next engineering round must bind the PR-read scheduling policy at its actual
shared host/account/resource boundary, including multiple windows, Open versus
Refresh, primary reset versus Retry-After, cancellation, authority retirement and
automatic retry limits. The existing normative documents specify bounded
backoff generally but do not settle that PR-window policy. This card stays
unfinished; no owner decision or scheduler repair is invented by this display
fix. Windows/Linux remain deferred under the user's overnight instruction.

## Full-card implementation — 2026-09-25

The continued assignment owns all AC1–AC3 on main from
`7f0111ec3278a6a1ed7011752e359e2367646c99`, under mandate r4 and the owner's
same-implementer/full-card amendment. Candidate
`7c71c570d1a0391f011a480456fc058210db88b9` closes the earlier scheduling frontier.
No policy question required owner intervention. The
[architecture contract](../../architecture/system-architecture.md#concurrency-and-stale-work)
records the implemented policy and boundaries.

The profile runtime now shares host/account/resource admission between all PR
windows. Primary reset and Retry-After use the later minimum; secondary reset
metadata cannot masquerade as its retry time. Missing server minimums back off
60–900 seconds, without capping an explicit server minimum. Rate failures have
zero automatic retries. Every initial or transient retry submission checks the
shared cooldown, which survives reauthentication and cannot be cleared by a
concurrent older success. Existing transient attempts stop at three. No deferred
retry retains request or credential authority. Closing a window cancels its read.

The default public course now enforces bounded scheduling. It loads 515 files,
1,000 threads and the real 20,000-line comparison through installed Git. Three
scenarios cover primary/secondary Retry-After 2 and primary reset 4 versus
Retry-After 1. Sixty repeated Refresh/Open gestures across sibling windows, plus
one after the shorter primary Retry-After, produce no extra request. Recovery
retains the exact comparison key, right-side line 100 and scroll 33333 after all
workbench/epoch/diff work settles. A real held files-page request is cancelled
by closing its window without changing the surviving review. The course makes
119 ordinary account requests, eight repository starts, three refusals and zero
mutations. Expiration alone starts nothing.

Removing admission makes the course fail at its wire-count assertion; removing
Last problem projection fails at the visible Details assertion. Both sources
were restored exactly. Core/host tests additionally cover precise deadlines,
clock movement, fallback/attempt/registry caps, resource and account separation,
session retirement, cancelled and stale terminals, and a transient retry after
a sibling observes throttling. The
[current handoff](../../rfcs/plans/bpr0968-rate-limit/HANDOFF-2026-09-25.md) and
[evidence manifest](../../rfcs/plans/bpr0968-rate-limit/evidence.json) retain the
exact commands, timing, negative controls and gate dispositions.

This acceptance concerns the safe macOS fixture course. Native appearance,
live GitHub/account behavior, Windows/Linux and release qualification are not
supplied; the user deferred other platforms. No attended session was created.

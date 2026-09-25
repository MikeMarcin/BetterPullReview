---
schema: "implement-rfc.card.v1"
id: "BPR-0968"
title: "Verify rendered primary and secondary rate-limit recovery"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0968-rate-limit","path":"docs/rfcs/plans/bpr0968-rate-limit/milestone-plan.json"}}
---

# Verify rendered primary and secondary rate-limit recovery

## Transferred obligation

Original RFC0013 acceptance row 4's full rendered backoff course.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

Provider/pager and BPR-0161 scheduler evidence covers bounded backoff, Retry-After, cancellation, exact session identity and typed states. It does not establish the complete rendered primary/secondary rate-limit workflow for a large review. RFC0013 r4 accepts that bounded component evidence and transfers this unobserved public course without inventing a pass.

## Acceptance

- AC1: Drive primary exhaustion and secondary throttling through the real composition using a safe local provider/transport fixture; observe useful distinct visible states and bounded scheduling, without contacting a production account.
- AC2: Verify Retry-After, capped retries, explicit retry/cancel, authority replacement and recovery preserve exact selection and do not produce request storms or stale publication.
- AC3: Record the exact candidate, input/request counts, rendered state and timing evidence; demonstrate the course fails if backoff or state projection is removed.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.


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

### Acceptance state

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

### Engineering frontier

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

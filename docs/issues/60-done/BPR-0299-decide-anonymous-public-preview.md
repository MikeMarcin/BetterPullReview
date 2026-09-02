---
id: BPR-0299
title: Decide whether to build anonymous public preview
status: Done
rfc: RFC-BPR-0017
depends_on: [BPR-0298, BPR-0313]
---

# Decide whether to build anonymous public preview

## Required final state

- A digest-bound product decision accepts or rejects the measured request cost,
  shared-address quota behavior, refresh behavior, triage/viewer/action loss,
  private-repository ambiguity, and GitHub.com-only scope.
- Approval defines exact public-preview naming, limit presentation, sign-in
  handoff, and supported-platform policy and grants no mutation or private read.
- A `do not build` result leaves RFC-BPR-0017 Deferred and BPR-0312 unresolved,
  so every implementation card remains dependency-blocked. It is recorded as
  evidence, not converted into Auto-Verified or a completed positive gate.
- Any available-target implementation exception is independent, cannot waive
  RFC-BPR-0002 final closeout, and updates RFC/card dependencies atomically.

## Parked on the product decision — 2026-09-01

[[BPR-0298]] and [[BPR-0313]] are done, so the measured facts this decision
rests on exist and are digest-bound: the sanitized captures under
`docs/verification/reviews/rfc0017-anonymous-probe-v001/captures/` plus the
conditional-request pair recorded by the family's live packet,
`rfc0017-anonymous-probe-v005` (60 core
requests per hour per source address, shared; a `304 Not Modified` costs one
of them; GraphQL refused with limit 0; REST review comments answered `200`
anonymously; 404 rather than 403 for a private or missing repository) and
RFC-BPR-0017's own recommendation that RFC-BPR-0016 is the stronger answer.
The decision is a choose-one subject in
`docs/verification/reviews/run-boundary-decisions-v001/offline-package/index.html`
(group `anonymous-public-preview`). A "do not build" outcome is recorded as
evidence on this card and leaves RFC-BPR-0017 `Deferred` with [[BPR-0312]]
unresolved; a "build" outcome must carry the naming, limit presentation,
sign-in handoff, and platform policy in its note, which [[BPR-0312]] then
validates.

## Decided: build it — 2026-09-01

The product owner chose to build anonymous public preview as a
credential-optional mode on the existing read path, on the
`run-boundary-decisions-v001` page (batch
`bpr:/attended-review/betterpullreview/c7e77830ef4fa92c6047c99b`, subject
`decision:/betterpullreview/bpr-0299/build-credential-optional-preview`
accepted, `do-not-build` recorded as not chosen; validated verdict and record
beside the page). The decision was taken against the measured facts the
subject bound: the shared 60-per-hour core budget, a 304 costing one unit,
GraphQL refused at limit 0, REST review comments answered 200 anonymously,
404 non-disclosure, and GitHub.com-only scope.

What the acceptance did not carry: the subject asked for a note defining the
public-preview name, how the limits and the missing review threads and triage
are presented, the sign-in handoff to the device flow, and the
supported-platform policy. The verdict has no note, so those four definitions
are still owed; they are exactly what [[BPR-0312]] validates, and they are
requested from the owner in the run report. This card's decision fact — the
mode is wanted — is settled and recorded as evidence.

Consequences applied in this commit: RFC-BPR-0017 leaves `Deferred` for
`Proposed` in its front matter and the index's deferred table (its three
eligibility gates now hold: RFC-BPR-0002 is Auto-Verified, [[BPR-0298]]
recorded the probe evidence, and this decision), and [[BPR-0312]] moves to
`20-ready/`. Every implementation card from [[BPR-0300]] onward still depends
on BPR-0312, so nothing can start before the four definitions are bound.

# RFC-0024 planning reconciliation — 2026-09-21

Run `rfc-0018-0026-closeout-20260920`, segment `rfc-0024`, milestone
`close-rfc-0024`; sole engineering/bookkeeping owner `/root/rfc0024` in
`/Users/jaeger/.codex/worktrees/0152/BetterPullReview`, branch
`codex/rfc-0018-0026-closeout`. The resolved coordinator-implementer strategy
and immutable RFC base `e851eb8a851ab605cf5a85260a1317ecdf66773b` are unchanged.
This continues the ordered pack after RFC-0018's owner amendment r2; it does
not extend that amendment's acceptance transfer to RFC-0024.

The owner authorized planning-stage updates to stale RFCs. The coordinator
clarified that the original implementation/closeout mandate includes unfinished
RFC-0024 product/native obligations, not only the stale closeout-only boundary.
The required behavior already exists in RFC-0024's accepted criteria; no
criterion is removed, weakened, or accepted by this planning reconciliation.

The main integrator reserved four IDs in commit `0c65b928`, incorporated here
as `d7606766`: BPR-0976 server-conversation fanout; BPR-0977 native geometry
and focus/lifecycle; BPR-0978 concurrent two-account same-server profile
isolation; BPR-0979 Windows activate/New Window handoff. BPR-0715 retains
its exact signed-out native acceptance. These are required BPR-0595
dependencies. BPR-0242 is Done; BPR-0974/0975 remain independent RFC-0018
follow-ups and must not be changed here.

The first mechanical amendment check refused new work IDs because it requires
an identical obligation set (`amendment must preserve every obligation;
reconcile changed requirements separately`). This document records that
authorized reconciliation: register the four new IDs as unplanned work, then
check their grouping into the existing milestone with all previous selectors,
exit criteria, scope, and predecessor relationships preserved.

The regrouping check passed: reconciled partial-plan fingerprint
`sha256:de6be5c2c20250b3f49e1ab4a95ad14e8fff332ae79ef27d7e8b4054fcdb9d02`
to complete-plan fingerprint
`sha256:d474f4560828d879c20cf10fd329f5c5be35e494c4aa1cdb0d7a6c38e09a35da`.
Only `close-rfc-0024` changes; semantic acceptance is still an engineering
and required-review obligation, not a helper verdict.

The retained accepted v007 signed-out/epoch verdicts are candidate-specific,
not new acceptance of a rebuilt app. Their applicability to BPR-0715 must be
explicitly reconciled; any remaining human decision uses the configured
attended-review workflow. Native Windows/macOS/Linux observations cannot be
inferred from an offscreen run. Finish independent implementation first, then
prepare exact pending review candidates if required. No live GitHub/GHES
mutation or changes to protected station checkouts are authorized.

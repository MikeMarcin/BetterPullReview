# Overview folder collapse and final reconciliation

## Assignment and mandate

Run `rfc0037-run-20260918`, segment `overview-folder-collapse-20260920`,
mandate revision `1`; owner `/root/rfc0037_final_reconcile` under the selected
coordinator-implementer recipe. The coordinator confirmed that the existing
mandate to implement RFC-BPR-0037, revise stale planning and land after closeout
includes the newer owner request BPR-0934 (attended decision 441, 2026-09-20).

The source/tracker root is `/Users/jaeger/.codex/worktrees/714d/BetterPullReview`,
branch `codex/rfc0037-overview-folders`, starting revision
`a578f8ec07fb1dd1eb98192e61ab8473152b023b`. The immutable RFC registration
baseline remains `9bd2002b859ce4c5599d86dcc51771aa365fa90b`.

## Plan reconciliation

Target discovery found the adopted four-milestone plan. All four are accepted
by their existing returns, ending with `milestone-return-rfc0037-dispatch-004.json`.
The current board contains twelve Done cards and BPR-0934 in Backlog, with no
unmet dependencies. The newer card was missing from the adopted plan.

`milestone_plan.py check-amendment` refused an additive proposal with
`amendment must preserve every obligation; reconcile changed requirements separately`.
That helper checks same-scope refinements only. The owner request and coordinator
assignment explicitly authorize this added requirement, so it is reconciled as
AC-10 and an appended `overview-folder-collapse` milestone. A structural comparison
confirmed the original four milestone definitions, work selectors and exit texts
are byte-for-byte equivalent as JSON; no original obligation or pending acceptance
was removed. Original intake/proposal/provenance records are unchanged.

## Current frontier

The Overview tree explicitly disables expansion, uses flat full-folder groups,
and calls `expandAll()` on every presentation. This prevents the requested control
and would overwrite a collapse on refresh. The bounded repair owns the Overview
model/view, focused Qt and composed public tests, BPR-0934 and this RFC's current
plan/verification records. No provider or publication authority is changed.

The previous implementation candidate `e4096e05` already belongs to main history.
Formal status remains Proposed: RFC-0008, 0013, 0015 and 0028 are Proposed and
0029 is Accepted. The separate main checkout is actively owned by BPR-0954 with
uncommitted release-maintenance changes. It must not be changed or paused by this
milestone. Local build output is private to this checkout; the already installed
Qt package is consumed read-only from the main checkout's dependency cache.

Pending: implement and exercise BPR-0934, record exact evidence, finish its card,
reconcile the extended RFC scope, and return the candidate without forcing main.
The original macOS attended decisions remain historical acceptance; they do not
certify the new folder interaction. This card requires a rendered automated course,
not a new owner verdict. No review server is running or required for this follow-up.

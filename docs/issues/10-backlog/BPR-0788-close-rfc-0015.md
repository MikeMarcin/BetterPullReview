---
schema: "implement-rfc.card.v1"
id: "BPR-0788"
title: "Verify and close RFC-BPR-0015"
status: "Backlog"
rfcs: ["RFC-BPR-0015"]
depends_on: ["BPR-0787"]
closeout_for: ["RFC-BPR-0015"]
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0015

RFC base: `6c9ef2038b7389f49028e71f2bb8219b07191267`.

## Closeout checklist

| Criterion | Direct check | Current result |
|---|---|---|
| The opt-in account row names host, observed login, configured-CLI trust risks and read-versus-publish choice without silently replacing a native profile or claiming exact account containment. | Rendered account row and explicit opt-in exercise | Pending BPR-0784 |
| A tests-disabled macOS application connects through configured `gh` and renders a representative fixture PR via the real public path; missing or changed executable, host, account or config produces a typed refusal without credential fallback. | Tests-disabled Mac composition and refusal fixture | Pending BPR-0785 |
| CLI reads preserve actual status, headers, body, correlation, bounded pagination, cancellation and redacted errors; removing the process or response adapter makes a direct public-path negative control fail. | Adapter/provider contract and public negative control | Pending BPR-0785 |
| Fixture review and inline-comment publishing traverse ordinary draft and Submit review admission, explicit CLI write confirmation, exact target and observed-account checks, bounded transport, reconciliation and typed terminal projection; a read opt-in or ambiguous write cannot issue a second POST. | Fixture public mutation path and ambiguity control | Pending BPR-0786; no live PR write authorized |
| Token and private response content remain absent from parent arguments, environment, logs, diagnostics, settings, SQLite and committed evidence; the app does not change global `gh` authentication or configuration. | Canary and source/runtime audit | Pending BPR-0785/0786 |
| Target-native qualification records supported platforms and residual executable/config/CA/redirect/account/process risks; unsupported platforms remain unavailable and canonical closeout awaits RFC-BPR-0014 and all evidence. | Native qualification, docs and RFC index reconciliation | Pending BPR-0787; RFC-BPR-0014 Proposed |

- [ ] Every revised RFC criterion has direct command/public-path evidence, producing revision and explicit residual limitation.
- [ ] The documentation gate, focused public-path checks, integration check and independent authority/protocol review are adjudicated.
- [ ] RFC-BPR-0014 is Auto-Verified before RFC-BPR-0015 becomes terminal; pending native, human or safe-target evidence remains open.
- [ ] RFC front matter, index and live cards agree. The historical strict-containment analysis is retained, not claimed passed by opt-in warnings.

Dispatch `d1` checkpoint under run `rfc0015-r1`, mandate `r1`:
`opt-in-cli-read` remains in progress (BPR-0784 and BPR-0785). The macOS
tests-disabled composition has a warning-labeled CLI read opt-in, consent
refusal probes, observed `/user` identity, a bounded `gh api --include`
process/response adapter, and pre/post account and default-configuration
observations. A status-only read check of the already configured `gh` returned
HTTP 200 for `/user` and HTTP 404 for a nonexistent public PR; no response
body or token was retained as evidence. These checks do not establish a
complete public PR render or authorize a live PR write.

Independent critical review found that CLI-backed Git-object transport for
cold full diffs is absent, so the representative tests-disabled public render
and process/response negative controls remain pending. Nonzero HTTP-200
GraphQL errors fail closed as generic `unavailable`, but error/rate-limit
classification remains incomplete. The next bounded objective is to supply
the credential-bound Git transport, exercise a complete fixture PR through the
public path with negative controls, and classify GraphQL error responses
without exposing content. Publishing and safe-target evidence remain for
BPR-0786. Existing unrelated working-tree edits are excluded.

Checkpoint verification: macOS release and dev builds passed; four offscreen
release consent/preview probes passed with zero preview requests; production
host-symbol, documentation, formatting and architecture checks passed. Eight
of nine focused CTest cases passed. The composition case initially failed
because its whole-application source digest still named the prior revision;
the reviewed digest inventory was resealed, its previously failing hostile
source-control test passed directly, and the review round-trip composition
CTest passed. A full rerun of the nine-case focused set is not yet recorded.
Independent critical review retained both blockers above.

Dispatch `d2` Git-object feasibility: the installed `gh` 2.100.0 accepted an
absolute public Git Smart discovery URL, but `gh api` returned HTTP 401 with
`WWW-Authenticate: Basic realm="GitHub"`. Only status, selected challenge
header and byte counts are retained in ignored
`build/rfc0015-d2-gh-smart-feasibility.json`; no token or response body was
retained. The [tagged CLI implementation](https://github.com/cli/cli/blob/v2.100.0/git/client.go#L922-L944)
delegates `gh repo clone` to Git and [configures](https://github.com/cli/cli/blob/v2.100.0/git/client.go#L147-L157)
a `gh auth git-credential` shell helper for authenticated cloning;
`gh auth setup-git` changes persistent Git credential-helper configuration.
Neither route establishes this milestone's exact leased Git-object access
under the no-token, no-shell and no-global-auth-change constraints. A public
`gh pr diff` read succeeded without token extraction (ignored metadata-only
`build/rfc0015-d2-gh-pr-diff-feasibility.json`), but a patch command is not
the application's current Git-object transport and has not been composed as
a selected-diff source. Preserve the full-diff limitation in the account
warning. The representative fixture read probe is being assessed separately;
do not mark BPR-0785 or an exit passed from the Git Smart 401 or a successful
CLI command alone.

The d2 GraphQL correction preserves typed `RATE_LIMITED` cause, retry and
numeric rate metadata from a completed nonzero HTTP-200 CLI response while
refusing partial success and response prose. Adapter and provider tests pass,
including the application error-validity invariant. Independent critical
review found no remaining defect in this bounded correction. This is
component evidence, not a substitute for the unresolved public PR render.

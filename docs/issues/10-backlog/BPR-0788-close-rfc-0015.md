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

Current frontier under run `rfc0015-r1`, dispatch `d1`, mandate `r1`:
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

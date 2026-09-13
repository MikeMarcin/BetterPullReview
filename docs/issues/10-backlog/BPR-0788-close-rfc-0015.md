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
warning. A d2 tests-disabled fixture attempt reached fake `gh --version` and
`gh api user`. After correcting the fixture's Core rate-limit headers, the
viewer was accepted, but capability discovery refused the fixture's
review-thread response because it lacked the exact split capability shape;
no PR request or render followed. The process-failure and malformed-response
negative courses refused the account and shut down cleanly. This is a fixture
gap, not positive public-path evidence. The auto-consent release probe was not
retained because it did not pass and would add an unattended opt-in path to
the product image. Only sanitized failure facts remain in ignored
`build/rfc0015-d2-cli-public-probe-attempt.json`. Do not mark BPR-0785 or an
exit passed from the Git Smart 401 or a successful CLI command alone.

The d2 GraphQL correction preserves typed `RATE_LIMITED` cause, retry and
numeric rate metadata from a completed nonzero HTTP-200 CLI response while
refusing partial success and response prose. Adapter and provider tests pass,
including the application error-validity invariant. Independent critical
review found no remaining defect in this bounded correction. This is
component evidence, not a substitute for the unresolved public PR render.

Dispatch `d3` under run `rfc0015-r2`, mandate `r2`, retains the entire
`opt-in-cli-read` milestone and its pending BPR-0784/BPR-0785 and exit
criteria. The owner authorized a carefully bounded revision of the old
no-helper trust gate to pursue Git objects with the already configured `gh`.
The RFC and live plan now permit an exact-repository `gh` operation that may
internally run Git and `gh auth git-credential`; BetterPullReview must not
read token bytes, invoke the helper or shell directly, change global
authentication, or silently fall back to native credentials. The plan
amendment also permits credential-free local Git to serve cloned objects
through the existing v2 pack pipeline without another network or helper
operation. Both plan amendment checks reported `amendment-checked` with
current-milestone impact;
the renewed d3 binding and frontier are in ignored
`build/rfc0015-r2-d3-assignment.json` and
`build/rfc0015-r2-d3-frontier.json`. The explicit helper, executable,
configuration and account-switch risks are disclosed in the pending UI and
documentation changes. These amendments are not Git-object or public-render
acceptance evidence. A tests-disabled rendered fixture PR and negative
controls remain required before the first milestone can pass; publishing
and any live PR mutation remain outside this dispatch.

The d3 launch probe then found that official `gh` 2.100.0 can migrate legacy
`config.yml` and `hosts.yml` on its first `--version` or `api` call, before a
post-request metadata check can prevent a global configuration write. A macOS
`sandbox-exec` global file-write denial refused that migration without changing
the files; the same denial with a narrowly owned temporary-store exception
allowed a current-schema exact-commit clone and local Git v2 pack probe. The
RFC, adapter guide and plan were amended again to require this OS-enforced
boundary for every `gh` launch and to fail closed when it is unavailable. The
third amendment check preserved all obligations and exits and affected only
`opt-in-cli-read`; the renewed d3 binding is in the ignored assignment. These
are feasibility checks, not production or Keychain acceptance. The isolated
tests-disabled fixture UI connected with its application storage in a
task-owned home, showed `Pull request ready` for fixture PR #17, and displayed
two files and a synthetic comment. Its footer still reported that the exact
comparison was unavailable: no Git-object/full-diff result is established.
Removing `gh` and supplying malformed HTTP framing each produced a visible
typed account refusal with Open and Refresh disabled, without a native fallback
observed in the UI. The fixture harness test passed; structural observations
are retained in ignored `build/rfc0015-cli-public-fixture-evidence/observations.json`.
The negative controls did not remove compiled adapters or instrument the
native-network path, so the direct missing-adapter control remains open.

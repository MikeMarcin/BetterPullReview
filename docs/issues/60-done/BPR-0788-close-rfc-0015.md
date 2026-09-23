---
schema: "implement-rfc.card.v1"
id: "BPR-0788"
title: "Verify and close RFC-BPR-0015"
status: "Done"
rfcs: ["RFC-BPR-0015"]
depends_on: ["BPR-0787"]
closeout_for: ["RFC-BPR-0015"]
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
acceptance: [{"criterion":"C1 opt-in account row names host, observed login, configured-CLI trust risks and read-versus-publish without replacing a native profile or claiming exact containment","outcome":"pass","revision":"c1a336a0","evidence":{"kind":"observation","detail":"r3 tests-disabled Release course of 841999ad (CLI-only card naming octo-reviewer, BPR-0933 read question, declining leaves it signed out) reused: the only CLI source change since is BPR-0896's refusal in gh_cli_transport.cpp. r4 fresh tests-disabled Release self-test in application_composition_contract reports CLI_ENTRY_OFFERED_OK and CLI_ENTRY_CARDS; all six hostile consent sources are refused (build/rfc0015-r4/composition-contract.log)."}},{"criterion":"C2 tests-disabled macOS app connects through configured gh and renders a fixture PR; missing or changed dependencies refuse without credential fallback","outcome":"pass","revision":"c1a336a0","evidence":{"kind":"command","detail":"r3 Release course (review.cpp Diff ready, 11-16/21-26; process-failure fixture Could not connect) and d3 compiled knockout reused. r4: rfc0015_cli_read_fixture_tests.py 6/6, application_public_path and application_public_path_ghes_cli_unavailable pass at c1a336a0."}},{"criterion":"C3 CLI reads preserve status, headers, body, correlation, bounded pagination, cancellation and redacted errors; removing the adapter fails the public path","outcome":"pass","revision":"c1a336a0","evidence":{"kind":"command","detail":"ctest gh_cli_transport, github_cli_process_runner, gh_cli_git_smart_transport, github_provider, github_read_contract, github_paged_read pass (build/rfc0015-r4/focused-ctest.log). BPR-0988's two discovery reads compile to the same closed gh invocation; the one-page search never follows continuation. d3 adapter-removal knockout retained."}},{"criterion":"C4 fixture review and inline publication through ordinary admission, per-write confirmation, exact target, reconciliation and typed terminal; no second POST","outcome":"pass","revision":"c1a336a0","evidence":{"kind":"command","detail":"github_mutation_contract, live_review_mutation_provider (whole binary passes, r3 had 17/18), live_review_submission_provider, both restart recoveries and application_review_round_trip_composition_contract pass; rfc0015_cli_publish_fixture_tests.py 6/6. gh_cli_transport refuses all five GraphQL-rooted shapes before launch (BPR-0896). d4 held-POST course retained; no live PR write."}},{"criterion":"C5 token and private content absent from arguments, environment, logs, settings, SQLite and evidence; no global gh change","outcome":"pass","revision":"c1a336a0","evidence":{"kind":"freeform","detail":"d4 synthetic audit and d5 native structural records retained with their stated limits; gh_cli_transport asserts the exact fresh child environment and stdin-only body for every operation including BPR-0988's reads. r4 ran synthetic fixture gh only; no credential was read or changed."}},{"criterion":"C6 target-native qualification and residual risks; unsupported platforms unavailable; canonical closeout after RFC-BPR-0014 and all evidence","outcome":"pass","revision":"c1a336a0","evidence":{"kind":"observation","detail":"BPR-0787 macOS qualification; application_composition_capabilities_* pass; RFC-BPR-0014 Auto-Verified 2026-09-21. The one open obligation, the review of CLI changes after 26f1c22e, is satisfied by owner amendment r4 (below). Windows/Linux compose no CLI entry; residual executable/config/CA/redirect/account/process risks disclosed, not contained."}},{"criterion":"Owner amendment r4: review obligation","outcome":"pass","revision":"c1a336a0","evidence":{"kind":"freeform","detail":"2026-09-23, product owner in the coordinating chat, verbatim: \"I also approve the BPR-0788 review\". Recorded as an owner decision that satisfies the pending authority/protocol review of the CLI changes after 26f1c22e. It is not an independent reviewer's report; no reviewer findings or identity exist. BPR-0803's device-flow interaction is not accepted by it and stays Backlog."}},{"criterion":"Documentation gate, focused public-path checks and integration check adjudicated","outcome":"pass","revision":"c1a336a0","evidence":{"kind":"command","detail":"verify_documentation.py DOCUMENTATION_OK; integration CTest 10/13 with the three qt_composed_github_scale_* rows failing at the unchanged BPR-0940 assertion; application_composition_contract RFC-0015 assertions pass and its five failing subtests are exactly BPR-0983's recorded findings; architecture_boundary_contract passes (build/rfc0015-r4/integration-ctest.log)."}}]
---

# Verify and close RFC-BPR-0015

RFC base: `6c9ef2038b7389f49028e71f2bb8219b07191267`.

**Digest reseal, 2026-09-17 ([[BPR-0819]]).** The
`application_review_round_trip_composition_contract` pins cited in the d1
checkpoint below went stale at `cdf6175c` and stayed red. BPR-0819 resealed
the whole-file and manifest pins after a per-commit reconciliation. The
RFC-BPR-0015 commits it covers are `cdf6175c`, `df458d30`, `b1f89533`,
`a78eefcf` and `f2b8e682`, plus `c97bb64a`, which had no review until the
integrator's recorded one. The mutation-branch pin never changed.
[[BPR-0821]] owns the two composition-contract assertions that still encode
BPR-0797's retired compile-time gates. Closeout should rerun both contracts.

## Closeout checklist

| Criterion | Direct check | Current result |
|---|---|---|
| The opt-in account row names host, observed login, configured-CLI trust risks and read-versus-publish choice without silently replacing a native profile or claiming exact account containment. | Rendered account row and explicit opt-in exercise | Passed for tests-disabled macOS fixture and installed-`gh` account UI in d5; BPR-0784/0787. r3 at `0f67560e`: tests-disabled Release of `841999ad` shows the CLI-only card naming `octo-reviewer` and the BPR-0933 read question (host, changing gh account, trusted executable/configuration/certificates, read not publish, details disclosed); declining leaves it signed out |
| A tests-disabled macOS application connects through configured `gh` and renders a representative fixture PR via the real public path; missing or changed executable, host, account or config produces a typed refusal without credential fallback. | Tests-disabled Mac composition and refusal fixture | Passed for tests-disabled macOS fixture read in d3 and d5; BPR-0785/0787. r3: same Release rendered `review.cpp` `Diff ready — 1 semantic changes` (11–16 / 21–26); process-failure fixture reads `Could not connect` with no native sign-in; d3 compiled knockout retained |
| CLI reads preserve actual status, headers, body, correlation, bounded pagination, cancellation and redacted errors; removing the process or response adapter makes a direct public-path negative control fail. | Adapter/provider contract and public negative control | Passed for macOS fixture read in d3; BPR-0785. r3: transport, process-runner, Git-bridge and provider contracts pass at `0f67560e` |
| Fixture review and inline-comment publishing traverse ordinary draft and Submit review admission, explicit CLI write confirmation, exact target and observed-account checks, bounded transport, reconciliation and typed terminal projection; a read opt-in or ambiguous write cannot issue a second POST. | Fixture public mutation path and ambiguity control | Passed for macOS fixture in d4; BPR-0786; no live PR write authorized. r3: mutation contract, submission provider and restart recoveries pass; 17/18 mutation-provider rows pass, the red one being BPR-0896's reply reconciliation outside this criterion. r4 at `c1a336a0` (product source as `d8da166e`): `live_review_mutation_provider` passes whole after [[BPR-0896]]; `gh_cli_transport` refuses its new GraphQL-rooted thread read before launch, so the CLI write surface stays review and inline only |
| Token and private response content remain absent from parent arguments, environment, logs, diagnostics, settings, SQLite and committed evidence; the app does not change global `gh` authentication or configuration. | Canary and source/runtime audit | Bounded source, synthetic-log and native structural audits passed in d4/d5; exact Keychain-call and credential-generation binding remain unproved (the criterion does not require them). r3 used synthetic fixture `gh` only |
| Target-native qualification records supported platforms and residual executable/config/CA/redirect/account/process risks; unsupported platforms remain unavailable and canonical closeout awaits RFC-BPR-0014 and all evidence. | Native qualification, docs and RFC index reconciliation | Mac qualification recorded in BPR-0787; Windows/Linux compose no CLI entry (capability rows, Release self-test); RFC-BPR-0014 Auto-Verified since 2026-09-21. r4, 2026-09-23: the product owner approved the pending review in the coordinating chat (an owner decision, not an independent reviewer's report); RFC-BPR-0015 Auto-Verified |

- [x] Every revised RFC criterion has direct command/public-path evidence, producing revision and explicit residual limitation.
- [x] The documentation gate, focused public-path checks, integration check and independent authority/protocol review are adjudicated. (r3: documentation, focused public-path and integration checks adjudicated at `0f67560e`; the d3–d5 independent review does not cover the CLI changes after `26f1c22e`. r4: the product owner approved that review obligation on 2026-09-23, recorded below as an owner amendment and not as an independent reviewer's findings; the focused and integration checks were rerun at `c1a336a0`.)
- [x] RFC-BPR-0014 is Auto-Verified before RFC-BPR-0015 becomes terminal; pending native, human or safe-target evidence remains open. (RFC-BPR-0014 Auto-Verified 2026-09-21; no native, human or safe-target criterion of RFC-0015 remains; live publication stays unauthorized and unclaimed.)
- [x] RFC front matter, index and live cards agree. The historical strict-containment analysis is retained, not claimed passed by opt-in warnings.

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

The d3 production Git-object bridge now composes the exact selected CLI
profile with a macOS write-denied `gh repo clone` for accepted commit objects.
Git inherits an isolated global/system configuration view, `gh` alone invokes
its own credential helper, and a fresh local bare store builds the pack without
network or another helper. The current Git-object qualification accepts only
`gh` 2.100.0 and Apple Git 2.50.1 (Apple Git-155), limits each created file to
32 MiB, and supervises a 96 MiB/128-entry temporary-store abort threshold.
That aggregate threshold is not a hard quota, and snapshot-derived local refs
are not fresh remote advertisements. Four focused adapter/provider/process
CTest cases pass, including a pack through the application v2 parser and
strict Git indexing, cancellation, bounded failures, account drift and
revocation (`build/rfc0015-r2-d3-focused-contracts.log`). Offline config and
synthetic credential probes are retained in ignored
`build/rfc0015-r2-d3-gh-clone-containment.json` and
`build/rfc0015-r2-d3-hostile-git-config.json`; the configured-account
status-only `/user` read under sandbox is in
`build/rfc0015-r2-d3-native-sandbox-viewer.json`. The direct Mac-only bridge
contract cannot run under the repository's outer network sandbox because
macOS refuses nested sandbox initialization; its fake-CLI/local-Git CTest is
registered and labeled as a closed local-only fixture while preserving the
production inner sandbox. This component evidence alone did not establish a
tests-disabled full-diff public render, an adapter-removal negative control,
Keychain backend behavior, or independent critical-review acceptance. The
later d3 public and review evidence below adjudicates the first three items.

The final d3 tests-disabled macOS release app rendered fixture PR
`octo-org/review-repository#17` through ordinary CLI opt-in, observed
`octo-reviewer` on `github.com`, file selection and the Git-object path.
`review.cpp` showed six earlier lines with values 11–16 and six later lines
with values 21–26, with the visible `Diff ready` semantic comparison and
both sides available. A missing `gh` and malformed HTTP framing visibly
refused account connection with Open and Refresh disabled. The positive
binary/source digests, exact UI observations, clean process exits and fixture
limitations are retained in ignored
`build/app-release/rfc0015-evidence/public-path.json` and `ax-excerpts.txt`.
The literal adapter-removal control built a separate tests-disabled app from
`cdf6175c` with only the CLI process factory removed from application
composition. Against the same healthy fixture and fresh storage, Connect
visibly refused CLI availability before identity or PR admission; Open and
Refresh remained disabled and the full-diff assertion could not pass. Its
one-line source delta, binary digest and reaped process are in ignored
`build/app-release/rfc0015-evidence/compiled-knockout.json`. The disposable
build/worktree was removed after the observation. Three fixture harness
tests, the final five-case adapter/provider/Qt CTest pass, documentation gate,
formatting and diff checks passed. No live PR write occurred.

Independent d3 critical review reran the four bridge-focused CTests and
found no blocking defect. It identified a revoke-only outer identity-call
cancellation gap: the selected public invalidation also cancels authority
jobs, so no changed public-path failure is shown; a dedicated wrapper test
remains a follow-up. Official-`gh` local clone probing substitutes file
transport and does not prove real HTTPS credential-helper exchange or the
Keychain backend. The native status-only `/user` probe and source review do
not close that gap. The production temporary-directory monitor's behavior
under real authenticated Git file churn also remains untested. Exact
configured executable, configuration and account races, the supervised
aggregate-store threshold, and snapshot-derived refs remain disclosed.
These limits do not support Windows/Linux or publication claims. Read
BPR-0784/BPR-0785 and all four `opt-in-cli-read` exits are accepted only for
the qualified macOS fixture read surface; BPR-0786/0787, RFC-BPR-0014
predecessor state and any safe-target or human/native evidence remain open.

Integration disposition for `rfc0015-r2/d3`: the first milestone is accepted
at the renewed binding in ignored `build/rfc0015-r2-d3-assignment.json`.
The bridge, fixture, qualification and card finishes were committed as
`2625472b`, `cdf6175c`, `c7269e4d`, `211e82f8`, `a83dd872` and `00dadf70`.
BPR-0784 and BPR-0785 are Done; BPR-0788 remains Backlog. The checked
milestone return is retained in ignored `build/rfc0015-r2-d3-return.json`.
The next bounded milestone is `guarded-cli-publishing` through fixture-only
review and inline-comment paths with per-write confirmation and exact-target
admission. This is not permission for a live PR write or a production release.

Dispatch `d4` under run `rfc0015-r2`, producer
`/root/rfc0015_publish`, mandate `r2`, completed the authored
`guarded-cli-publishing` milestone at source revision `df458d30`. The
tests-disabled macOS app used the same opted-in configured `gh` profile to
draft and confirm a fixture comment review and a right-side source-line
comment on `octo-org/review-repository#17`. Each confirmation named
`github.com`, observed `octo-reviewer`, repository, PR and operation, and
warned that the active CLI account can switch. A Cancel retained the review
draft. A held synthetic `gh api` process showed one POST to each exact REST
endpoint with the body on stdin. Externally advancing the fixture to published
then produced one durable transmitted record and server receipt for each.
The review UI returned to enabled New review; a separate final inline replay
displayed the success-only status `Published PRRC_fixture9002; refreshed 1
comment(s).` The latter observation resolves the independent reviewer's
pending-versus-success projection question. No live GitHub write was made.

The new transport accepts only canonical review/inline mutation shapes,
requires a durable pre-launch transmission witness for POST, checks observed
CLI identity around calls, and leaves an ambiguous attempted write to GET
reconciliation without a second POST. Six focused mutation/provider/public
composition CTests, six fixture tests, documentation gate, focused formatting
and diff checks passed. The independent critical reviewer found no source
blocker and specifically requested the success-only inline label, which the
final replay recorded. The bounded audit found the synthetic body absent
from application and diagnostic logs; request bodies use stdin rather than
arguments, and no token-extraction or global `gh` auth/configuration mutation
was added. The source/binary, UI/POST/recovery, test and negative-control
evidence is indexed in ignored `build/rfc0015-r2-d4-publication-evidence.json`
with logs `build/rfc0015-r2-d4-final-focused-ctest-2.log`,
`build/rfc0015-r2-d4-final-fixture-tests.log`, and
`build/rfc0015-r2-d4-final-documentation.log`. The original review binary
digest was not retained; the final inline replay's tests-disabled digest is
in the evidence record. `BPR-0786` is Done at `a90aaa3d`; the implementation
and fixture were committed at `df458d30`.

The next authored milestone is `native-qualification-and-closeout` through
`BPR-0787` and `BPR-0788`. It must collect target-native macOS read and
publication-fixture evidence, leave Windows/Linux unavailable until their
own gates pass, reconcile the verification ledger against RFC base
`6c9ef2038b7389f49028e71f2bb8219b07191267`, and preserve the configured
CLI trust limits. Official-`gh` HTTPS credential-helper and Keychain exchange
are not proven by the synthetic fixture. No safe live PR target has been
named, so live publication remains unattempted. RFC/index terminal status
also waits for RFC-BPR-0014 Auto-Verified and any required native or human
evidence. The unrelated `.gitignore`, `.mask.json` and BPR-0775/BPR-0776
working-tree changes remain outside this dispatch.

Dispatch `d5` checkpoint under run `rfc0015-r2`, producer
`/root/rfc0015_qualification`, mandate `r2`: BPR-0787 is Done at
`30cd8c7d`. The six revised RFC criteria are reconciled against original
RFC base `6c9ef2038b7389f49028e71f2bb8219b07191267` in
[the verification ledger](../../verification/RFC-BPR-0015.md). The
macOS 26.6.2 arm64 tests-disabled app (binary SHA-256
`61e09d302521dc76509c832909df2765aca1fabbe23eee39683b464c834d52d9`)
rendered the representative fixture PR and semantic diff, showed the
configured-CLI trust and separate-publication warning, and displayed the
installed `gh` account identity in an actual-account UI course. The d4
fixture review and inline-publication POST/receipt/terminal evidence remains
accepted; no live pull request was mutated. A read-only owner-controlled
private HTTPS clone using official `gh` 2.100.0 and Apple Git 2.50.1
recorded `gh auth git-credential get`, exact head, unchanged selected
configuration metadata and executable digest, and removed temporary storage.
The matching Keychain item was checked by metadata only. This supports a
configured-keyring/helper-backed read, not precise Security API calls or
credential-generation binding. The old 2.100.0 redirect probe did not reach
a passing containment result, so executable/configuration/CA/redirect/account
and descendant/PID races remain visible opt-in risks. Windows/Linux remain
unavailable; the locally signed development app is not notarized or an
installable release package. No safe live PR target has been named.

The documentation gate, documentation tests, read/publish fixture tests,
eight focused public/composition CTests, two Git/CLI process CTests and
`git diff --check` passed in d5. A scoped independent critical review found
no blocker, then verified the `git-credential get` evidence precision and
redirect-guidance correction. Ignored evidence remains at
`build/rfc0015-r2-d5-{assignment,frontier,ui-evidence,native-gh-evidence,private-helper-evidence,package-path}.json`,
`build/rfc0015-r2-d5-{focused-ctest,process-ctest,documentation}.log`,
`build/rfc0015-r2-d5-trust-probe.log` and d4 publication records.
`BPR-0788` remains Backlog and RFC-BPR-0015 remains Proposed because
RFC-BPR-0014 is Proposed in the canonical index. Preserve this closeout
criterion pending until that predecessor becomes Auto-Verified and any
remaining required native or human evidence is adjudicated. A command or
local bundle does not supply the missing canonical or distribution evidence.

## Closeout round r3 checkpoint, 2026-09-22

Run `rfc0015-r3`, segment `rfc0015-r3-closeout`, mandate r3 ("implement
available unattended work, focus on closing out rfcs. You can revise and
update the cards and rfcs if they have gone stale."), coordinator-implementer
recipe revision 2, non-delegating leaf. Entry `2218ef1f`; candidate
`0f67560e` (product source unchanged since `841999ad`). Binding: plan
`rfc-bpr-0015-cli-profile`, milestone `native-qualification-and-closeout`,
accepted `opt-in-cli-read` and `guarded-cli-publishing`; ignored
`build/rfc0015-r3/checkpoint-entry.json`.

Done this round: [[BPR-0933]] (short read opt-in, Done), [[BPR-0982]] (self-test
and contract reconciliation after BPR-0909, Done), the missing-process fixture
guard, stale-text reconciliation of the RFC and six architecture/guide
copies, [[BPR-0983]] recorded, [[BPR-0803]] re-observed. Every revised
criterion has current evidence in the
[r3 ledger section](../../verification/RFC-BPR-0015.md#closeout-round-r3-2026-09-22):
native tests-disabled Release course (SHA-256 `4ed0aa15…a532`), integration
pass 22/26 with the four failures classified (BPR-0940 ×3, BPR-0896), the
composition contract's RFC-0015 parts, architecture boundaries on an archive of
the candidate, fixture and documentation tests.

Blocked on one obligation this leaf cannot supply: an independent
authority/protocol review of the CLI changes after the d5 candidate
`26f1c22e` (discovery `1210eecc`/`4efd549c`/`1b73d61f`, routing `1a74b36d`,
shared sign-in `cbd50b74`, CLI Git objects `c3a2ba95`/`60dba14e`/`c97bb64a`/
`37343abf`/`0c66306f`/`eecdda2a`/`12e9bdb8`, thread check `9a2cae53`, this
round's `12d032cf`, and the BPR-0803 device-flow interaction). Next action:
the coordinator arranges that review against the ledger's scope; with no
blocking finding, finish this card against the recorded evidence, set
RFC-BPR-0015 and its index row to Auto-Verified, and record the review.
No attended session, live GitHub write or credential change was made; task
applications exited or were reaped and fixture directories were removed.

## Closeout round r4, 2026-09-23

Run `rfc0015-r4`, segment `rfc0015-r4-closeout`, mandate r4,
coordinator-implementer recipe revision 2, non-delegating leaf. Entry and
evidence revision `c1a336a0` on local `main`. The product source there is
identical to `d8da166e`. Binding: plan `rfc-bpr-0015-cli-profile`, milestone
`native-qualification-and-closeout`, with `opt-in-cli-read` and
`guarded-cli-publishing` already accepted. The binding is in ignored
`build/rfc0015-r4/checkpoint-entry.json`.

**Owner amendment r4: the review obligation is approved.** On 2026-09-23 the
product owner wrote, verbatim, in the chat coordinating this closeout:

> I also approve the BPR-0788 review

This is recorded as an explicit owner amendment that satisfies the one
obligation r3 left open: the authority and protocol review of the CLI changes
after `26f1c22e` that is listed in the
[ledger](../../verification/RFC-BPR-0015.md#pending-independent-authority-and-protocol-review).
It is an owner decision, not an independent reviewer's report. No reviewer
examined those commits for this round, and no findings, dispositions or
reviewer identity exist to record. The approval does not claim that
[[BPR-0803]]'s device-flow interaction was exercised. That interaction stays
BPR-0803's own Backlog work.

**Nothing after the candidate invalidates the evidence.** There are 114
commits from `0f67560e` to `c1a336a0`. Two of them reach the CLI path:

- [[BPR-0896]] (`d8da166e`) adds `observe_review_thread_comments` to the
  GraphQL-rooted shapes that `gh_cli_transport` refuses before launch. This
  narrows the CLI write surface. The CLI transport still admits only the REST
  review and inline-comment shapes of criterion 4.
- [[BPR-0988]] (`8d1898e7`) adds two bounded REST discovery reads: a one-page
  repository issue search, whose continuation is never followed, and
  requested reviewers. It also adds a repository-scoped wire parent. The
  shared request validation admits that parent only for the search of its
  exact repository. `gh_cli_transport` compiles both reads to the same closed
  argument array, fresh environment and neutral working directory as every
  other read. The owner's approval does not name this change, and BPR-0988
  itself records that it was self-reviewed only. This round examined it and
  did not find that it breaks any RFC-0015 criterion. That examination is
  not an independent review.

Checks at `c1a336a0` (logs in ignored `build/rfc0015-r4/`):

- Focused, 16/16 pass (`focused-ctest.log`): `gh_cli_transport`,
  `github_cli_process_runner`, `gh_cli_git_smart_transport`,
  `github_provider`, `github_read_contract`, `github_paged_read`,
  `github_mutation_contract`, `live_review_mutation_provider` (whole, where
  r3 had 17/18 rows), `live_review_submission_provider`, both restart
  recoveries, `application_public_path`,
  `application_public_path_ghes_cli_unavailable`, both
  `application_composition_capabilities_*` rows, and
  `application_review_round_trip_composition_contract`.
- `rfc0015_cli_read_fixture_tests.py` 6/6 and
  `rfc0015_cli_publish_fixture_tests.py` 6/6 pass.
- `application_composition_contract` builds a fresh tests-disabled Release.
  Every RFC-0015 assertion passes: the CLI runner and both consent gates in
  the composition, all six hostile consent sources refused, the macOS CLI
  binary markers, and the Release self-test's `CLI_ENTRY_OFFERED_OK` and
  `CLI_ENTRY_CARDS`. Five subtests fail, and they are exactly
  [[BPR-0983]]'s three recorded findings: three vault-selection paths, the
  updater's two extra readers of the automated-test boundary, and that
  variable's name in the Release binary. The async-mutation inventory test that failed in r3 now passes.
- Integration, 10/13 pass (`integration-ctest.log`):
  `architecture_boundary_contract` (it now passes in the checkout),
  setup destination core/application/public path, device authorization,
  anonymous preview and `application_personal_discovery_public_path`. The
  three `qt_composed_github_scale_*` rows fail at the same pre-existing
  [[BPR-0940]] assertion as r3.
- The documentation gate and documentation tests pass on the closeout
  commit.

Disposition: every revised criterion keeps its r3 evidence. r4 reruns the
automated checks at `c1a336a0` and reuses the r3 tests-disabled UI course.
Since that course's `841999ad` source, the only change to a CLI source is
BPR-0896's refusal in `gh_cli_transport.cpp`. The Release self-test above
confirms the Use GitHub CLI entry again at `c1a336a0`. The owner has approved the review obligation. RFC-BPR-0014
is Auto-Verified, and no native, human or safe-target criterion remains.
RFC-BPR-0015 is Auto-Verified for the qualified macOS scope. Windows and
Linux compose no CLI entry, live publication stays unauthorized and
unclaimed, and the d5 residual trust risks are unchanged. r4 made no
attended session, live GitHub request or credential change. The only
application processes were the test suites' own offscreen runs, and each one
exited.

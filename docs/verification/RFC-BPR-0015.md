# RFC-BPR-0015 verification ledger

RFC baseline: `6c9ef2038b7389f49028e71f2bb8219b07191267`. Current formal status: **Proposed**. The d3–d5 tables below are historical; where they differ, the [r3 closeout round](#closeout-round-r3-2026-09-22) records the current state (RFC-BPR-0014 is Auto-Verified, and Windows and Linux compose no CLI entry rather than a refusing row). The 2026-09-13 product-owner amendment supersedes the historical strict-containment eligibility tests. Those tests remain evidence of configured-CLI trust risks, not guarantees supplied by the new opt-in.

The available production surface is macOS. Native HTTPS remains the default; GitHub CLI reading needs explicit opt-in, and each CLI-backed review or inline-comment write needs a separate exact-target confirmation. Windows and Linux visibly refuse the CLI profile pending their own native credential, process, read/write and UI checks. No live pull request was mutated during this qualification.

## Revised acceptance

| Revised RFC criterion | Direct evidence and producing revision | Result and limit |
|---|---|---|
| Opt-in account row names host, observed login, configured-CLI trust and read versus publish | Tests-disabled Mac account UI and consent in ignored `build/rfc0015-r2-d5-ui-evidence.json`; read implementation `cdf6175c`, first milestone accepted `ad450d7d` | **Passed on Mac.** The fixture displayed `octo-reviewer`; a separate installed-`gh` run displayed `MikeMarcin` on `github.com`. The warning names redirects, configuration, certificates, executable, helper and account-switch risk, and says that read consent does not authorize publication. It does not claim exact account containment. |
| Tests-disabled Mac app renders a representative fixture PR; missing or changed dependencies refuse without native fallback | d3 `build/app-release/rfc0015-evidence/public-path.json` and `compiled-knockout.json`, d5 `build/rfc0015-r2-d5-ui-evidence.json`; source `cdf6175c` | **Passed on Mac fixture.** The d5 release app showed `Pull request ready`, two files, and `Diff ready — 1 semantic changes` for `review.cpp`; earlier values 11–16 and later values 21–26 were visible. The d3 literal process-adapter removal and missing-process/malformed-response courses visibly refused. The fixture's Git objects are local and do not prove a live private PR render. |
| CLI reads retain HTTP status, headers, body, correlation, pagination, cancellation and redacted errors; missing adapter fails public path | d3 focused contracts `build/rfc0015-r2-d3-focused-contracts.log`, compiled knockout above, d3 acceptance `00dadf70` | **Passed for the bounded Mac fixture path.** A command exit alone was not counted as an HTTP or UI result. The direct adapter-removal control failed against the healthy fixture. |
| Review and inline-comment fixture publication use ordinary draft and Submit review, separate confirmation, exact target, transport, reconciliation and typed terminal | d4 `build/rfc0015-r2-d4-publication-evidence.json`, `build/rfc0015-r2-d4-final-focused-ctest-2.log`; implementation `df458d30`, card acceptance `a90aaa3d` | **Passed on Mac fixture.** Held synthetic POSTs were observed at the review and inline endpoints, followed by durable server receipts; the inline UI displayed `Published PRRC_fixture9002; refreshed 1 comment(s).` Read consent alone could not write, and ambiguous attempted writes reconciled without another POST. No live PR write was authorized or attempted. |
| Token and private response content stay out of BPR arguments, environment, logs, diagnostics, settings, SQLite and committed evidence; no global `gh` auth/configuration mutation | d4 source/runtime synthetic audit in `build/rfc0015-r2-d4-publication-evidence.json`; d5 native structural records `build/rfc0015-r2-d5-native-gh-evidence.json` and `build/rfc0015-r2-d5-private-helper-evidence.json` | **Bounded check passed.** BPR passes POST bodies on stdin; the audited fixture body was absent from its application/diagnostic logs. The native probe recorded only structural facts and a hash of a private commit ID. Config-file metadata and executable digest were unchanged around the native clone. This does not prove absence from all memory or bind concurrent replacements. |
| Target-native qualification and residual risks; unsupported targets unavailable; canonical closeout order | d5 native records, this ledger, BPR-0787; RFC-BPR-0014 status in `docs/rfcs/README.md` | **Mac qualification recorded; formal RFC closeout pending.** `gh` 2.100.0 and Apple Git 2.50.1 (Apple Git-155) ran on macOS 26.6.2 arm64. Windows/Linux have no positive claim. RFC-BPR-0014 remains Proposed, so RFC-BPR-0015 cannot become Auto-Verified. |

## Native Mac qualification and exact limits

The selected installed `/opt/homebrew/bin/gh` reported 2.100.0. A sandboxed `gh api user` returned the active `github.com` login `MikeMarcin`; `gh auth status --json hosts` reported `tokenSource=keyring`, and a matching generic-password Keychain item was found by metadata-only lookup. No token value was requested or printed. Under the same global file-write denial plus an owned temporary-store exception, a read-only exact-commit `gh repo clone` of a small owner-controlled private repository completed over HTTPS. Git Trace2 recorded `remote-https` and a `gh auth git-credential get` child start; local Git resolved the requested exact head. The probe observed at most 16,046 bytes and 15 temporary entries, and removed the store. The selected CLI executable digest and `config.yml`/`hosts.yml` metadata were unchanged. The repository name, response body and objects were not retained. See ignored `build/rfc0015-r2-d5-private-helper-evidence.json`.

That course supports one real configured-keyring/helper-backed HTTPS Git read. It does **not** establish the precise Security API calls or credential generation, a held account across separate requests, a hard 96 MiB disk quota, or absence of a process escaping its group. Trace2 and the 50 ms supervisor were probe-only instrumentation. A separate public exact-commit clone succeeded without invoking the helper, showing why public Git success alone was insufficient. The d3 local-file clone and sandbox-migration probes remain distinct, credential-free feasibility evidence.

The earlier official-`gh` 2.95/2.96 trust probe sent an authenticated request through an ambient Unix socket and followed a cross-authority 302. The current 2.100.0 local trust course in ignored `build/rfc0015-r2-d5-trust-probe.log` progressed through socket and certificate setup but failed its old exact-origin authorization assertion at the redirect stage; it gives **no passing 2.100.0 redirect-containment verdict**. Config/socket and CA identity, executable replacement, active-account switching, and descendant/PID reuse therefore remain residual risks. The d3 legacy-config probe showed an unsandboxed first launch can migrate `config.yml` and `hosts.yml`; the same launch under file-write denial refused before an API request and left them unchanged. The production boundary denies all `gh` writes except the owned clone store, but cannot bind an externally changing credential, image, config, CA, redirect destination or escaped descendant to one immutable generation.

The d5 tests-disabled application binary SHA-256 was `61e09d302521dc76509c832909df2765aca1fabbe23eee39683b464c834d52d9`. Its Mac development `.app` passed `codesign --verify --deep --strict`; Gatekeeper assessment exited 3, so this is **not** a notarized or installable package. RFC-BPR-0014 owns package distribution. The fixture and observed actual-account app processes exited or were reaped; the duplicate task-owned alternate-home probe was terminated without supplying UI evidence. See ignored `build/rfc0015-r2-d5-ui-evidence.json` and `build/rfc0015-r2-d5-package-path.json`.

The CLI profile retains the existing RFC-BPR-0013 `MaximumCommits=250` parser ceiling. No evidence here claims `EpicGames/UnrealEngine#1773` renders, or that a safe live review/comment target has been named.

## Closeout round r3, 2026-09-22

Mandate r3 (user): "implement available unattended work, focus on closing out
rfcs. You can revise and update the cards and rfcs if they have gone stale."
Run `rfc0015-r3`, segment `rfc0015-r3-closeout`, coordinator-implementer
recipe revision 2, non-delegating leaf; entry `2218ef1f` on local `main`;
immutable RFC base unchanged. RFC-BPR-0014 became Auto-Verified on 2026-09-21,
which cleared the only blocker d5 recorded. Formal status stays **Proposed**:
the independent authority/protocol review that d3–d5 relied on does not cover
the CLI changes made after the d5 candidate `26f1c22e`, and it cannot be
self-supplied.

### What changed in this round

- [[BPR-0933]] (`12d032cf`): the read opt-in is a headline question and a
  short consequence paragraph, with every residual risk behind Show Details.
  The card-row warning's Git-object claim now names `gh` 2.100.0 or 2.101.0.
- [[BPR-0982]] (`841999ad`): the self-test CLI course, capability probe and
  composition contract follow the discovered Use GitHub CLI entry that
  [[BPR-0909]] introduced. Seven public path rows, both capability rows and
  the contract's CLI checks had been red since 2026-09-18.
- `2028bd56`: the external fixture refuses its `missing-process` course where
  the app's post-BPR-0909 fallback would reach an installed `gh`.
- Documentation (`da336ba0`, `64ec105f` and this round's status sentences): the
  `gh` 2.101.0 qualification, the RFC's stale amendment wording (recorded as a
  reconciliation, no criterion changed) and the closeout state.
- [[BPR-0983]] records unrelated composition contract drift (updater boundary
  readers, moved vault selection); [[BPR-0803]]'s stale premise is corrected.

### Revised acceptance at candidate `0f67560e`

Product source is unchanged from `841999ad` to the candidate.

| Revised RFC criterion | Current direct evidence | Result and limit |
|---|---|---|
| Opt-in account row names host, observed login, trust and read versus publish | Tests-disabled Release of `841999ad` (SHA-256 `4ed0aa15…a532`), external positive fixture, task-owned AX driver: Home → Use GitHub CLI → CLI-only card `GitHub CLI (octo-reviewer, active in gh)` → read question naming `github.com`, the changing gh account, trusted executable/configuration/certificates and that reading does not authorize publishing, with the full risk list behind Show Details. Declining leaves the card Signed out; the card warning on Accounts & hosts is unchanged and passes its Qt assertions. | **Passed on Mac fixture.** The owner-accepted BPR-0909/0915 flow runs the bounded `gh` version and active-account inventory before the question; that is recorded in the RFC reconciliation, not hidden. |
| Tests-disabled Mac app renders a fixture PR; missing or changed dependencies refuse without native fallback | Same course: Connect → `@octo-reviewer` → `octo-org/review-repository#17` → `review.cpp` `Diff ready — 1 semantic changes`, both sides available, earlier 11–16, later 21–26. Process-failure fixture: the question is asked and accepted, the card reads `Could not connect`, the account stays signed out. d3 compiled adapter-removal knockout (`cdf6175c`) and malformed-response course retained. | **Passed on Mac fixture.** `missing-process` was not run: on this station it would reach `/opt/homebrew/bin/gh`, now refused by the fixture. |
| CLI reads keep status, headers, body, correlation, pagination, cancellation and redacted errors; missing adapter fails | `gh_cli_transport`, `github_cli_process_runner`, `gh_cli_git_smart_transport`, `github_provider` pass at the candidate; d3 knockout retained. | **Passed** for the bounded Mac fixture path. |
| Review and inline fixture publication through ordinary admission, per-write confirmation, exact target, reconciliation and typed terminal; no second POST | `github_mutation_contract`, `live_review_submission_provider` and both restart-recovery rows pass; 17 of 18 `live_review_mutation_provider` rows pass one by one, including idempotency, indeterminate attempts, omitted/ambiguous reconciliation and revoked leases. The d4 held-POST fixture course (`df458d30`) is retained; the per-write confirmation and mutation-branch pin are unchanged. | **Passed on Mac fixture.** The red row is [[BPR-0896]]'s reply reconciliation (a failing-first test for an in-progress reply defect on every transport), outside this criterion's review and inline scope. No live PR write. |
| Token and private content absent from arguments, environment, logs, settings, SQLite and evidence; no global `gh` change | d4 synthetic audit and d5 native structural records retained; the discovery inventory never passes `--show-token` and runs under the same write-denied launch; this round's courses used synthetic fixture `gh` only. | **Bounded check passed**, same limits as d4/d5. |
| Target-native qualification, residual risks, unsupported platforms unavailable, canonical order | macOS qualification (d5, r23 at `a9492592`, this round); Windows/Linux compose no CLI entry (`application_composition_capabilities_*` and the tests-disabled Release self-test report the entry absent off macOS and offered on macOS); RFC-BPR-0014 Auto-Verified. | **Met except the review below.** Windows/Linux absence is source/composition evidence from this Mac, not a native observation there. |

### Checks at the candidate

- Integration pass, 26 selected rows (`build/rfc0015-r3/integration-ctest.log`,
  189.8 s): 22 pass. Failing: `qt_composed_github_scale_{100,125,200}` at the
  pre-existing [[BPR-0940]] assertion (identical at the `2218ef1f` baseline;
  the new BPR-0933 dialog test runs before it and passes at all three scales)
  and `live_review_mutation_provider` at BPR-0896's row, as above.
- `application_composition_contract` (fresh tests-disabled Release): all
  RFC-0015 consent-gate and CLI-entry assertions pass; the remaining failures
  are BPR-0983's baseline findings.
- `VerifyArchitectureBoundaries.cmake` passes on a `git archive` of
  `0f67560e`; the checkout run fails only on the stray ignored
  `.claude/worktrees/agent-a17cae88115dbbb4e` copy, as RFC-0014 recorded.
- `rfc0015_cli_read_fixture_tests.py` 6/6, `rfc0015_cli_publish_fixture_tests.py`
  6/6, documentation gate and 64 documentation tests pass.

### Pending: independent authority and protocol review

A reviewer independent of this round needs to adjudicate the CLI credential,
authority and request-boundary changes made after `26f1c22e`, reusing any
record that already covers a commit:

- account discovery and version gates: `1210eecc`, `4efd549c`, `1b73d61f`;
- session routing and shared sign-in: `1a74b36d`, `cbd50b74`;
- CLI Git objects: `c3a2ba95`, `60dba14e`, `c97bb64a`, `37343abf`,
  `0c66306f`, `eecdda2a` (adds `gh` 2.101.0), `12e9bdb8`;
- the write path's thread check `9a2cae53`, and this round's `12d032cf`;
- the unexercised interaction found on [[BPR-0803]]: Use GitHub CLI pressed
  during a device flow hides the live code without cancelling the flow.

A finding that demonstrates a failed criterion blocks closeout; other
findings become follow-up cards. With no blocking finding, BPR-0788 can finish
against the evidence above without repeating it.

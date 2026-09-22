# RFC-BPR-0011 Verification Plan and Ledger

RFC base: `29516d7aae0b61b79571f836c92533459882f5d2`

Status: **the implemented explicit-invocation scope is verified for owner-amended
closeout r4.** BPR-0245 owns the terminal transition after RFC-BPR-0010.
The current macOS fixture path passes from a real workbench gesture through
core policy, exact selected context, durable session admission, a pinned
workspace, the process adapter, typed terminal projection and exact cleanup.

Historical Windows evidence is preserved under BPR-0240/BPR-0476 and the later
BPR-0622/BPR-0826 integration. Linux confirmation is separately recorded.
Those records bind their own revisions, not today's Windows/Linux executable.
BPR-0332's owner choice to document the shipped dispatch was accepted on
2026-09-01 and is no longer pending.

## Owner-amended closeout r4 — 2026-09-21

Assignment: `/root/rfc0011_owner_closeout_retry`, mandate `r4`, sole
engineering writer on `main`. Tested product candidate:
`71b25a2438202411c3c1e5c046e8d4e77ae10070`. RFC baseline:
`29516d7aae0b61b79571f836c92533459882f5d2`.
The [mandate](../rfcs/plans/rfc-bpr-0011-owner-closeout/mandate-r4.md),
[adopted milestone](../rfcs/plans/rfc-bpr-0011-owner-closeout/milestone-plan.json)
and [entry checkpoint](../rfcs/plans/rfc-bpr-0011-owner-closeout/checkpoint-entry.json)
retain the assignment and original twelve-stage source plan.

Owner r4 replaces replay after every HEAD movement with focused current checks
and reuse of unchanged relevant evidence. All nine criteria remain. As the
RFC's verification matrix specifies, independent authentication is assessed
through a local child-identity canary: its synthetic success/refusal changes
without BPR supplying credentials. This is not a live GitHub or installed-agent
result. Human wording sufficiency and current native package qualification
remain separate from implemented behavior.

The targeted development build returned `ninja: no work to do`.
Measured SHA-256 identities:

| Candidate | SHA-256 |
|---|---|
| App | `a0f9c85cd58e4c9d486ae39c554b16f30f4bb73e867ebfbb6c17ad21ca775d90` |
| Public invocation driver | `55b92b981a8d35a19bf02f3c52184af389ec5b412656b9a0a32ecde902abae39` |
| Hostile canary | `c863579add03490d3928516b5997e46ec9292fd91373023e44f19720dfe73e31` |

Current criterion evidence, in the RFC's order:

| Criterion | Evidence | Result and limits |
|---|---|---|
| 1. Four protocols, streaming, cancellation | Launch/loop tests, real canary, composed cancellation and staleness | Pass: partial frames, stderr pressure, ceilings, causal order and typed terminals; descendants stop and provisional output clears. |
| 2. No supplied credentials | Full child argv/environment plus public exact-context course | Pass: safe allowlist and fixed arguments match; parent credential canaries and unchecked prose are absent. This does not sandbox an agent's independent credentials or tools. |
| 3. Independent agent identity | Real child's independently configured synthetic identity | Pass within amended fixture scope: only child configuration changes success/refusal; no live GitHub identity was observed. |
| 4. Corporate host policy | Core profile tests and public forbidden-profile course | Pass: explicit GHES refuses personal Codex profiles before workspace/process effects and retains corporate alternatives. |
| 5. Pinned isolated workspace | Workspace tests, public seed, cleanup retry and workspace diagnostic | Pass: child HEAD equals the session head; origin/alternates/hooks/worktree topology is absent; cleanup preserves substitutes and removes only the restored original. |
| 6. Explicit existing checkout and risk | Workspace/panel tests and attached tooltip inspected in source | Pass for off-by-default per-run intent and unknown-versus-clean distinction. Tooltip: “Let the agent see uncommitted work in your checkout. Off by default; chosen for this run only.” Human sufficiency is unjudged. |
| 7. Exact session and staleness | Core binding, two-process seed/reopen and composed stale terminal | Pass: exact PR/base/head/epoch and invoking-window session match, next ID survives restart, and head movement never rebinds. |
| 8. Context input channel | Launch/canary argv equality and public whole-map schema-v2 stdin equality | Pass: protocol arguments are independent of context; complete accepted input travels over stdin/structured follow-up. |
| 9. Core/host/Qt boundary | Core-only build, host/panel tests, public cuts, linked-host audit and tracked-source scan | Pass on those surfaces; the live-checkout architecture invocation failed on an unrelated nested checkout as recorded below. No new independent review claimed. |

Commands and actual outcomes:

- `cmake --build --preset dev --target app_better_pull_review test_better_pull_review_agent_invocation_composition_public_path test_better_pull_review_qt_agent_launch_canary_tests test_better_pull_review_agent_profile_core_tests test_better_pull_review_agent_session_core_tests test_better_pull_review_agent_context_supplement_core_tests test_better_pull_review_agent_launch_tests test_better_pull_review_agent_protocol_loop_tests test_better_pull_review_agent_workspace_tests test_better_pull_review_agent_invocation_host_tests test_better_pull_review_qt_agent_launch_panel_tests`
  — pass, no work to do.
- `ctest --test-dir build/dev --output-on-failure --no-tests=error -R '^application_agent_invocation_(composition|construction_cut|launcher_construction_cut|gesture_cut)_contract$'`
  — 4/4 pass in 38.10 s. The positive course launches the real child, compares
  full context and reopens durable state; three separate cuts suppress the
  required effects.
- `ctest --test-dir build/dev --output-on-failure --no-tests=error -R '^(core_agent_profile|core_agent_session|core_agent_context_supplement|application_agent_launch|application_agent_protocol_loop|application_agent_workspace|application_agent_invocation_host|qt_agent_launch_panel|qt_agent_launch_canary|application_agent_invocation_(cancellation|staleness|cleanup_retry|workspace_diagnostic)_contract|architecture_boundaries|composed_application_hosts)$'`
  — 14/15 pass in 154.34 s. Canary 71.96 s, cancellation 20.19 s, staleness
  20.21 s, cleanup retry 19.15 s, workspace diagnostic 12.69 s.
  `architecture_boundaries` alone fails because its recursive CMake scan
  includes the existing unrelated
  `.claude/worktrees/agent-a17cae88115dbbb4e` checkout and reports duplicate
  GitHub-adapter ownership. That checkout is preserved.
- `git archive --format=tar --output=/tmp/bpr-rfc0011-architecture.EWAQW3/candidate.tar 71b25a2438202411c3c1e5c046e8d4e77ae10070`,
  extraction to `/tmp/bpr-rfc0011-architecture.EWAQW3/source`, then
  `cmake -DBPR_ROOT=/tmp/bpr-rfc0011-architecture.EWAQW3/source -P /tmp/bpr-rfc0011-architecture.EWAQW3/source/cmake/VerifyArchitectureBoundaries.cmake`
  — pass: application, provider and Qt boundaries verified against exact
  tracked source. Archive SHA-256
  `cdddbae8828577393ce9427daeb158c286083e94b569c462ee768f0e8ce79bee`.
  The temporary export can be regenerated from that revision.
- `cmake --build --preset core-only --target test_better_pull_review_agent_profile_core_tests test_better_pull_review_agent_session_core_tests test_better_pull_review_agent_context_supplement_core_tests test_better_pull_review_agent_protocol_loop_tests`,
  then `ctest --test-dir build/core-only --output-on-failure --no-tests=error -R '^(core_agent_profile|core_agent_session|core_agent_context_supplement|application_agent_protocol_loop)$'`
  — build pass, 4/4 pass in 0.98 s without Qt/provider linkage.

The baseline-to-current inventory maps profile/session/context, host,
workspace, process and panel changes to their completed cards and recorded
reviews, including later BPR-0622/BPR-0826 integration. Current source review
confirms validated allowlist iteration, separate protocol arguments and input,
the public whole-context predicate and each zero-effect construction/gesture
cut. No product, test or package source changes in this milestone. This is
a closeout review, not a new independent security audit; a code simplification
pass would have no changed code to simplify.

Documentation verification: `python3 scripts/verify_documentation.py --root .`
returns `DOCUMENTATION_OK`; `python3 tests/scripts/verify_documentation_tests.py`
passes 64/64 in 2.071 s, including its expected invalid-fixture diagnostics.
`git diff --check` passes. The product, tests, build/package definitions,
historical review packets and original issue plan are byte-identical to the
tested entry revision. The documentation change reconciles acceptance/status
and candidate limits; core state and typed policy, host effect/credential
authority, persistence, request/generation/cancellation and Qt projection have
no implementation impact. Their existing architecture allocation remains
authoritative.

### Preserved native evidence and follow-ups

- BPR-0240 retains the 2026-08-26/27 Windows lifecycle/descendant courses,
  including `8e952928` and the `88a8c063` assignment-order assertion.
  BPR-0476 retains its Windows non-ASCII pass/fail/restored course at
  `b594abed`.
- BPR-0622 supplies later Windows supervisor/recovery evidence at
  `c94e02b1`, merged as `2ba02ddb`.
  [Its native ledger](BPR-0622-windows-native-evidence.md) also retains the
  original composition session-mismatch failure. BPR-0826 fixes that fixture;
  [its Windows rerun](BPR-0826-windows-rerun.md) at
  `559377700d52f37d6c094cd646200c67ab20eeb5` passes seed/reopen in 26.85 s.
  [Linux confirmation](dsp2-linux-confirmation.md) at `780d0a1c` passes all
  eight invocation rows and the ownership/recovery courses. These are native
  process records, not native visual or installed-package acceptance.
- BPR-0332's accepted `run-boundary-decisions-v001` decision selected
  documenting the Windows dispatch. Historical decisions and protected
  candidate bytes remain unchanged.
- BPR-0825 remains Parked under RFC-BPR-0013 for the owner-private descriptor
  ownership decision. Double-kill, handle-rundown, inheritable-pipe and
  modifier-bypass limits remain documented. BPR-0875 remains In Progress under
  RFC-BPR-0013 for its Windows round-trip measurement; the later Linux
  confirmation does not supply that result.
- BPR-0958 through BPR-0961 retain RFC-BPR-0014's current native package,
  authentication, vault and removal qualifications. No current Windows/Linux
  build or installed package was run in this milestone.
- Optional follow-up: evaluate installed Codex/Claude/corporate-harness
  compatibility and independent live authentication against a separately
  authorized target/account. The local canary does not supply that result.
- Optional follow-up: judge the existing-checkout risk wording against a named
  future UI candidate. No human verdict exists. The RFC requires explicit
  selection and disclosure, not an attended wording-sufficiency decision;
  recording this limitation creates no new attendance gate or session.

## Closeout authority and sequencing

RFC-BPR-0001 through RFC-BPR-0010 are now Auto-Verified. BPR-0245 was the
only selected open card at entry, with no unmet dependencies; 26/27 RFC0011
cards were already Done. Their history remains candidate-specific evidence,
not a blanket observation of today's executable.

## What this RFC is, in one sentence

Launch separately installed agent tools using **their own** credentials and
**their own** GitHub access, and contribute only the local review context
GitHub cannot provide.

## The rule everything else serves

**BetterPullReview passes no credential.** Not a model provider's, not AWS's,
and not its own GitHub token.

The convenience that would erode this is obvious and would look helpful: the
agent needs GitHub access, BetterPullReview holds a GitHub token, so pass it
along and save the user a setup step. That would carry the reviewer's own
authority into a process this tool does not audit — and the RFC's own non-goals
say BetterPullReview does not claim to audit every file or network request a
full agent accesses. Handing it authority it cannot then account for is the
worst of both.

Structural expression: the launch request has no field a credential
could occupy, and the child's environment is an allowlist rather than an
inheritance. Because an allowed *name* can indirectly select a secret value,
credential-bearing name classes and case aliases are invalid in core and the
application revalidates before reading any parent value.

## The second rule

**Prompts and context go over stdin or a structured protocol, never argv.**

Command lines are world-readable on every platform this ships to — `ps` on
POSIX, the process list on Windows. A context supplement naming an unreleased
feature, a customer, or a private path would be visible to every other user of
a shared machine, and to anything sampling process lists.

Structural expression: the process request carries arguments and a
separate input channel, and the context has no path into the former.

## Delivered composition and historical native evidence

BPR-0247 composes the portable invocation path from the public authorities
delivered by BPR-0228 through BPR-0230. BPR-0250 now admits sessions durably
before downstream effects and supplies only checked note IDs plus an exact
current draft identity/revision. Its two-process canary proves selected prose on
full child stdin, hostile unchecked prose absent, and immutable session/next-ID
reopen through distinct child-produced phase markers. Its SQLite course validates
retained WAL on an isolated clone before any original-path SQLite open and
compares main/WAL/SHM existence and bytes after every incompatible refusal. A
second exact capture at the preflight-to-open application boundary rejects
deterministic wrong-app/future/tampered namespace replacement, while
identity-bound, known-child-only cleanup sanitizes the private clone and makes
substitution or nonempty cleanup failure explicit.
BPR-0334 has removed the duplicated portable protocol implementation and
proved the shared loop, including complete availability-batch causal gating and
teardown-failure precedence, on the portable test surface. BPR-0240's
target-native Windows evidence has since landed: on 2026-08-26/27 the full
canary and all five composed agent courses ran green on the Windows host,
including the Job Object no-pre-assignment-side-effect and descendant-teardown
courses, with assignment-before-resume pinned by a source-order assertion at
`88a8c063`. The record lives in BPR-0240's dated sections; BPR-0245's closeout
consumes that exact record under r4 without upgrading it to today's native
executable.

That was checked against the RFC text rather than assumed. Its non-goals
exclude calling model APIs and storing provider credentials; its acceptance
criteria name Codex and Claude **fixture processes** and canary argv,
environment, and IPC inspection. No API key, no agent installation, and no
network is required for any criterion.

A smoke test against a really installed Codex or Claude CLI would be extra
validation. The RFC does not ask for it, so it is recorded here as optional
rather than carded as a gate — carding it would put a permanent open item on a
board where every other open item is genuinely blocked.

## Architecture allocation

Restating what `docs/architecture/ai-agent-boundary.md` already says, with the
evidence each region will carry:

| Region | Selected responsibility | Planned evidence |
|---|---|---|
| Functional core | Host-to-provider policy, explicit-invocation legality, selected review scope, context inclusion policy, revision binding, session state, and staleness | Qt-free and provider-free tests under `core-only`, including that no core type has a field a credential could occupy |
| Application host | Executable resolution, process launch, environment allowlist, workspace preparation, stdio transport, context construction, cancellation, session lifecycle | Fixture-process tests inspecting the child's exact argv, environment, and input channel |
| Qt adapter | Explicit launch commands, provider and scope disclosure, streaming presentation, cancellation controls | Adapter tests asserting every launch is an intent and that no widget path holds a credential |

## Claim matrix

A row is claimed only when its card lands with direct evidence.

| Claim | Direct surface | Command or interaction | What will prove it | Result |
|---|---|---|---|---|
| The agent owns authentication | Core types, and a real child process | `core_agent_profile`, `application_agent_launch`, `qt_agent_launch_canary` | No credential appears, and no request type has a field one could occupy | **Verified on the POSIX component surface** (BPR-0172, BPR-0175, BPR-0240). `agent_profile` and `agent_context_supplement` each have a guard that reads the type. The profile validator rejects credential-bearing names and case/alias variants, plus Git askpass, SSH-command, proxy/exec, `GIT_CONFIG*`, and `GCM_*` injection channels; application materialization refuses an invalid profile whole. Safe operational Git names remain accepted. The canary launches a real POSIX child and compares its complete environment against the safe allowlist while the parent holds planted GitHub, Anthropic, and AWS secrets. Changing only the child's independently configured synthetic identity changes success to refusal while argv and context stay equal. **Windows status:** converted 2026-08-26/27 (BPR-0240) — the MSVC-compiled backend passed the full canary target-native, and the credential/argv inspection and independent-identity courses are platform-neutral in that suite, so the planted-secret allowlist comparison and identity-controlled refusal now hold on a real Windows child. BPR-0332's accepted 2026-09-01 decision documents the shipped dispatch. |
| Host policy is enforced | Core | `core_agent_profile` | Refused by policy, and the refusal names which host forbade it | **Verified** (BPR-0172). The verdict carries `RefusedBy` and, where a host requires a profile, `RequiredInstead`. An unmapped host refuses before any other check, so a well-formed profile cannot reach a permissive answer there; making it permit fails the suite. |
| Every invocation is explicit | Qt and shipped composition | `qt_agent_launch_panel`, `application_agent_invocation_composition_contract`, launcher/host construction cuts, gesture cut, `architecture_boundaries` | No launch occurs without a user command; the Qt path emits an intent and launches nothing itself | **Verified on the composed portable fixture path** (BPR-0177, BPR-0247, BPR-0250, BPR-0420). `agent_launch_panel` holds no process and no launcher. Core/context refusal writes no session; durable-admission refusal starts no workspace, environment, executable, or process work. The real selected-diff gesture reaches one admission, workspace, launcher, terminal, and cleanup. A test-only probe delegates to the real portable launcher and proves exactly one `run`, paired with one panel start intent and one host process start. Removing launcher construction keeps the gesture observable but prevents host construction and every downstream effect, so the same exact positive predicate is false. Removing the host or disconnecting the gesture retains its separate zero-effect proof. This is not evidence that the Windows backend compiles or runs. |
| Context never travels in argv | Host, and a real child | `application_agent_launch`, `qt_agent_launch_canary` | The supplement reaches the child over its input channel, and the argv is inspected in full | **Verified** (BPR-0175, BPR-0240). `agent_launch_request` separates `Arguments` from initial/deferred input and `build_agent_launch` has no branch reading the supplement into the former. argv is compared argument for argument — in the request and again from the child — and the complete version-two context is found on standard input. |
| A session is revision-pinned | Core, SQLite, and Qt projection | `core_agent_session`, `sqlite_review_ledger_storage`, `application_agent_invocation_host`, `qt_agent_launch_panel` | The session becomes stale rather than retargeting, and immutable identity plus monotonic next ID survive restart | **Verified** (BPR-0173, BPR-0250). The per-review high-water sequence is independent of bounded 128-row retention and allocates atomically across simultaneous open stores. Reopen returns the exact latest binding and next ID without a write. Current, head-moved, epoch-pair-changed, and different-review projections all retain the stored binding. |
| The supplement carries only local state | Core, host adapter, and shipped canary | `core_agent_context_supplement`, `application_agent_launch`, `qt_agent_launch_panel`, `application_agent_invocation_composition_contract` | It carries every admitted local field GitHub cannot supply, and no provider credential, unselected prose, or presentation coordinate | **Verified for the composed portable scope** (BPR-0174, BPR-0240, BPR-0247, BPR-0250). Schema version two carries exact application/session/profile, PR/revision/epoch, receipt, defect, semantic selection, and explicitly selected private context. Note controls are unchecked and carry IDs only; the draft control carries ID/revision only. Dispatch reloads and revalidates both. The two-process public canary selects one of two notes plus the current draft, compares full child stdin exactly, and requires the hostile unchecked prose and credential canary absent. |
| Supported process protocols stream and fail honestly | Host with real canary children | `ctest --test-dir build/dev -R '^(application_agent_launch\|application_agent_protocol_loop\|qt_agent_launch_canary\|application_agent_invocation_(cancellation\|staleness)_contract)$' --output-on-failure` | Output arrives incrementally without deadlock and each terminal condition has a distinct typed outcome | **Verified on the POSIX portable surface** (BPR-0240, BPR-0247, BPR-0333, BPR-0334). Component canaries cover strict four-protocol framing, pressure, nonzero exit, timeout, cancellation, causal preemption, and descendants. Each of the three declared ceilings is proven by the exact typed cause it declares rather than by the coarse outcome: a flood past `maximum_agent_stdout_bytes` and a flood past `maximum_agent_stderr_bytes` each end in `output_too_large`, distinguished from one another by the channel each leaves empty, and both oversize-frame courses assert `frame_too_large` by value, so collapsing it into `malformed_frame` fails the suite. Removing any one ceiling from the source fails the assertion that names it. The exact cause survives the host's terminal decode (BPR-0356): a refused decoder restates the generic `malformed_frame` on every later call, so the host adopts a trailing cause only when the drain recorded none. Both halves of that ordering are covered — an exchange that stops early is still refused as `incomplete_frame`, and deleting the adoption fails that case. BPR-0334 puts those rules in one loop selected by both platform backends. Its fake transport proves that a later App Server response already present beyond a 16 KiB read boundary remains a causal violation before any follow-up write, and that teardown failure only replaces success while nonzero exit, decoder failure, cancellation, and timeout retain the first cause. The separate 256 KiB stderr course is retained; it proves draining without deadlock, which is a different property from the bound. The shipped-panel courses additionally cancel a live descendant and supersede a live run by accepted-head movement; both clear provisional output, deliver the distinct typed terminal, clean the exact workspace, and become quiescent. The Windows backend compiles under MSVC and converted target-native on 2026-08-26/27 (BPR-0240): `qt_agent_launch_canary` green in 107.54 s with every selected protocol reaching a typed successful terminal, beside the composition (20.52 s), cancellation (16.53 s), staleness (16.64 s), cleanup-retry (16.57 s), and workspace-diagnostic (6.14 s) contracts. The Windows canary includes `test_windows_assignment_failure_has_no_pre_assignment_side_effect` and `test_timeout_terminates_the_descendant_process_tree`, and assignment-before-resume is pinned by a source-order assertion at `88a8c063`. BPR-0332's accepted decision documents the shipped dispatch; the r4 section above binds closeout evidence and its limits. |
| A disposable workspace is pinned | Host and concrete Git cache | `application_agent_workspace`, `git_process_contract`, `application_agent_invocation_composition_contract`, `application_agent_invocation_cleanup_retry_contract` | The workspace matches the session's head, stays outside cache, carries no cache topology, and cleanup cannot retarget | **Verified on the composed POSIX fixture path** (BPR-0176, BPR-0247). The pin is `Binding.HeadOid`, never the checkout's. Concrete materialization uses an owner-private destination outside the retained cache, exact-head pack transfer, detached checkout, and validation of HEAD plus absent origin, alternates, hooks, and linked worktrees. Cleanup carries the original opaque directory identity. The hostile Git contract replaces the deterministic path with a same-marker substitute and proves cleanup preserves both; restoring the original identity then permits exact removal. Parent-root cleanup separately carries its original identity, removes only that exact empty directory non-recursively, and retains substituted or nonempty roots. The public canary proves ordinary terminal cleanup removes both workspace and parent. Its cleanup-retry course refuses a live substitution, retains the exact capability and non-quiescent state, then proves composed shutdown de-duplicates an effects-actor retry after terminal completion and removes the restored workspace plus parent without deleting the substitute. |
| Existing-checkout mode is explicit | Host plus Qt | `application_agent_workspace`, `qt_agent_launch_panel` | The choice is deliberate and the dirty-worktree risk is shown | **Verified** (BPR-0176, BPR-0177). A profile cannot opt itself in: the mode is refused without a choice made for this invocation, carried on the intent rather than read from a setting. `checkout_cleanliness` keeps `unknown` distinct from `clean`, so a host that could not determine the state cannot report the reassuring answer. The toggle's tooltip states the risk; whether that wording is sufficient is a presentation question no attended review has yet judged. |

## Standing composed-host linkage audit

BPR-0248 requires the exact demangled
`bpr::application::agent_invocation_host` constructor in both the
development-fixture and production binaries. This is linkage evidence only;
the construction cut, gesture cut, real child, cancellation, staleness, and
cleanup courses remain the evidence that the shipped panel constructs and
invokes that host.

BPR-0335 adds the exact portable process-launcher and Qt launch-panel
constructors to that standing linkage inventory. BPR-0420 supplies the
launcher's separate runtime complement: a test-only port wrapper records an
actual `run` and delegates it to the real composed launcher, while the positive
course still requires the real panel gesture, host process-start count, and
child canary. Its launcher-construction cut preserves the gesture but removes
launcher and host, leaves downstream effects at zero, and makes the shared
positive predicate false. This remains portable evidence only; it does not
authenticate the Windows backend.

The review-mutation linkage inventory is profile-specific. Both profiles
require the asynchronous application-host and coordinator constructors plus
the generic `bpr::application::reconcile` decision used after mutation.
Development fixture additionally requires the exact test-only provider and
testing-grant factory while forbidding the live provider and factory.
Tests-disabled production requires the live provider and factory while
forbidding the fixture component type and testing-grant factory. Independent
construction, grant, selection, and
host-dispatch cuts are the runtime complements; production's empty grant
performs no live observation or submission. A missing binary, missing
supported demangling chain, nonzero inspector, failed demangler, or malformed or
zero-defined table is a hard failure. Only parsed defined records count;
`U`/`UND` references and anchors embedded in surrounding template text do not.
Windows declaration prefixes are accepted only through the first allowlisted
calling convention outside balanced template, parameter, and array delimiters;
nested conventions do not select the anchor boundary and unbalanced
declarations fail closed. Absolute, common, debug, and unknown symbol kinds or
sections do not count. CTest treats the verifier's exit status as authoritative.

## Cards

| Card | Slice |
|---|---|
| BPR-0172 | Core: agent profiles, host policy, explicit-invocation legality |
| BPR-0173 | Core: session identity, revision binding, staleness |
| BPR-0174 | Core: the context supplement and what it may carry |
| BPR-0175 | Host: process launch, environment allowlist, credential boundary |
| BPR-0176 | Host: workspace preparation, pinned and dirty |
| BPR-0177 | Qt: explicit launch, disclosure, cancellation |
| BPR-0178 | Documentation: extend `docs/architecture/ai-agent-boundary.md` |
| BPR-0179 | Simplification and closeout preparation |
| BPR-0335 | Standing linkage inventory: portable launcher and Qt panel constructors |
| BPR-0420 | Composed runtime: portable launcher construction and real-gesture invocation |
| BPR-0240 | Complete context supplement and process lifecycle semantics |
| BPR-0247 | Portable invocation host, exact-head workspace, panel, and public composition |
| BPR-0250 | Durable bounded sessions and exact private-context selection |
| BPR-0334 | One portable protocol loop behind narrow POSIX and Windows process transports |

## Simplification

One behaviour-preserving removal, and the reasoning for what was *not* removed.

**Removed: the duplicated platform protocol lifecycle.** BPR-0334 replaces
roughly 250 lines of decoder, follow-up, publication, stderr, and terminal
policy copied into the Windows-only translation unit with one private portable
loop. The retained seam is deliberately narrow: native transports only write,
perform bounded reads with availability-batch completion, wait, close input,
and terminate. Keeping those process primitives platform-local avoids a false
abstraction over `QProcess` and Windows `HANDLE`/Job Object ownership.

**Removed: the `sizeof`-sum credential guard.** `core_agent_context_supplement`
first asserted the struct's size against the sum of its members. It was
replaced by a structured binding over every field. The sum had to be
hand-computed with padding — it was wrong on the first attempt — and it fails
for the wrong reason on a layout change while passing for a field swapped with
one of equal size. The binding form requires the exact field count and names
each one.

`core_agent_profile` still uses the sum form. It was left alone deliberately:
it is committed, it was *proven* to fire on a `Token` field, and rewriting a
working guard to match a sibling's style is churn rather than simplification.
The reasoning is recorded here so the next person choosing between the two
forms does not have to rediscover it.

**Not removed: `agent_run_outcome::failed_to_start`.** BPR-0240 reaches it with
a missing executable and keeps it distinct from cancellation. The state is
real — an executable a reviewer configured wrongly — and collapsing it would
give the reviewer the wrong recovery action.

**Not removed: the `external_terminal` protocol.** BPR-0240 now exercises its
versioned stdin envelope, incremental line output, cancellation, timeout, and
recovery against the real canary. The RFC makes it mandatory: a corporate
harness exposing no supported machine-readable interface must still be
launchable, and removing it would push reviewers onto a personal provider
their host forbids.

## Closeout preparation

BPR-0245 has current focused evidence for all nine owner-amended criteria.
The original predecessor blocks are closed. The r4 section above owns the
terminal decision and retains evidence limits and separate follow-ups.

### Not delivered

Named rather than left for a reader to infer from silence:

- **No live or installed agent is run by verification.** The hostile canary
  traverses the shipped profile, policy, panel, host, workspace, launcher, and
  projection path. A smoke test against an installed Codex or Claude harness is
  optional extra validation, not a gate.
- **No installed-agent compatibility smoke.** BPR-0240 exercises documented
  App Server, Codex JSONL, Claude stream-JSON, and external-terminal shapes
  through the RFC's named hostile canary. Installed Codex CLI help and its
  generated App Server schema were inspected for framing; no installed agent
  was run and no live compatibility claim is made. An installed Codex or
  corporate Claude harness remains optional extra validation, not a gate.
  Findings, chat, and MCP helpers remain RFC-BPR-0012 scope.
- **No Windows launch through `QProcess`.** `QProcess` cannot assign a Job
  Object before start, and post-start assignment permits descendant escape, so
  Windows launches only through `windows_agent_process.cpp`: suspended create,
  Job Object assignment, then resume, with the order pinned in source. That is
  the delivered design, not a deferral; its target-native proof is BPR-0240's
  2026-08-26/27 record, and there is no pre-start refusal to document
  (BPR-0332).
- **No agent output/result/private-prose persistence.** The bounded catalog
  stores only immutable session/profile/revision metadata. Stream, terminal,
  findings, chat, and MCP lifecycle persistence remain RFC-BPR-0012 scope.
- **The existing-checkout risk wording is unjudged.** It is shown, and no
  attended review has said whether it is enough.

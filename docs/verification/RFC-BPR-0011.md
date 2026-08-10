# RFC-BPR-0011 Verification Plan and Ledger

RFC base: `29516d7aae0b61b79571f836c92533459882f5d2`

Status: **policy, workspace, panel, context, and process-lifecycle components
exist; product composition remains incomplete.** BPR-0172 through BPR-0179
delivered policy, revision binding, the first context/process contracts,
workspace, and Qt panel components. BPR-0240 completes the version-two context
and four-protocol process lifecycle with hostile POSIX child-process evidence.
Windows launch is deliberately disabled before process start until safe
pre-start descendant containment is implemented and proved target-native. As
the `Not delivered` section states, the public application still constructs
none of them; BPR-0232 owns explicit invocation after its ledger, draft,
defect, and process dependencies.

RFC-BPR-0011 stays `Proposed`. No card here makes a closure claim, and closure
remains ordered behind RFC-BPR-0002.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0002, RFC-BPR-0003, and RFC-BPR-0007. All remain
incomplete and `Proposed`; done BPR-0088 records the product owner's direction
to advance the implementation frontier despite their open work.

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

Planned structural expression: the launch request has no field a credential
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

Planned structural expression: the process request carries arguments and a
separate input channel, and the context has no path into the former.

## What this RFC still needs that it does not have

The public authorities and invocation path tracked by BPR-0228 through
BPR-0232. BPR-0240 supplies the previously missing exact context and
process-lifecycle component behavior.

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

A row is claimed only when its card lands with direct evidence. Every row is
unstarted.

| Claim | Direct surface | Command or interaction | What will prove it | Result |
|---|---|---|---|---|
| The agent owns authentication | Core types, and a real child process | `core_agent_profile`, `application_agent_launch`, `qt_agent_launch_canary` | No credential appears, and no request type has a field one could occupy | **Verified on the POSIX component surface** (BPR-0172, BPR-0175, BPR-0240). `agent_profile` and `agent_context_supplement` each have a guard that reads the type. The profile validator rejects credential-bearing names and case/alias variants, plus Git askpass, SSH-command, proxy/exec, `GIT_CONFIG*`, and `GCM_*` injection channels; application materialization refuses an invalid profile whole. Safe operational Git names remain accepted. The canary launches a real POSIX child and compares its complete environment against the safe allowlist while the parent holds planted GitHub, Anthropic, and AWS secrets. Changing only the child's independently configured synthetic identity changes success to refusal while argv and context stay equal. Windows starts no child. |
| Host policy is enforced | Core | `core_agent_profile` | Refused by policy, and the refusal names which host forbade it | **Verified** (BPR-0172). The verdict carries `RefusedBy` and, where a host requires a profile, `RequiredInstead`. An unmapped host refuses before any other check, so a well-formed profile cannot reach a permissive answer there; making it permit fails the suite. |
| Every invocation is explicit | Qt, structurally | `qt_agent_launch_panel`, `architecture_boundaries` | No launch occurs without a user command; the Qt path emits an intent and launches nothing itself | **Verified structurally** (BPR-0177). `agent_launch_panel` holds no process and no launcher, and the architecture scan enforces that the whole Qt adapter contains no `QProcess` — so a refresh or a navigation cannot reach one because there is none to reach. The refresh-and-navigate interaction the row originally imagined is not run, because there is no composed surface to run it against; see *Not delivered*. |
| Context never travels in argv | Host, and a real child | `application_agent_launch`, `qt_agent_launch_canary` | The supplement reaches the child over its input channel, and the argv is inspected in full | **Verified** (BPR-0175, BPR-0240). `agent_launch_request` separates `Arguments` from initial/deferred input and `build_agent_launch` has no branch reading the supplement into the former. argv is compared argument for argument — in the request and again from the child — and the complete version-two context is found on standard input. |
| A session is revision-pinned | Core | `core_agent_session` | The session becomes stale rather than retargeting, and the original identity is retained | **Verified** (BPR-0173). Nothing in the header rewrites a binding; `continue_against_binding` returns the session's own unconditionally. The epoch pair is part of the identity, asserted after first asserting both bindings agree on every revision. |
| The supplement carries only local state | Core and host adapter | `core_agent_context_supplement`, `application_agent_launch`, `qt_agent_launch_canary` | It carries every RFC-required local field GitHub cannot supply, and no provider credential or token | **Verified as a component** (BPR-0174, BPR-0240). Schema version two carries exact session/PR/revision/epoch identity; upload and last-reviewed heads; complete file receipt and changed state; immutable defect history; semantic selection, segments, groups, mapping confidence, and optional source; and only explicitly selected notes/drafts. Presence differs from empty. Construction is bounded and refuses invalid input whole. The real canary finds every required context class on stdin, while full argv contains none of it. |
| Supported process protocols stream and fail honestly | Host with real canary children | `ctest --test-dir build/dev -R '^(application_agent_launch\|qt_agent_launch_canary)$' --output-on-failure` | Output arrives incrementally without deadlock and each terminal condition has a distinct typed outcome | **Verified on the POSIX component surface** (BPR-0240). The bounded parser rejects malformed JSON, duplicate keys, invalid escapes, and malformed raw UTF-8 while accepting JSON escapes and valid raw Unicode. One-byte fragmentation covers an interleaved App Server id0/id1/id2, correlated delta, and `turn/completed`; each response phase is enabled only after the preceding host followup is completely written. A real hostile child that preloads the whole otherwise-valid App transcript before reading those followups is refused as a causal violation. Codex JSONL lifecycle completion; Claude `system/init`, assistant, and terminal result; and external text retain their separate contracts. Failed/interrupted App statuses remain distinct. The hostile child also covers partial writes, stderr pressure, bounds, nonzero exit, cancel/timeout, and a live descendant. Windows conditionally proves pre-start refusal only; it has no process-lifecycle success claim. |
| A disposable workspace is pinned | Host | `application_agent_workspace` | The workspace matches the session's head and is unaffected | **Verified** (BPR-0176). The pin is `Binding.HeadOid`, never the checkout's. The same workspace is prepared twice with the worktree dirtied in between and the pin does not move. |
| Existing-checkout mode is explicit | Host plus Qt | `application_agent_workspace`, `qt_agent_launch_panel` | The choice is deliberate and the dirty-worktree risk is shown | **Verified** (BPR-0176, BPR-0177). A profile cannot opt itself in: the mode is refused without a choice made for this invocation, carried on the intent rather than read from a setting. `checkout_cleanliness` keeps `unknown` distinct from `clean`, so a host that could not determine the state cannot report the reassuring answer. The toggle's tooltip states the risk; whether that wording is sufficient is a presentation question no attended review has yet judged. |

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
| BPR-0240 | Complete context supplement and process lifecycle semantics |

## Simplification

One behaviour-preserving removal, and the reasoning for what was *not* removed.

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

RFC-BPR-0011 stays **`Proposed`**. No closure claim is made, and closure order
is unchanged: RFC-BPR-0002 still needs real Windows x64 evidence (BPR-0036),
and every later RFC inherits that block.

### Not delivered

Named rather than left for a reader to infer from silence:

- **Nothing composes the agent surface.** `agent_launch_panel` is built,
  tested, and shown by no window; `review_queue_notifier`'s neighbours are
  composed but this is not. `verify_composed_hosts.py` will say so the day it
  is, and until then the honest statement is that a reviewer cannot invoke an
  agent in the product.
- **No agent tool is ever really run.** The canary is the RFC's named surface.
  A smoke test against an installed Codex or Claude harness is optional extra
  validation, not a gate — checked against the RFC text rather than assumed.
- **No installed-agent compatibility smoke.** BPR-0240 exercises documented
  App Server, Codex JSONL, Claude stream-JSON, and external-terminal shapes
  through the RFC's named hostile canary. Installed Codex CLI help and its
  generated App Server schema were inspected for framing; no installed agent
  was run and no live compatibility claim is made. An installed Codex or
  corporate Claude harness remains optional extra validation, not a gate.
  Findings, chat, and MCP helpers remain RFC-BPR-0012 scope.
- **No Windows agent launch.** `QProcess` cannot assign a Job Object before
  start, and post-start assignment permits descendant escape. The adapter
  refuses before `QProcess::start`; a suspended-create/assign/resume design and
  target-native proof remain required before Windows launch can be claimed.
- **No composed result events or session store.** BPR-0232 owns the typed
  product lifecycle; persistence beyond the RFC's pinned session identity is
  not claimed.
- **The existing-checkout risk wording is unjudged.** It is shown, and no
  attended review has said whether it is enough.

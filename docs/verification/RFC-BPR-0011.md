# RFC-BPR-0011 Verification Plan and Ledger

RFC base: `29516d7aae0b61b79571f836c92533459882f5d2`

Status: **components complete; product composition absent.** BPR-0172 through
BPR-0179 delivered and simplified policy, revision binding, context, process,
workspace, and Qt panel components. As the existing `Not delivered` section
states, the public application constructs none of them. BPR-0232 owns the
explicit user-invocation path.

RFC-BPR-0011 stays `Proposed`. No card here makes a closure claim, and closure
remains ordered behind RFC-BPR-0002.

## Why this RFC can be implemented now

`depends_on` names RFC-BPR-0002, RFC-BPR-0003, and RFC-BPR-0007, all
implemented on available targets. Done BPR-0088 records the product owner's
direction to advance the implementation frontier on available targets.

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
inheritance. Nothing to guard, because there is nothing to pass.

## The second rule

**Prompts and context go over stdin or a structured protocol, never argv.**

Command lines are world-readable on every platform this ships to — `ps` on
POSIX, the process list on Windows. A context supplement naming an unreleased
feature, a customer, or a private path would be visible to every other user of
a shared machine, and to anything sampling process lists.

Planned structural expression: the process request carries arguments and a
separate input channel, and the context has no path into the former.

## What this RFC needs that it does not have

Nothing.

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
| The agent owns authentication | Core types, and a real child process | `core_agent_profile`, `application_agent_launch`, `qt_agent_launch_canary` | No credential appears, and no request type has a field one could occupy | **Verified** (BPR-0172, BPR-0175). `agent_profile` and `agent_context_supplement` each have a guard that reads the type; adding a `Token` field fails to compile. The canary launches a real child and reports its own environment, compared name for name against the whole allowlist while the parent holds three planted secrets. Inheriting the system environment in the launcher fails it. |
| Host policy is enforced | Core | `core_agent_profile` | Refused by policy, and the refusal names which host forbade it | **Verified** (BPR-0172). The verdict carries `RefusedBy` and, where a host requires a profile, `RequiredInstead`. An unmapped host refuses before any other check, so a well-formed profile cannot reach a permissive answer there; making it permit fails the suite. |
| Every invocation is explicit | Qt, structurally | `qt_agent_launch_panel`, `architecture_boundaries` | No launch occurs without a user command; the Qt path emits an intent and launches nothing itself | **Verified structurally** (BPR-0177). `agent_launch_panel` holds no process and no launcher, and the architecture scan enforces that the whole Qt adapter contains no `QProcess` — so a refresh or a navigation cannot reach one because there is none to reach. The refresh-and-navigate interaction the row originally imagined is not run, because there is no composed surface to run it against; see *Not delivered*. |
| Context never travels in argv | Host, and a real child | `application_agent_launch`, `qt_agent_launch_canary` | The supplement reaches the child over its input channel, and the argv is inspected in full | **Verified** (BPR-0175). `agent_launch_request` separates `Arguments` from `Input` and `build_agent_launch` has no branch reading the supplement into the former. argv is compared argument for argument — in the request and again from the child — and the note is found on standard input. |
| A session is revision-pinned | Core | `core_agent_session` | The session becomes stale rather than retargeting, and the original identity is retained | **Verified** (BPR-0173). Nothing in the header rewrites a binding; `continue_against_binding` returns the session's own unconditionally. The epoch pair is part of the identity, asserted after first asserting both bindings agree on every revision. |
| The supplement carries only local state | Core | `core_agent_context_supplement` | It carries what GitHub cannot supply, and no provider credential or token | **Verified** (BPR-0174). `Notes` is copied from `SelectedNotes` and from nowhere else — the builder holds no note store — so an unselected note has no source rather than being filtered out. The default case is compared field for field against a fully specified empty supplement. |
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

**Not removed: `agent_run_outcome::failed_to_start`.** Nothing in the suite
reaches it, because the canary always starts. It stays because the state is
real — an executable a reviewer configured wrongly — and deleting it would
force the launcher to report a failure to start as a cancellation, which is a
different thing to a reviewer.

**Not removed: the `external_terminal` protocol.** It carries no arguments and
nothing exercises it end to end. The RFC makes it mandatory: a corporate
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
- **No streaming, findings, chat, or MCP helper.** RFC-BPR-0012 owns those.
- **No result events or session store.** A session records what was asked; what
  came back, and where sessions live, are not this RFC's.
- **The existing-checkout risk wording is unjudged.** It is shown, and no
  attended review has said whether it is enough.

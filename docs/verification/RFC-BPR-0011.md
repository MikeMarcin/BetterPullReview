# RFC-BPR-0011 Verification Plan and Ledger

RFC base: `29516d7aae0b61b79571f836c92533459882f5d2`

Status: planned. BPR-0171 allocates cards BPR-0172 through BPR-0179.

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
| The agent owns authentication | Host, fixture process | Launch and inspect the child's argv and environment | No credential appears, and no request type has a field one could occupy | Unstarted (BPR-0175) |
| Host policy is enforced | Core | Select a forbidden personal provider on a corporate host | Refused by policy, and the refusal names which host forbade it | Unstarted (BPR-0172) |
| Every invocation is explicit | Core plus Qt | Refresh, navigate, and receive new activity | No launch occurs without a user command; the Qt path emits an intent and launches nothing itself | Unstarted (BPR-0172, BPR-0177) |
| Context never travels in argv | Host | Launch with a context supplement | The supplement reaches the child over its input channel, and the argv is inspected in full | Unstarted (BPR-0175) |
| A session is revision-pinned | Core | Move the head under a live session | The session becomes stale rather than retargeting, and the original identity is retained | Unstarted (BPR-0173) |
| The supplement carries only local state | Core | Build a supplement for a review | It carries what GitHub cannot supply, and no provider credential or token | Unstarted (BPR-0174) |
| A disposable workspace is pinned | Host | Prepare a workspace and dirty the active worktree | The workspace matches the session's head and is unaffected | Unstarted (BPR-0176) |
| Existing-checkout mode is explicit | Host plus Qt | Choose an existing checkout with uncommitted changes | The choice is deliberate and the dirty-worktree risk is shown | Unstarted (BPR-0176, BPR-0177) |

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

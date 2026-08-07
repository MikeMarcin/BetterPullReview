# AI Agent Boundary

## Architectural allocation

- The core owns explicit-invocation legality, Git-host/provider policy, selected context scope, revision binding, staleness, and finding-promotion decisions.
- The host owns executable/workspace/process/session/protocol lifecycle, environment policy, context-supplement construction, cancellation, and typed result events.
- The Qt adapter owns visible provider/scope disclosure, explicit commands, approvals, streaming chat/findings, and error presentation.

## Principle

Codex and the approved Claude/Bedrock harness are separate trusted applications. They use their own authentication, GitHub access, sandbox, tools, and approval policies exactly as they would when launched directly. BetterPullReview does not proxy model traffic or provide agent credentials.

## Provider profiles

A profile declares executable, protocol, supported capabilities, workspace policy, environment allowlist, host policy, and persistence behavior.

Initial protocols:

```text
codex_app_server
codex_jsonl_exec
claude_stream_json
external_terminal
```

The external-terminal fallback is mandatory when a corporate harness exposes no supported machine-readable interface.

## Host policy

Git hosts map to allowed or required agent profiles. A corporate GHES profile can require the corporate Claude harness and forbid a personal provider. Unknown hosts have AI disabled until explicitly mapped.

## Workspaces

- Existing checkout: deliberate user selection for repository-wide context.
- Disposable review workspace: default for bounded AI review, pinned to the exact review head and isolated from the active worktree.

## Context supplement

The agent retrieves PR data through its own GitHub tools. BetterPullReview supplies only local state not represented by GitHub, including selected epochs, last-reviewed head, changed-since-review files, local defect state, current selection, and explicitly included private notes or drafts.

Every supplement includes host, repository, PR, base OID, head OID, selected epoch pair, and schema version.

## Session semantics

- Every invocation is an explicit user action.
- Sessions are pinned to immutable OIDs.
- Head movement marks results stale; it does not silently retarget the session.
- BetterPullReview can record what supplemental context it sent, but it cannot claim to audit every item the full agent independently accessed.
- Structured AI output is a private proposal until a user explicitly promotes or publishes it.
- A full agent may use its own GitHub tools subject to its normal approvals; BetterPullReview learns resulting shared changes through normal synchronization.

## What implementation settled

The sections above were written before any of this existed. They state the
allocation; what follows is what building it decided, and what each claim can
be checked against.

### The credential rule is a shape, not a promise

`core::agent_profile` has no field a credential could occupy — no token, no
key, no generic string map. `AllowedEnvironment` is a list of variable
**names**; a map would give every name a value, and a value is where a secret
goes.

`core_agent_profile` asserts this by reading the type: each field by name, and
then the struct's total size against the sum of those fields. Adding a `Token`
member fails to compile with a message asking why it cannot hold a credential.
A test that built a profile and searched it for a secret would pass for every
profile nobody thought to put one in.

The same guard, as a structured binding over every field, covers
`core::agent_context_supplement` in `core_agent_context_supplement`. The
binding form is the better one: a `sizeof` sum has to be hand-computed with
padding, fails for the wrong reason on a layout change, and passes for a field
swapped with one of equal size.

### The environment is an allowlist because of how it is built

`application::materialize_agent_environment` iterates the **allowlist** and
asks the parent for each name. It never enumerates the parent, so a variable
the profile did not name cannot be reached and therefore cannot be copied.

A function that started from the parent's environment and removed
known-sensitive names would be a denylist wearing an allowlist's name, and
would pass every secret nobody had thought to list.

Checked twice. `application_agent_launch` compares the request's environment
against the whole allowlist, name for name. `qt_agent_launch_canary` launches
a real child that reports its own environment, and compares **that** against
the allowlist — because the request can carry the boundary perfectly and the
call can lose it: `QProcess` hands a child a copy of the parent's environment
unless told otherwise, and `systemEnvironment()` is the convenient call. One
word changes it, and only the canary notices.

Both tests plant three secrets of different shapes and assert the parent holds
each at the moment the child runs. Grepping for one name would pass against a
child that inherited another.

### Nothing sensitive travels in argv, and the reason

Command lines are world-readable: `ps` on POSIX, the process list on Windows. A
context supplement naming an unreleased feature, a customer, or a private path
would be visible to every other user of a shared machine.

`agent_launch_request` separates `Arguments` from `Input`, and
`build_agent_launch` has no branch that reads the supplement into the former.
`Arguments` carries the profile's protocol switch and nothing else, asserted
argument for argument rather than searched for the note — in the request, and
again from the child itself.

### Private notes travel only where selected

`core::build_agent_supplement` copies `Notes` from `SelectedNotes` and from
nowhere else. It holds no note store, so a request that selected none produces
a supplement with none **because there was no source**, rather than because a
filter removed them.

A redaction step would be worse in the way that matters: it is a place a
mistake can live, and the mistake is silent. The supplement looks right to
whoever wrote the code and wrong only to the reviewer whose unfinished thinking
is now in a tool they meant to ask about a diff.

### Process ownership is enumerated

`QProcess` and `QProcessEnvironment` have an ownership allowlist in
`cmake/VerifyArchitectureBoundaries.cmake`. It named two files; RFC-BPR-0011
adds `src/adapters/agent/src/agent_process_launcher.cpp` as the third, and the
scan checks that the adapter's own CMake target owns it.

The launcher is **not** in `src/adapters/qt`. That whole directory is an
effect-free region, because presentation starts no processes — and the scan
refused the first attempt to put it there. `qt::agent_launch_panel` holds no
process and no launcher, so a refresh, a navigation, or new activity arriving
cannot reach one.

### A session answers about what it was asked

`core::observe_agent_session` compares and reports; nothing rewrites a
binding. Head movement makes a session stale and never retargets it, and
`continue_against_binding` returns the session's own binding unconditionally —
a named function rather than a field access, so continuing against an old
revision is a call a reader can find.

The epoch pair is part of the identity. An agent asked "what changed since the
last review" and one asked "what changed in the whole pull request" were asked
different questions even where the head is identical.

### The dangerous workspace mode needs a choice, not a setting

`application::prepare_agent_workspace` refuses `existing_checkout` without an
explicit choice for **this invocation**. A profile cannot opt itself into the
reviewer's uncommitted work. `checkout_cleanliness` has three states because
"not asked" is not "clean": a host that could not determine the state must say
so rather than pick the reassuring answer.

### Where to check each claim

| Claim | Test |
|---|---|
| No profile type can carry a credential | `core_agent_profile` |
| An unmapped host permits nothing | `core_agent_profile` |
| A required profile excludes the merely allowed | `core_agent_profile` |
| Head movement makes a session stale, never retargets | `core_agent_session` |
| The epoch pair is part of session identity | `core_agent_session` |
| No supplement field can carry a credential or an unselected note | `core_agent_context_supplement` |
| An oversize supplement is refused whole | `core_agent_context_supplement` |
| The request's environment is exactly the allowlist | `application_agent_launch` |
| The supplement never reaches argv | `application_agent_launch` |
| A real child receives only the allowlist | `qt_agent_launch_canary` |
| A real child's argv carries no context | `qt_agent_launch_canary` |
| A cancelled run publishes nothing | `qt_agent_launch_canary` |
| Existing-checkout mode needs a choice | `application_agent_workspace` |
| Dirty state is reported, unknown stays unknown | `application_agent_workspace` |
| The launch surface only emits intents | `qt_agent_launch_panel` |
| Scope is disclosed before launch is possible | `qt_agent_launch_panel` |
| Process ownership is enumerated | `architecture_boundaries` |

### What is not claimed

BetterPullReview records what supplemental context it sent. It **cannot** claim
to audit every item the full agent independently accessed — the agent has its
own GitHub access and its own tools, which is the point of the arrangement.

Nothing composes the launch surface yet: the panel is built and tested and no
window shows it. The real agent tools are not exercised; the canary is the
named surface, and a smoke test against an installed Codex or Claude is
optional extra validation rather than a gate.

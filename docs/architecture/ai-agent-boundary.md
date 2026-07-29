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

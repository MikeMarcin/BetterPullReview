---
id: RFC-BPR-0011
title: AI Agent Profiles, Launch, Authentication Isolation, and Context Supplements
status: Proposed
area: BPR
milestone: M5 Explicit AI Assistance
depends_on: [RFC-BPR-0002, RFC-BPR-0003, RFC-BPR-0007]
pack_version: 2
---

# RFC-BPR-0011: AI Agent Profiles, Launch, Authentication Isolation, and Context Supplements

## Summary

Launch separately installed Codex and approved Claude/Bedrock agent flows using their own credentials and GitHub access, while adding revision-pinned BetterPullReview-only context.


## Motivation

The user already launches Codex at home and a corporate Claude/Bedrock harness at work to inspect PRs through their own GitHub access. BetterPullReview should embed that existing trusted flow and contribute only local review context GitHub cannot provide.

## Goals

- Add configurable agent profiles and process protocols.
- Support Codex App Server, Codex JSONL execution, Claude stream-JSON, and external-terminal fallback.
- Preserve each agent's own model, GitHub, AWS, sandbox, approval, and credential boundaries.
- Bind Git hosts to allowed/required agent profiles.
- Launch in an existing checkout or disposable revision-pinned workspace.
- Build a versioned local context supplement.
- Make every invocation explicit and revision-pinned.

## Non-goals

- Calling model APIs directly.
- Storing OpenAI, Anthropic, AWS, Bedrock, or agent GitHub credentials.
- Claiming BetterPullReview audits every file or network request a full agent accesses.
- Automatically running an agent after refresh or new activity.

## Architectural allocation

- **Core:** owns host-to-provider policy, explicit-invocation legality, selected review scope, context inclusion policy, revision binding, AI session state, and staleness decisions. It receives no provider or GitHub credential.
- **Host:** owns executable resolution, process launch, environment allowlists, workspace preparation, stdio/protocol transport, context-supplement construction from approved observations, cancellation, session lifecycle, and agent-result events.
- **Qt adapter:** owns explicit launch commands, provider/scope disclosure, approval prompts/cards, streaming presentation, cancellation controls, and error presentation.
- Agents retain their own authentication and authority. BetterPullReview supplies only the selected local context through the host request path.

## Agent profile

Profiles include display name, executable, protocol, arguments, environment allowlist, workspace policy, supported capabilities, host policy, and persistence policy. Executable paths may be administratively locked and hash-checked.

## Context supplement

The supplement includes PR URL, expected base/head OIDs, selected epochs, previous upload, last-reviewed head, changed/unread/accepted file state, local defect state, current selection, and only explicitly included private notes or drafts.

## Acceptance criteria

- Codex and corporate Claude fixture processes launch through their selected protocol with streamed output and cancellation.
- BetterPullReview passes no provider credential or its own GitHub credential.
- Agent GitHub access succeeds or fails according to the agent's independently configured identity.
- Corporate GHES host policy blocks a forbidden personal provider even if selected manually.
- Disposable workspace is pinned to expected head and isolated from active worktree changes.
- Existing-checkout mode is explicit and shows dirty-worktree risk.
- Every session records exact host/repository/PR/base/head/epoch identity and becomes stale after head movement.
- Prompts/context are sent over stdin or structured protocol, not command-line arguments.
- Provider-policy and staleness decisions are core-tested without agent processes/Qt; host process-boundary tests and Qt explicit-invocation tests prove no credential or authority bypass.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Agent owns authentication | Canary launch integration | Inspect parent/child argv/env/IPC and verify no supplied credential |
| Host policy is enforced | Real configuration path | Attempt forbidden profile on corporate host |
| Workspace is revision-pinned | Disposable workspace | Compare HEAD and uncommitted state to session manifest |
| Context supplement is exact | Session trace | Compare supplied local fields/OIDs to workbench projection |
| AI authority remains host-bounded | Policy/process/Qt tests | Core decides policy, host launches, Qt presents; no credential crosses regions |

## Documentation impact

Profile schema, supported protocols, host policy, credential boundary, workspace modes, context schema, process environment, cancellation, staleness, and corporate harness fallback are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.

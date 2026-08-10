---
id: BPR-0232
title: Compose explicit agent invocation into the shipped workbench
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0226, BPR-0228, BPR-0229, BPR-0230, BPR-0240, BPR-0247, BPR-0250]
---

# Compose explicit agent invocation into the shipped workbench

The policy, partial context, process, workspace, and Qt panel components exist.
The application constructs none of them, so a reviewer cannot invoke an agent.
BPR-0240 first completes the missing exact-context and process-lifecycle
semantics; the ledger, draft, and defect dependencies then supply their real
context authorities.

## Scheduling and design checkpoint

The product owner deferred target-native Linux and Windows environments until
the end of the remaining portable work. Commit `8886017` supplies BPR-0240's
portable version-two context and four-protocol lifecycle; its Windows launch
claim remains deliberately absent. BPR-0232 may therefore implement and verify
the available-target composition through BPR-0247 now, but this umbrella card
remains Backlog and cannot become terminal before both BPR-0240 and BPR-0247 do.

The composition must not mistake the existing descriptive workspace record for
a checkout. The retained cache actor will materialize one exact accepted head
into an owner-private destination outside the private cache, detach and verify
that head, remove remote/alternate/hook authority, and clean up only that owned
workspace after terminal delivery. The accepted directory then becomes the
child's explicit working directory. Existing-checkout mode remains a separate
per-invocation opt-in with visible dirty/unknown risk.

The first shipped profile source is a small immutable set, not eager executable
discovery or a credential-bearing generic map: Codex App Server, Codex JSONL,
the approved Claude stream-JSON profile, and a documented external-terminal
harness. GitHub.com may offer all four; explicit GHES policy forbids the Codex
profiles and permits only the corporate/external alternatives. Executable
resolution, workspace creation, and environment materialization begin only
after the explicit gesture and core policy verdict. A missing locally
configured agent remains a typed failed-to-start result.

## Required final state

- The shipped workbench presents the explicit invocation surface and the
  selected profile, scope, revision binding, workspace mode, and dirty-worktree
  risk.
- A user gesture enters core legality and host policy before any process is
  resolved or launched.
- The host constructs the exact stdin/structured context, environment
  allowlist, and revision-pinned workspace; no BetterPullReview credential can
  cross the boundary.
- Streaming/result lifecycle, cancellation, staleness, and shutdown are
  presented through typed events rather than direct widget/process coupling.
- A public canary trace proves the composed surface while retaining the RFC's
  optional status for a smoke run against an actually installed agent.
- Extend the standing composition inventory to the launcher and panel, then
  prove construction and invocation separately from their linked symbols.

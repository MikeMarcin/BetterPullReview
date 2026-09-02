---
id: BPR-0501
title: "Host: read-only MCP context helper over stdio"
status: In Progress
rfc: RFC-BPR-0012
depends_on: [BPR-0496]
---

# Host: read-only MCP context helper over stdio

A first-party helper executable speaks MCP over stdio and exposes only an
enumerated set of read-only tools over the frozen review projection an
`agent_session`'s `agent_revision_binding` pins. It has no network listener,
no credential, no mutation tool, and no filesystem root beyond the frozen
snapshot. Its evidence is a canary modeled on
`tests/fixtures/agent/agent_canary_main.cpp`, which enumerates the tools and
then attempts every forbidden network, filesystem, and write access.

## Required final state

- The helper is its own executable target under a new `src/adapters/mcp/`.
  Recorded reason: `src/adapters/agent/` is a narrow launch port for a process
  BetterPullReview does not implement, while this is a program it ships that
  serves its own projection outward. The implementing slice confirms or
  overturns that placement in the ledger, with the reason either way.
- The tool list is a closed source-level enumeration. There is no dynamic,
  configured, or plugin registration, so adding a tool is a reviewable diff.
- No credential is reachable: the environment is an allowlist as the launch
  path's is, and no type has a field a token could occupy.
- No network listener and no outbound socket, and no filesystem root beyond
  the snapshot directory; a path outside it is refused, symlinks included.
- No mutation operation exists — no write, no draft, no publish, no note
  store, not even a disabled one.
- It serves one snapshot pinned to one `agent_revision_binding`; head movement
  makes it stale rather than silently re-reading.

## Non-goals

- Findings, chat, workshop, and promotion — BPR-0497 through BPR-0503.
- Constructing or supervising the helper in the shipped application — BPR-0504.
- Packaging or distributing the helper to external agents — outside this RFC.

## Required direct evidence

- A new `application_mcp_context_helper` registered with
  `bpr_add_network_isolated_test`, run with `ctest --preset dev -R
  '^application_mcp_context_helper$' --output-on-failure`.
- The canary enumerates the advertised tools and compares the list exactly
  against the source enumeration; adding a tool without updating the list
  fails.
- A sweep over every entry in the refusal table — outbound connect, listener
  bind, read above the snapshot root, symlink escape, and each write, draft,
  publish, and note tool name — asserts a refusal for each, not a sample, and
  each refusal is shown to fail when its check is removed.
- The helper's complete child environment is compared against the allowlist
  while the parent holds planted GitHub, Anthropic, and AWS secrets, as
  `qt_agent_launch_canary` already does.
- If any socket surface survives, the same change declares
  `LOOPBACK_PORT_COUNT` and runs `test_network_isolation_manifest`; no ordinary
  selector matches that registration, and [[BPR-0452]] sat red for a day on it.

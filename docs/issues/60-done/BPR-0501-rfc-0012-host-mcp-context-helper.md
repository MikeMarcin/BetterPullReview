---
id: BPR-0501
title: "Host: read-only MCP context helper over stdio"
status: Done
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

## What this slice delivered — 2026-09-02

| Claim | Command | Result | Commit |
|---|---|---|---|
| The helper is its own executable target under a new `src/adapters/mcp/`, linking `BetterPullReview::Core` and nothing else | `cmake --build --preset dev` | Placement **confirmed**. `tool_better_pull_review_mcp_context_helper` builds from `src/adapters/mcp/`; the library `BetterPullReview::AdapterMcp` links Core only — no application host, no Qt, no GitHub/Git/SQLite adapter. Reason recorded in the ledger and in `src/adapters/mcp/CMakeLists.txt` | this commit |
| The tool list is a closed source-level enumeration, and the advertised list equals it exactly | `ctest --preset dev -R '^application_mcp_context_helper$' --output-on-failure` | Passed 0.45 s. Three independently sourced lists agree: the `enum class mcp_tool` text read out of the header, the compiled `mcp_tools` array, and the names returned by a real `tools/list` over stdio. A fourth list written in the canary means a tool added to all three still fails until a person edits the test | this commit |
| No credential is reachable: the environment is an allowlist as the launch path's is | same selector | The parent held `GITHUB_TOKEN`, `ANTHROPIC_API_KEY`, and `AWS_SECRET_ACCESS_KEY`; the child received all three and retained exactly `BPR_MCP_SNAPSHOT_ROOT` and `BPR_MCP_PINNED_BINDING`. No secret value appears on either child channel; all three still exist in the parent | this commit |
| No network listener and no outbound socket | `nm -u build/dev/src/adapters/mcp/better_pull_review_mcp_context_helper` | No socket, connect, bind, listen, accept, or resolver import among the 84 undefined symbols. `LOOPBACK_PORT_COUNT` therefore stays 0 and `test_network_isolation_manifest` is not required | this commit |
| No filesystem root beyond the snapshot; a path outside it is refused, symlinks included | same selector | `weakly_canonical` plus a component-wise containment test refuses `../../../etc/passwd`, `/etc/passwd`, `../binding`, and a real directory symlink out of the snapshot, for both path-taking tools | this commit |
| No mutation operation exists — not even a disabled one | same selector | Ten write, draft, publish, note, and network names are refused `unknown_tool`, and the canary asserts none of them is a declared descriptor | this commit |
| One snapshot pinned to one `agent_revision_binding`; head movement is stale | same selector | With the pin unchanged and the snapshot's binding record moved to a new head, every `tools/call` answers `revision_binding_stale` and the transcript contains neither the frozen bytes nor the frozen paths | this commit |
| Every entry of the refusal table fails when its own check is removed | `python3` mutation sweep, 19 mutations, rebuild and re-run each | 19/19 `FAILED-AS-REQUIRED`; every restore byte-exact by md5; the restored tree builds and passes. Table in the slice report | this commit |
| Nothing in RFC-BPR-0012's landed surface regressed | `ctest --preset dev -R '^(core_agent_finding\|core_agent_finding_promotion\|core_agent_chat_scope\|application_agent_finding_protocol\|application_agent_launch\|qt_agent_launch_canary)$'` | 6/6 passed, 65.97 s | this commit |
| The new adapter directory and the new automated executable break no boundary | `ctest --preset dev -R '^(architecture_boundaries\|architecture_boundary_contract\|live_mutation_isolation\|core_boundary_contract)$'` | 4/4 passed, 95.1 s | this commit |

## Integrator reproduction — 2026-09-02

Reconfigured and rebuilt `build/dev` (both exit 0), then
`ctest --preset dev -R '^(application_mcp_context_helper\|core_agent_chat_scope\|core_agent_finding\|core_agent_finding_promotion\|application_agent_finding_protocol\|application_agent_launch\|live_mutation_isolation\|architecture_boundary_contract)$' --output-on-failure`:
8/8, with `application_mcp_context_helper` at 0.10 s, `live_mutation_isolation`
17.26 s, and `architecture_boundary_contract` 66.30 s. The placement decision
(`src/adapters/mcp/`, core-only link, `tool_` prefix so the live-mutation
inventory walks it) stands; the root agent guide now lists the sixth adapter
and the RFC-BPR-0012 ledger's status paragraph names the landed slices.

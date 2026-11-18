---
schema: "implement-rfc.card.v1"
id: "BPR-2257"
title: "Agent host policies exist only for host IDs 1 and 2, so an Enterprise server given another host ID refuses every agent for good"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2257--BPR-2257--5648cc8479b2.json","origin":"BPR-2256's Out of scope asked whether it can happen; the coordinator asked for a card if it can (2026-11-18)","related_cards":["BPR-1749","BPR-2256"],"related_rfcs":["RFC-BPR-0092"]}
---

# BPR-2257: Agent host policies exist only for host IDs 1 and 2, so an Enterprise server given another host ID refuses every agent for good

## Finding

It can happen. Read at `963ef198b`; not reproduced in a running app.

1. The window states its agent policies once, by host ID:
   `built_in_agent_policy(git_host_id{ 1U }, github_com)` and
   `built_in_agent_policy(git_host_id{ 2U }, explicit_ghes)`
   (`src/app/src/github_application/window_construction.cpp`,
   `agentPolicies`, given to the launch host, the chat host, quick asks and
   agent setup). `core::policy_for_host` finds a policy only by that ID; any
   other ID gets none, and `core::decide_agent_invocation` then refuses every
   profile with `host_not_mapped` ("An unmapped host permits nothing at
   all").
2. Host IDs are not fixed at 1 and 2. They are allocated as the next free ID:
   - A server added in the account setup gets
     `max(LastHostId + 1, every profile's host + 1)` unless a profile already
     has its hostname (`window_sign_in.cpp`, the `SavedServers` loop, around
     line 766).
   - A `gh` CLI account on a hostname no profile has gets the same
     (`window_sign_in.cpp`, around line 421).
   - At start, a profile with no remembered row is renumbered with
     `++nextHost` once the remembered connections hold a `LastHostId`
     (`profile_runtime_construction.cpp`, around line 494).
3. So with github.com at host 1, the first Enterprise server added gets 2
   and is mapped (the owner's case in [[BPR-2256]]). A second Enterprise
   server gets 3. So does a `gh` account on another Enterprise hostname while
   a native Enterprise profile holds 2. Every agent on that server is then
   refused for good: setup shows "Agents aren't allowed for this pull
   request's server.", and the launch, chat and quick-ask hosts refuse too.
4. The reverse also holds: whichever server holds ID 2 gets the Enterprise
   policy and ID 1 the github.com one, by number, not by what the server is.
   Restored saved servers at start are all given host ID 2
   (`profile_runtime_construction.cpp`, around line 478).

## Direction (not decided)

Derive each host's policy from its profile's server contract (github.com
or GHES), for every profile the window composes, rather than from two fixed
IDs. Keep `decide_agent_invocation`'s rule that a host with no policy permits
nothing.

## Acceptance

- AC1. Every host the window can review has the built-in policy of its kind,
  whatever ID it was allocated.
- AC2. A test composes two Enterprise servers (or an Enterprise `gh`
  account beside a native Enterprise profile) and shows each one's agents
  decided by the Enterprise policy.
- AC3. A host with no policy still permits nothing.

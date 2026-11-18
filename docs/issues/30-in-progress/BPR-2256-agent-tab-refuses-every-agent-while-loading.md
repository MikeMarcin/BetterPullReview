---
schema: "implement-rfc.card.v1"
id: "BPR-2256"
title: "While a pull request loads, the Agent tab says every agent is not allowed for its host, because setup has no host policy until the comparison is accepted"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","reservation":"docs/issues/_reservations/BPR-2256--BPR-2256--d6788db6a954.json","found_by":"owner, 2026-11-18, build 68: on opening a pull request on a GitHub Enterprise Server host, the Agent tab listed the custom agent, Claude Code and Codex, each marked 'Not allowed for this pull request's host.' Left alone, the tab corrected itself once the pull request finished loading","related_cards":["BPR-1749","BPR-1753"],"related_rfcs":["RFC-BPR-0092"]}
acceptance: []
---

# BPR-2256: While a pull request loads, the Agent tab says every agent is not allowed for its host, because setup has no host policy until the comparison is accepted

## Observed

The owner, 2026-11-18, build 68 on macOS. They opened a pull request on a
GitHub Enterprise Server host, which is their only signed-in connection, with
host ID 2. Its agent list held one custom agent (Claude stream JSON, its
own program). The Agent tab's **Set up an agent** step showed three cards,
and each said:

```text
Not allowed for this pull request's host.
```

The cards were the custom agent, Claude Code (found on `PATH`) and Codex
(found on `PATH`). Nothing could be chosen. After a while the tab changed by
itself: the custom agent was chosen, the Repository step was complete, and
**Check it works** was ready.

The session log puts a time on it. The pull request read started 27 s after
launch and succeeded at 63 s. The first epochs were accepted in the same
instant (63.4 s). For about 36 s, the tab showed a refusal that was not true.

The Enterprise host's built-in policy allows Claude stream JSON, the external
terminal profile, and added agents of those protocols. So the custom agent
and Claude Code were allowed all along. Codex is the only one that host
really refuses, and its card looked exactly like the two that were wrongly
refused.

## Cause

1. The window builds its policies once, keyed by host ID: github.com (1) and
   explicit GHES (2) (`window_construction.cpp`, `AgentPolicies`).
2. Setup's rows carry a policy verdict ([[BPR-1749]]). `agent_setup_host`
   starts with `Policy` unset. The only call that sets it is in
   `refresh_agent_authority` (`window_agent_chat.cpp`), and it is reached only
   after the epoch host has an accepted `Authority`, `AcceptedRevision` and
   `AcceptedComparison` that match `AgentReview`. All three early returns
   above it leave the policy as it was.
3. With the policy unset, `decide_agent_invocation` returns
   `host_not_mapped` for every profile, on purpose: "An unmapped host permits
   nothing at all". Every row is `!Verdict.Permitted`.
4. `agent_setup_view.cpp` turns every refused verdict into the same sentence,
   "Not allowed for this pull request's host." It does not distinguish "no
   policy yet" from a real `profile_forbidden_on_host` or
   `profile_not_required_one`.
5. The host is known long before the comparison. `accept()`
   (`window_sign_in.cpp`) sets `AgentReview.Host` from the handoff when the
   pull request is taken, before any read starts. The setup is simply not told
   at that point.
6. The same early returns cause the opposite error when the window moves to
   another pull request. Setup keeps the previous review's policy until the
   new comparison is accepted. A move from a github.com pull request to an
   Enterprise one briefly shows Codex as allowed on a host that refuses it. The
   launch host decides separately, so nothing launches, but the setup view
   says something false.

## Change

### 1. Give setup its host's policy when the pull request is taken

- In `accept()`, as soon as `AgentReview` is set, call
  `AgentSetup->observe_policy(core::policy_for_host(AgentPolicies, aHandoff.Host))`.
  It is the same call `refresh_agent_authority` makes, from the same list.
- In `refresh_agent_authority`, keep the existing call. It should give the
  same answer. Where the accepted `authority.Host` differs from
  `AgentReview->Host`, the function already clears the agent host, and setup
  should be cleared too (rule 2).
- Where the window drops its review (the early returns that call
  `AgentHost->clear()`, and closing the pull request), reset setup's policy
  with `observe_policy(std::nullopt)`. That way a later review never shows the
  previous host's verdicts.

### 2. Tell "not decided yet" from "refused"

- Setup's projection says whether a policy has been observed for the current
  review. The core keeps returning `host_not_mapped` for a missing policy.
  That is its safety rule and does not change.
- While no policy has been observed for a review that is loading, the
  agent cards show their found path and "Waiting for the pull request to
  load…" instead of a refusal. They cannot be chosen, and Check stays
  unavailable, as now.
- Once a policy is observed, a refused card names its real reason:
  - `profile_forbidden_on_host`: "Not allowed for this pull request's host."
  - `profile_not_required_one`: name the required agent.
  - `host_not_mapped` once the review has loaded: "Agents aren't allowed
    for this pull request's server.", the sentence `agent_tab_view.cpp`
    already uses for the send refusal. Reuse that one string rather than
    writing a second.
- Apply the same distinction anywhere else setup rows are shown, such as
  Settings → Agents if it ever shows verdicts. One mapping, not one per view.

### 3. Tests

- Application: an `agent_setup_host` that has not observed a policy reports
  the "waiting" state, not a refusal. After `observe_policy` with an
  Enterprise policy, the Claude stream JSON and custom-agent rows are
  permitted and Codex is refused with `profile_forbidden_on_host`.
- App composition (fixture course): open an Enterprise-host pull request
  whose read is held back. While it is held, the Agent tab shows each card
  waiting and none refused. Release the read: the allowed agents become
  choosable, and Codex shows the real refusal.
- App composition: open a github.com pull request, then move to an
  Enterprise one with its read held back. Codex is not shown as allowed
  while the second pull request loads.
- Qt view: each refusal reason, and the waiting state, renders its own
  sentence.

## Acceptance

- AC1. When a pull request is taken, its Agent tab shows the verdicts of its
  own host's policy as soon as the host is known, without waiting for the
  revision or comparison to load.
- AC2. While no policy has been observed, an agent card never says it is not
  allowed. It says the pull request is still loading, and nothing can be
  chosen or checked.
- AC3. A refused agent names the reason that refused it, and a host's real
  refusal (Codex on an Enterprise host) can be told apart from waiting.
- AC4. Moving to another pull request never shows the previous host's
  verdicts.
- AC5. A missing policy still permits nothing (`decide_agent_invocation` is
  unchanged), and the launch host's own decision is unchanged.
- AC6. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- Making the pull request read faster on Enterprise hosts. About 36 s here
  is a separate question.
- Host policies beyond the two built-in kinds, or policies a user or
  administrator can configure.
- Host-ID allocation. Host IDs are allocated at sign-in from the remembered
  `LastHostId`, while the policy list is fixed at IDs 1 and 2. That is
  correct only while every profile keeps those IDs. If a host can ever take
  another ID, every agent on it would be refused for good, not just while
  loading. That needs its own card if it can happen.

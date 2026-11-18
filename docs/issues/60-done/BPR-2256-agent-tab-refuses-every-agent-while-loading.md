---
schema: "implement-rfc.card.v1"
id: "BPR-2256"
title: "While a pull request loads, the Agent tab says every agent is not allowed for its host, because setup has no host policy until the comparison is accepted"
status: "Done"
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
  whole Agent drawer is one loading state: no Set up steps, no agent cards
  and no refusal, only "Waiting for the pull request to load…". Nothing can
  be chosen or checked. Once the policy arrives, the normal steps appear.
  (Owner amendment 2026-11-18, replacing per-card "Waiting…" text: "If we
  can't get past step 1 then the whole drawer should just be in a loading
  state".)
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
  whose read is held back. While it is held, the Agent drawer is in its
  loading state, and no card or refusal is shown. Release the read: the allowed agents become
  choosable, and Codex shows the real refusal.
- App composition: open a github.com pull request, then move to an
  Enterprise one with its read held back. Codex is not shown as allowed
  while the second pull request loads.
- Qt view: the loading state shows alone, and each refusal reason renders
  its own sentence.

## Acceptance

- AC1. When a pull request is taken, its Agent tab shows the verdicts of its
  own host's policy as soon as the host is known, without waiting for the
  revision or comparison to load.
- AC2. While no policy has been observed, the Agent drawer shows a single
  loading state that says the pull request is still loading, with no Set up
  steps, agent cards or refusals, and nothing can be chosen or checked
  (owner amendment 2026-11-18).
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

## Evidence (2026-11-18, macOS, Debug dev tree)

Commit `9718ff807` on `claude/release-0-5-0-agent-policy-while-loading`.

As built: `agent_setup_projection::PolicyPending` (a host starts pending;
`observe_policy` decides, `forget_policy` pends again). The window's
`observe_setup_policy` gives setup the policy of `AgentReview`'s host from
`accept()`, on both early returns of `refresh_agent_authority` and on its
success path, and forgets it when no review is taken. Where the card said to
clear setup when the accepted authority differs from `AgentReview`, setup
instead keeps the policy of the review being taken: the accepted authority is
the previous review's then, and `AgentReview` is the one the tab is for. A
new pull request's read invalidates the previous authority as it starts,
which resets `AgentReview`, so the previous verdicts go at once. The
`agent_setup_view` shows its loading state alone while pending (owner
amendment); `qt::agent_verdict_words` is the one mapping from a verdict to
words, and `qt::host_not_mapped_words` is shared with the send refusal.
Settings › Agents' Check note, while pending, says to open a pull request or
wait for it to load rather than that the host refuses the agent.

Found while verifying: with the policy known at `accept()`, the setup can
reach Check it works before the comparison is accepted, and the workspace
check then failed (`application_fixture_agent_guard_public_path` and
`application_agent_queue_second_window_loading_public_path` failed on that).
`check_agent_workspace` now waits (`AgentSetupWorkspaceWaiting`, shown
running) and makes the workspace once `refresh_agent_authority` sees the
accepted comparison; Stop or a dropped review ends the wait. Both courses
pass.

- AC1. `observe_setup_policy` runs in `accept()`. Covered by the Enterprise
  and move courses below and the guard course; no fixture hold separates
  `accept()` from the accepted comparison, so "before the comparison" is
  shown by the code path and by the workspace wait it made necessary, not
  by a course.
- AC2. `application_agent_setup_host_policy_public_path` (Enterprise
  fixture, files page held): while held the Agent tab is in setup, the
  projection is pending, the loading state is shown, and no card and no
  "Not allowed" or "aren't allowed" text is present. `qt_agent_setup_view`
  (`test_host_policy_states`): loading state alone, no title, step, card or
  footer. Nothing can be chosen (no card) or checked (Check all disabled:
  no permitted row).
- AC3. After release on Enterprise: the added Claude stream JSON agent and
  Claude Code are permitted; Codex is refused `profile_forbidden_on_host`
  and says "Not allowed for this pull request's host.".
  `qt_agent_setup_view` gives each refusal reason (forbidden, required one,
  not mapped, invalid, unknown) and waiting its own sentence;
  `application_agent_setup_host` (`test_policy_pending_until_observed`)
  shows pending, then the Enterprise verdicts, then a decided unmapped host,
  then pending again.
- AC4. The move course (github.com fixture with PR 17 and 18): Codex is
  permitted on 17; while 18's files page is held the setup is pending, no
  agent is permitted and the drawer is its loading state; after release the
  policy is observed. With the window changes reverted (`window_sign_in.cpp`,
  `window_agent_chat.cpp`) the course fails: "while the next pull request
  loads, no agent shows the previous review's verdict: decided: Claude
  Code+none Codex+none". A fixture serves one host, so the move is within
  github.com, not github.com to Enterprise.
- AC5. `core::decide_agent_invocation` and the launch host are untouched;
  with no policy every row is refused (`test_policy_pending_until_observed`,
  `core_agent_profile` passes).
- AC6. Passed: `application_agent_setup_host_policy_public_path`,
  `application_agent_setup_host`, `qt_agent_setup_view`,
  `qt_workbench_agent_send_refusal`, `qt_agent_footer`, `qt_settings_window`,
  `qt_review_tour_agent_setup_call`, `core_agent_setup`,
  `core_agent_profile`, `application_fixture_agent_guard_public_path`,
  `application_agent_queue_second_window_loading_public_path`,
  `application_agent_queue_windows_public_path`,
  `application_agent_queue_close_runs_on_public_path`,
  `application_agent_queue_restart_public_path`,
  `application_agent_chat_helpers_public_path`,
  `application_agent_keep_action_public_path`,
  `application_agent_steer_sent_public_path`,
  `application_agent_steer_unavailable_public_path`,
  `architecture_boundaries`; clang-format 22 check on the changed C++ files;
  clang-tidy clean on them apart from two findings already in
  `agent_tab_view` lines this change does not touch; the documentation gate
  after merging main. `test_network_isolation_manifest` fails on
  `sqlite_prompt_pack_storage` and `gh_cli_git_smart_transport`, as on main
  before this change.
- The UI work loaded the frontend-design skill; the loading state is a
  native busy bar and one muted line, with the platform palette and style.

The host-ID question in Out of scope can happen: [[BPR-2257]] (Backlog).


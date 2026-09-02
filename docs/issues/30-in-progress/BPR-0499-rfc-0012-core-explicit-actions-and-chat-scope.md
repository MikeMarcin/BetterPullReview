---
id: BPR-0499
title: "Core: explicit AI actions, side-chat scope, and stale continuation"
status: In Progress
rfc: RFC-BPR-0012
depends_on: [BPR-0496]
---

# Core: explicit AI actions, side-chat scope, and stale continuation

Extends `core::decide_agent_invocation` in
`src/core/include/bpr/core/agent_profile.hpp` to cover the seven entry points
`docs/ux/ai-experience.md` names — ask about selection, analyze current file,
analyze latest upload, evaluate proposed fix, workshop comment, run AI review,
open full agent session — as explicit intents and nothing else. It adds chat
scope as a typed value and reuses `agent_session_staleness`,
`agent_revision_binding`, and `observe_agent_session` from
`src/core/include/bpr/core/agent_session.hpp` unchanged for continuation.

## Required final state

- The seven entry points are a closed enumeration. Legality stays a pure
  function of the profile, the host policy, the current selection, and the
  session, so the suite runs under `core-only`.
- Every legal invocation carries an explicit intent. There is no
  default-constructed or implied intent, so refresh, navigation, comment
  arrival, and revision update are observations with nothing to pass.
- Chat scope is a typed value over selected lines, file, pull request, and
  repository. It starts at the reviewer's selection and widens only through an
  explicit intent that records the scope it came from.
- Private notes and unpublished drafts are opt-in per session and default off.
  The opt-in lives on the session rather than a profile or a setting, so it
  cannot be turned on once and forgotten.
- After `head_moved` or `epoch_pair_changed` a session offers exactly two
  continuations — a new session, or an explicit old-revision continuation —
  and retargets neither. `observe_agent_session` keeps returning a verdict.

## Non-goals

- The finding type and anchor verdicts — BPR-0497; promotion — BPR-0498.
- The drawer, the chips, and the stale banner — BPR-0502.
- Any process, protocol, decode, or bound — BPR-0500.
- The MCP helper's scope, which is its own frozen projection — BPR-0501.

## Required direct evidence

- `ctest --preset core-only -R '^core_agent_(session|profile|chat_scope)$'
  --output-on-failure`. The implementing slice records whether the new cases
  extend `core_agent_session` and `core_agent_profile` or register
  `core_agent_chat_scope`.
- A table-driven case over all seven entry points asserts each is legal only
  with its own explicit intent; an eighth without a case fails the count.
- Refresh, navigation, comment arrival, and revision update are each swept and
  each produces zero invocations, asserted by reading the type.
- A chat turn at file scope with no widening intent stays there; making it
  widen fails by name, and the widening case records the prior scope.
- A session without the notes opt-in carries zero notes and zero drafts even
  when both exist for the review.
- After `head_moved` the session offers exactly the two continuations; making
  it retarget fails the existing binding assertion in `core_agent_session`.

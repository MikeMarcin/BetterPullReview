---
schema: "implement-rfc.card.v1"
id: "BPR-2378"
title: "After the head moves, the agent runtime keeps a comparison of its own and the next Agent tab turn is refused"
status: "Done"
rfcs: ["RFC-BPR-0126"]
depends_on: ["BPR-2181", "BPR-2193"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"pinned-session implementer (pinned-session card actions), 2026-11-24: reproducible without a pinned conversation","reservation":"docs/issues/_reservations/BPR-2378--BPR-2378--c4c270dd9de2.json","related":["BPR-2181","BPR-2193"],"acceptance":["BPR-2378:core","BPR-2378:host","BPR-2378:course"]}
---

# BPR-2378: After the head moves, the agent runtime keeps a comparison of its own and the next Agent tab turn is refused

## Report

Found by the implementer of the pinned-session card actions, and
reproducible without a pinned conversation. After the pull request's head
moves, the agent runtime keeps the comparison it was given at the move while
the window settles on a different one at the same head (observed: comparison
epochs 3/3 in the runtime, 1/1 in the window). The next Agent tab turn at
the new head is refused with `stale_or_mismatched_agent_context`, or ends as
`stale_result`. A plain send from a conversation never used reproduces it.

## Cause

The Agent tab's turns run on the profile's runtime of the session
([[BPR-2181]]). Its `review_agent_stack` has an epoch host of its own, fed by
the authority the window lends it. That epoch host resolves its own comparison
(its default selection, Whole PR) and never learns which comparison the
window shows. A turn the window composes names the window's comparison epochs
in its binding (`dispatch_agent_invocation`), and the stack's agent host
admits only a turn whose binding equals the one it observes
(`agent_invocation_host::admissible`).

Before a head move the two usually agree, because both resolve Whole PR on
the same catalog. After a move the window can settle on another pair: a
restored tab's comparison, Previous upload to latest, or a pair the reviewer
picks. The window then lends the same revision again with its new epochs.
The stack accepts the loan, resolves Whole PR again, and observes epochs the
window never shows. Every fresh turn is refused as
`stale_or_mismatched_agent_context`. [[BPR-2193]]'s note records the same
gap ("the stack's own epoch host need not reach the window's epochs"); it
let turns start before the comparison matched, which avoided a hang and left
this refusal.

## Fix

- Core: `core::follow_comparison(epoch_state, explicit_comparison_selection)`
  (`epoch_transition.hpp`) decides how a second consumer of a review reaches
  the comparison another consumer settled on. It returns `followed`,
  `waiting` (ingestion, a resolution or an import is running, or nothing is
  accepted), `select` with the `select_comparison_anchors_intent` to
  dispatch, or `unreachable`. Unreachable means the pair belongs to another
  authority or catalog incarnation, or was asked for and resolved to
  something else.
- Composition: the window lends the runtime the comparison it settled on
  beside the revision (`attach_agent_runtime`, `review_agent_stack::lend`).
  Once a loan's ingestion has settled, the stack asks its epoch host for
  that pair as the core decides. It asks at most once per catalog revision,
  so a refused ask ends. No turn starts until the core says the pair is
  followed or cannot be: a turn sent meanwhile waits (`accepting`) and
  starts on `Ready`. If the pair cannot be followed, the next turn is refused
  by name. It is never answered under another comparison.
- A turn already running keeps the comparison it was admitted under. The
  stack does not follow while its agent host runs a turn, because moving
  the binding then would supersede the turn. It follows when that run
  retires, before the runtime hears of the retirement.
  [[BPR-2193]]'s waiting loan carries the comparison too.
- Test access: `agent_chat_composition_evidence::RuntimeObserved`, the
  binding the runtime's agent host observes.
- Docs: `docs/architecture/system-architecture.md` (the profile runtime
  paragraph).
- The production application manifest digest in
  `application_review_round_trip_composition_contract.cmake` is resealed.

## Acceptance

- **BPR-2378:core** `core_epoch_policy`
  (`test_follow_comparison_decides_the_settled_pair`): nothing accepted
  waits. The accepted pair is followed. Another pair is asked for at the
  accepted catalog revision, and equal inputs decide equally. A resolution
  in flight waits, and once it resolves the pair is followed. A pair asked
  for that resolved to another is unreachable, and so is a pair from another
  catalog incarnation. Passed at 16b9977cc.
- **BPR-2378:host** `application_epoch_host`
  (`a_second_consumer_follows_the_comparison_the_window_settled_on`): an
  epoch host standing in for the runtime's accepts a moved head and resolves
  its own comparison. Following a pair the window settled on at that head,
  the host dispatches the core's selection, waits for its resolution, and
  then accepts exactly that pair at the moved head. A followed pair asks
  nothing more. Passed at 76ea55709 (before the rebase; the test and its subject are unchanged by it).
- **BPR-2378:course** `application_agent_chat_head_move_send_contract`
  (fixture public path, the fixture agent only). The head moves with the
  shipped refresh before any question was asked. The window settles at the
  new head, and the reviewer picks Previous upload to latest and starts a new
  side-chat session. The Agent tab's Send is then answered by one process,
  with no refusal. The session's binding, the window's binding and the
  runtime's observed binding are one value. Passed at 16b9977cc; 8 of 8 repeats at 76ea55709,
  together with `application_agent_chat_continued_contract` and
  `application_agent_chat_anchored_continuation_contract`.

Regression set, passed at 16b9977cc (rebased onto 9ea34d63b, after
[[BPR-2376]] and [[BPR-2377]]): `application_agent_chat_*` (all 14 rows),
`application_native_agent_sessions`,
`application_review_round_trip_composition_contract`,
`application_composition_source_contract`,
`application_agent_invocation_host`, `core_epoch_policy`,
`architecture_boundaries`, and `coding-format-check`. Before the rebase, at
76ea55709, `application_agent_finding_composition_contract`,
`application_agent_finding_dif_context_contract`,
`application_agent_invocation_composition_contract`,
`application_agent_invocation_staleness_contract`,
`application_review_agent_runtime`, `application_fixture_agent_every_ask`,
`application_agent_queue_windows_public_path`,
`application_agent_queue_close_runs_on_public_path`,
`application_agent_queue_restart_public_path`,
`architecture_boundary_contract`, `test_network_isolation_manifest` and
`application_epoch_host` also passed.

The course row is registered `WINDOWS_UNCONTAINED` and pinned
`unmeasured (registered on macOS)` in
`scripts/verify_test_network_isolation.py`.

---
schema: "implement-rfc.card.v1"
id: "BPR-2253"
title: "qt_workbench_agent_send_refusal and application_native_agent_sessions fail on main"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-2253--BPR-2253--18fec1e7b72b.json","origin":"seen while building BPR-2252, 2026-11-17"}
---

# qt_workbench_agent_send_refusal and application_native_agent_sessions fail on main

While building [[BPR-2252]], the implementer ran the wider
`agent|tour_chat|workbench_window_scale` set. 120 of 122 tests passed. Two
failed, and they also fail the same way on c6a27facc with BPR-2252's change
reverted, so BPR-2252 did not cause them:

- `qt_workbench_agent_send_refusal`
- `application_native_agent_sessions`

The likely sources are today's lane changes: BPR-2241 added turn seconds,
BPR-2245 changed the queue, BPR-2251 and BPR-2250 changed the history
executor, and BPR-2248 added the commit asks.

## Acceptance

- Find the commit that broke each test. Fix the code, or the test if its
  expectation is stale.
- Both tests pass at -j4.

## What broke each test (2026-11-17)

Neither failure is a product regression. Both tests hold an expectation that
an intended, owner-driven change made stale. The culprits were confirmed by
building each test at the commit and at its parent in a separate tree.

### `qt_workbench_agent_send_refusal`: b3569ae3b ([[BPR-2219]])

- Passes at 5637808d0 and fails at b3569ae3b with "a suggestion pressed while
  the Ready line is up is sent as written".
- b3569ae3b, `src/core/src/prompt_macro_shipped.cpp:123`: the built-in
  Review this pull request prompt's body changed from its name to its
  instructions (`review_body(...)`). Owner, 2026-11-16: "How is 1 sentence
  useful to anyone trying to tweak the review request to their particular
  circumstances?"
- A chip sends its body (`agent_tab_view::press_offer` sends
  `offer.Expansion.Text`). The test compared the sent text with the chip's
  name (`agentPrompt`), which equalled the body only while the body was the
  name.
- Fix, the test: "as written" is the chip's body, `agentPromptExpansion`
  (`tests/adapters_qt/workbench_window_tests.cpp:15378`).

### `application_native_agent_sessions`: f82523784 ([[BPR-2154]])

- Passes at f15e0c07f and fails at f82523784 in the course
  `tour-native-reopen` with `tour-native-capabilities-wait-for-retirement`.
- f82523784, `src/adapters/qt/src/tour_side_chat.cpp:1280` (now 1290):
  RFC-BPR-0126 made words typed in a tour chat while its turn is shown
  (`Running || !HostIdle`) a follow-up, so Send is enabled as Queue (or
  Steer). The [[BPR-2040]] check, from 89a9e1cd8, held the Codex host's
  retirement and required Send to be disabled.
- What the check protects still holds: no new process launches before the
  host retires, New session waits, and a queued message is released only by
  the retirement (`chat_queue_retired`, [[BPR-2152]]). With the
  instrumentation the course saw quiescent=0, New session disabled, the
  status line not live and Send enabled.
- Fix, the test: while retirement is held the button may be enabled only as
  Queue or Steer, never Send
  (`tests/end_to_end/agent_invocation_composition_public_path.cpp:1407`).

[[BPR-2252]] is unchanged.

## Evidence

- Lane 26bd84fb5, `cmake --build --preset dev`: both tests pass at -j4, and
  the 13 tests matching
  `^qt_workbench_agent|^application_native|agent_invocation` pass at -j4.
- Format check and clang-tidy on the two changed files pass;
  `python3 scripts/verify_documentation.py --root .` prints DOCUMENTATION_OK.

---
schema: "implement-rfc.card.v1"
id: "BPR-0502"
title: "Qt: side-chat drawer with context chips and stale state"
status: "Done"
rfcs: ["RFC-BPR-0012"]
depends_on: ["BPR-0499","BPR-0500"]
---

# Qt: side-chat drawer with context chips and stale state

A new side-chat panel sits beside
`src/adapters/qt/include/bpr/qt/agent_launch_panel.hpp` and presents what a
reviewer needs to know before typing: provider, Git host, workspace, selected
epoch pair, exact head OID, the context chips currently in scope, and whether
the session is current or stale. Every send, scope change, note opt-in, and
continuation choice emits an intent through a dispatch callback. The widget
holds no process, no launcher, and no credential.

## Required final state

- The panel presents a projection it is given and nothing it computed. Nothing
  the user does changes the display; it moves only when `present` is called
  with an accepted projection, as `review_draft_editor` already works.
- Provider, Git host, workspace, selected epoch pair, and the exact head OID
  are each shown, not summarized — the RFC's visible session contract.
- A context chip appears only from an explicit context action. Navigating,
  refreshing, and receiving a comment add none.
- A stale projection shows stale and offers exactly the two continuations
  BPR-0499 defines. The widget starts neither on its own.
- The panel holds no launcher, process, credential, or endpoint member; the
  only way out is an intent, which is what makes "a widget performs no
  effects" structural rather than reviewed.
- The suite runs with no GitHub, Git, SQLite, AI, filesystem, network, or
  process effect.

## Non-goals

- Entry-point legality, scope policy, and continuation rules — BPR-0499.
- Findings, the queue, and the workshop — BPR-0503.
- Decode, bounds, and delivery — BPR-0500.
- Constructing the panel in the shipped window — BPR-0504.

## Required direct evidence

- A new `qt_side_chat_panel` registered in `tests/adapters_qt/CMakeLists.txt`
  beside `qt_agent_launch_panel`, run with
  `ctest --preset dev -R '^qt_side_chat_panel$' --output-on-failure`.
- A field-reading guard fails to compile when a launcher, process, credential,
  or endpoint member is added to the panel.
- Every disclosed field is swept: changing it in the projection changes the
  presented value, and deleting the presentation of any one fails by name.
- Driving refresh, navigation, and a simulated comment arrival through the
  panel produces zero intents and zero new chips.
- A stale projection shows stale and offers both continuations; each emits an
  intent only when chosen, and making either start by itself fails.
- `ctest --preset dev -R '^architecture_boundaries$' --output-on-failure`
  still shows no `QProcess` in the Qt adapter.

## What this slice delivered — 2026-09-02

`src/adapters/qt/include/bpr/qt/side_chat_panel.hpp` and
`src/adapters/qt/src/side_chat_panel.cpp` (the `side_chat_projection` the panel
is given, `side_chat_context_chip`, the four typed intents —
`send_chat_message_intent`, `widen_chat_scope_intent`,
`chat_private_context_intent`, `choose_chat_continuation_intent` — the
`side_chat_panel_members` aggregate the guard reads, and the panel itself),
proven by the new `qt_side_chat_panel` registered beside `qt_agent_launch_panel`
in `tests/adapters_qt/CMakeLists.txt`.

### Decisions worth knowing

- **The projection has no field the observed revision could occupy.** The panel
  is handed the session's own binding and the `agent_continuation_offer` core
  already decided, never the head now under review, so BPR-0499's
  "retargets neither" survives one layer outward rather than being restated.
- **Ten fields, ten accessors, one sweep.** Provider, Git host, workspace,
  epoch pair, head OID, session state, scope, context chips, private-context
  opt-in, and continuations are separate labels read through separate
  accessors. Every sweep row asserts the named field moved *and* that the other
  nine did not, so a panel that summarised any two into one sentence fails the
  rows of both.
- **The panel's members are one aggregate**, and the guard reads it two ways: a
  structured binding names all 23 members and their types, and
  `sizeof(side_chat_panel)` is pinned to a probe holding exactly that aggregate.
  A forbidden member added inside the set fails the first; one added to the
  class outside it fails the second.
- **`observe` contains no dispatch call and writes no field.** Refresh,
  navigation, comment arrival, and revision update reach the panel through it,
  and all it does is record `core::note_agent_observation`.
- **A stale session is answered before it is talked to again**: `Send` is
  disabled while the offer is `Required`, and answering is an intent like every
  other gesture. Choosing does not move the display; the next projection does.
- **One judgement beyond the card's words**: the send intent carries a
  `core::agent_action_intent` built from the projection's own `EntryPoint`,
  so every message leaving the panel names the explicit action it is.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The new suite passes | `ctest --preset dev -R '^qt_side_chat_panel$' --output-on-failure` | 1/1 passed, 0.22 s | this commit |
| No `QProcess` survives in the Qt adapter | `ctest --preset dev -R '^architecture_boundaries$' --output-on-failure` | 1/1 passed | this commit |
| The siblings and the core half are undisturbed | `ctest --preset dev -R '^(qt_agent_launch_panel\|qt_agent_launch_canary\|core_agent_chat_scope)$' --output-on-failure` | 3/3 passed | this commit |
| The whole dev graph still builds | `cmake --build --preset dev` | build exit 0 | this commit |
| Formatting and whitespace | `clang-format --dry-run --Werror` on the three touched C++ files; `git diff --check` | exit 0 | this commit |
| Every disclosed field moves on its own | the 14-row sweep over every disclosed field, each asserting the other nine fields did not move | pass | this commit |
| Refresh, navigation, comment arrival, and revision update add nothing | the four-observation sweep: zero intents, zero chips, no field movement | pass | this commit |
| A stale session shows stale, offers exactly two continuations, starts neither | the stale path, with `OfferedBinding` asserted equal to the session's binding and unequal to the moved head | pass | this commit |
| Non-vacuity | 17 scripted mutations, each built, run, restored, and md5-verified byte-exact: ten presentation deletions and two self-starting continuations each failed by name, four forbidden members failed the field-reading guard at compile time, and a member added outside the aggregate failed the size guard | every mutation failed; every restore byte-exact; the restored tree builds and passes | this commit |

Handed to the next slices: nothing constructs this panel yet — composing it in
the shipped window, and populating the projection from the host, is
[[BPR-0504]]'s work. The finding queue and comment workshop are [[BPR-0503]]'s.

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0) and ran
`ctest --preset dev -R '^(qt_side_chat_panel\|qt_agent_launch_panel\|core_agent_chat_scope\|architecture_boundaries\|application_mcp_context_helper)$' --output-on-failure`:
5/5, `qt_side_chat_panel` 0.22 s, `architecture_boundaries` 7.64 s.
`clang-format --dry-run --Werror` clean on the three new files. The worker's
note stands: nothing constructs the panel yet, which is [[BPR-0504]]'s.

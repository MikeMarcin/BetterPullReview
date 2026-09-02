---
id: BPR-0502
title: "Qt: side-chat drawer with context chips and stale state"
status: Ready
rfc: RFC-BPR-0012
depends_on: [BPR-0499, BPR-0500]
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

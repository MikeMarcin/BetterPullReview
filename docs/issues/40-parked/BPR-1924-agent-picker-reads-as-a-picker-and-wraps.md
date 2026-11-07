---
schema: "implement-rfc.card.v1"
id: "BPR-1924"
title: "The agent/model/effort picker reads as a picker, not a text field, and wraps to a second row when the drawer is narrow"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-07, BPR-1914 v004 look","related_cards":["BPR-1878","BPR-1914"]}
parked_on: "human-review"
acceptance: [{"criterion": "AC1 Offscreen pictures at 380, 520 and 900 px drawer widths, light and dark, show the picker as a picker; at narrow widths it sits on its own row with its full text", "outcome": "pass", "revision": "e64e658b2", "evidence": {"kind": "command", "detail": "qt_agent_picker_wrap with BPR_AGENT_PICKER_RENDER_ROOT draws the Agent and Tour tabs at 380, 520 and 900 in light and dark: a filled pill with an up-and-down chevron; wrapped under the tools with its whole words at 380 and 520, beside Send at 900; the Tour picker bare on its bar. The frozen look (--attended-agent-picker) printed ATTENDED_AGENT_PICKER_SHOWN:wrapped=1:drawer=380.", "path": "docs/issues/40-parked/BPR-1924-agent-picker-reads-as-a-picker-and-wraps.md#verification"}}, {"criterion": "AC2 A Qt test checks the wrap threshold, and that the picker never overlaps or truncates the question field", "outcome": "pass", "revision": "e64e658b2", "evidence": {"kind": "command", "detail": "qt_agent_picker_wrap sweeps the drawer from 380 to 900 by 4 in light and dark: the picker wraps exactly when scope + gap + picker + Send at their size hints exceed the row; it never intersects the composer, Send or the scope menu; the composer keeps the field's whole width; the picker keeps its size hint; once unwrapped it stays so as the drawer widens, and wraps again at 380. It also checks the chevron is drawn, the pill answers the pointer and no field is drawn round the Tour picker.", "path": "docs/issues/40-parked/BPR-1924-agent-picker-reads-as-a-picker-and-wraps.md#verification"}}, {"criterion": "AC3 Owner look: Accept/Reject on a frozen build", "outcome": "pending", "revision": "e64e658b2", "evidence": {"kind": "observation", "detail": "Frozen build /Users/jaeger/.codex/attended-builds/bpr1924-agent-picker-v001.mzvj9dl5 of e64e658b2; new short session 9c9dd1be31be491685541f4fd0be8f15, candidate BPR-1924-agent-picker-v001, one Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1924-agent-picker-reads-as-a-picker-and-wraps.md#attended-review-v001"}}, {"criterion": "AC4 The documentation gate passes", "outcome": "pass", "revision": "e64e658b2", "evidence": {"kind": "command", "detail": "verify_documentation.py --root . prints DOCUMENTATION_OK.", "path": "docs/issues/40-parked/BPR-1924-agent-picker-reads-as-a-picker-and-wraps.md#verification"}}]
---

# BPR-1924: The agent/model/effort picker reads as a picker, not a text field, and wraps to a second row when the drawer is narrow

## Owner (2026-11-07)

"this box around the model picker looks more like a text entry field than a
dropdown/picker. It also makes me think that maybe the picker needs to wrap to
a 2nd row on the agent tab if the drawer is narrow."

The owner's two screenshots are in the BPR-1914 v004 attended session
(`/Users/jaeger/.codex/attended-review/bpr1914-v004-QBUboY/state`).

## Change

Load `frontend-design:frontend-design` first.

- The bottom bar's agent · model · effort control (Agent tab, and the Tour
  tab's copy from BPR-1878; side chat composers too, if they show it) reads
  unmistakably as a picker: a chevron or popup affordance, and a hover and
  press state, not a text-field-like bordered box around the question field.
  Keep the question field visually distinct from the picker.
- In a narrow drawer, the picker wraps to its own row under the question
  field instead of crowding or truncating it. In a wide drawer it stays on one
  row.

## Acceptance

- AC1. Offscreen pictures at 380, 520 and 900 px drawer widths, light and
  dark, show the picker as a picker. At narrow widths it sits on its own row
  with its full text.
- AC2. A Qt test checks the wrap threshold, and that the picker never
  overlaps or truncates the question field.
- AC3. Owner look: Accept/Reject on a frozen build.
- AC4. The documentation gate passes.

## Implementation (2026-11-07)

`frontend-design:frontend-design` was loaded first. The design choice: the
picker borrows the up-and-down chevron of the native "Selected lines" menu
beside it, so the two pickers in the row read as one kind of control and
neither as a place to type. All colours come from the palette.

- **The picker** (`agent_run_settings_picker.cpp`): a filled pill (base mixed
  with the text colour at 11%), the chevron on its right, 18% under the
  pointer, 26% while pressed or its menu is open, and a highlight ring for
  keyboard focus. The icon-only narrow mode (`set_narrow`) is removed: the
  picker always shows its whole words, and its tooltip leads with them.
- **Agent tab** (`agent_tab_view.cpp`): `fit_run_settings` runs on the
  field's resize and layout requests. It keeps the picker beside Send while
  +, the spark, the scope menu, the picker and Send fit the row at their size
  hints with a 12 pixel gap; otherwise the picker moves to a row of its own
  under the tools, left-aligned. The choice depends only on the field's width
  and the controls' sizes, never on where the picker is now.
  `run_settings_wrapped()` reports it.
- **Tour tab** (`review_tour_editor.cpp`): `agent_bar_field`, the rounded box
  that looked like a text field, is gone. The picker stands bare on the bar,
  its right edge on Preview and publish's. `fit_agent_bar` is removed.
- **Side chats** show no picker, so nothing changed there.
- **Look** (`main.cpp`): `--attended-agent-picker` with
  `--attended-tour-authoring` finds only the stub `claude`, drags the Comments
  drawer to 380 pixels and opens the Agent tab
  (`ATTENDED_AGENT_PICKER_SHOWN:wrapped=1:drawer=380`). The round-trip
  composition contract is resealed for it.

### Verification

- `qt_agent_picker_wrap` (new), `qt_review_tour_agent_help`,
  `qt_review_tour_agent_setup_call`,
  `application_review_round_trip_composition_contract` (resealed),
  `architecture_boundaries`: pass.
- `qt_drawer_surfaces_fit_scale_100` fails with 16 clips of
  `tourDraftTour` and `tourSessionTranscriptOpen` at 280 pixels. It fails the
  same way with this card's six Qt files put back to `main` (6c52dc5b7), so
  it is not this change.
- `coding-format-check` passes; `verify_documentation.py` prints
  `DOCUMENTATION_OK`.
- Not run, by the owner's "dont wait for tests": the full
  `workbench_window_tests` course and the other scales.

### Attended review (v001)

- Frozen build `/Users/jaeger/.codex/attended-builds/bpr1924-agent-picker-v001.mzvj9dl5`
  of `e64e658b2` (executable SHA-256
  `5f93d41085b22a2afc86a0b44eca1b99d66c85f3f76881f232f32597082d39a6`).
  Its launcher's own offscreen check reached
  `ATTENDED_AGENT_PICKER_SHOWN:wrapped=1:drawer=380`.
- Launch: `python3 /Users/jaeger/.codex/attended-builds/bpr1924-agent-picker-v001.mzvj9dl5/launch.py`.
- New short session `9c9dd1be31be491685541f4fd0be8f15`, state
  `/Users/jaeger/.codex/attended-review/bpr1924-v001-iLA7d7/state`, host
  started with `serve --tailscale`.
- One candidate, `BPR-1924-agent-picker-v001` (revision `e64e658b2-v001`),
  with one Accept/Reject question.
- Status: ready, awaiting the owner.

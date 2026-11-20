---
schema: "implement-rfc.card.v1"
id: "BPR-2311"
title: "Agent history and setup drop numbered-slot histories and the pre-list settings move"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"cleanup","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2310--BPR-2313--b73895df6230.json","origin":"owner, 2026-11-20: \"All, incl. notes check\" (vestigial items cut; no legacy support for old data)","related_cards":["BPR-1753","BPR-1769","BPR-1847","BPR-2283","BPR-2310","BPR-2312"],"acceptance":["AC1","AC2","AC3","AC4"]}
---

# BPR-2311: Agent history and setup drop numbered-slot histories and the pre-list settings move

## Asked

Owner, 2026-11-20, approving the vestigial-item list: "All, incl. notes
check". The owner's standing policy: no legacy support for old data (as for
[[BPR-2283]]'s theme files).

## What changed

- **Legacy histories.** The Agent tab's History › Sessions menu no longer has
  "Legacy histories (agent identity unknown)" with View and Replay, and the
  read-only "Legacy history" dialog that View opened goes with it.
  `native_agent_history_projection::Legacy` is gone, and so is the optional
  `agent_conversation_key` that `native_agent_session_port::history` and
  `replay` and `agent_invocation_host::restore_native` took to find numbered
  agent slots. The SQLite store no longer lists slots, reads a `legacy:N`
  view, or replays from one; a `legacy:N` source is now refused as any
  unknown source is. The composition stops passing the pull request's key
  and stops routing read-only views to the Agent tab (only `view:` produced
  them, and only the legacy menu sent one there). The product contract drops
  the "Legacy numeric agent slots" sentence.
- **Pre-list agent settings.** `core::legacy_agent_settings` and
  `core::migrate_agent_entries` are gone. A profile whose settings were never
  moved into the agent list now starts with the list it has (normally
  empty) and is marked moved; nothing is read from the old Claude wrapper,
  model, effort or Codex and harness fields. `agent_setup_dependencies`
  loses `ClaudeWrapper` and `LegacyClaudeModel`; setup's saved Claude wrapper
  is only the list's Claude Code entry.
- **Turn outcomes.** `core::legacy_agent_turn_outcome` is gone. A stored
  per-pull-request turn with no kept outcome (written before [[BPR-1847]]) is
  left out when the conversation loads; the rest of the conversation loads.

## Kept, and why

- The `agent_conversations` tables and `agent_conversation_store`: they are
  live. [[BPR-2181]] writes turns no window saw there, and the scratch choice,
  build answer and scratch folder are kept there.
- The workbench profile's `ClaudeWrapper` and `ClaudeModel` fields and the
  window's `claude_process_wrapper_source`: still read by the review-cycle
  and quick-ask launch paths, outside this card.

## Acceptance

- AC1. No history menu offers a numbered agent slot, and the store neither
  lists nor views nor replays one.
- AC2. Loading unmoved setup settings yields the list as stored, marked
  moved, and no agent is made from the old fields; it stays so on the next
  load.
- AC3. A stored turn with no outcome is left out and the conversation loads.
- AC4. Build, focused tests, `architecture_boundaries`, format and tidy on
  changed files, and the documentation gate pass.

## Evidence

Dev preset, macOS, worktree branch `claude/vestigial-cuts` on main
`c937eab1b`:

| Criterion | Check | Result |
|---|---|---|
| AC1 | `sqlite_agent_setup_storage` (`legacy:1` is no replay source; replay, history and capacity courses unchanged), `application_agent_invocation_host` | Passed |
| AC2 | `application_agent_setup_host` `test_the_agent_list` (Chosen, effort and a wrapper set; the list stays empty and moved, and at the next load), `test_saved_programs_win`, `test_auto_chosen_wrapper_keeps_its_entry` and `test_reset_does_not_resurrect_saved_wrapper` now seed the wrapper as a kept list entry; `core_agent_entry`, `core_agent_grant` | Passed |
| AC3 | `sqlite_agent_setup_storage`: outcomes deleted for a stored thread; it loads with those turns left out; `core_agent_turn` | Passed |
| AC4 | `architecture_boundaries`; `format.py` and `format_check.py` (clang-format 22.1.8) on every changed C++ file; `verify_documentation.py` | Passed |
| AC4 | `tidy.py --warnings-as-errors` on every changed C++ file | No finding on a changed line. Four predate this card and were left as they were: `agent_setup_store.cpp` string concatenation in the table rebuild (two), `agent_tab_view.cpp` `press_offer`'s copy and parameter name, and a string concatenation in `agent_invocation_composition_public_path.cpp` |

The Agent tab, the composition (`window_agent_setup.cpp`,
`window_construction.cpp`), `qt_claude_process_wrapper` and
`application_agent_invocation_composition_public_path` (its
`transcript-replay` course now checks the Sessions menu offers no submenu)
compile; those suites were not run, because the Qt adapter and application
build did not fit the shared disk.

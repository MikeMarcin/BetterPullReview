---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AQ2"
title: "The host detects the agents when setup shows, checks the chosen one and keeps the choice"
status: "Done"
rfcs: ["RFC-BPR-0092"]
depends_on: ["BPR-PENDING-AQ1"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0092 adoption, 2026-10-22 (owner answers Q1-Q9)","related_cards":["BPR-PENDING-AN"],"milestone":"RFC-BPR-0092 M2"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "a856809fa", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^application_agent_setup_host$'", "detail": "test_detection_starts_nothing: load, detect and Look again run on manual lanes against a scripted process port, and its request list stays empty while rows, the choice without asking, Searched and the Repository step are published. Saved programs are inspected and never run (test_saved_programs_win). The composed window builds the host and only loads it; nothing triggers a check until a gesture."}}, {"criterion": "AC2", "outcome": "pass", "revision": "a856809fa", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(application_agent_setup_host|application_agent_launch)$'", "detail": "The check runs claude auth status --json and then --version for a signed-in Claude, codex login status for Codex, each with agent_status_timeout_milliseconds, no input, WritingOnly's empty folder, the profile's allowlist and no credential name. Decoded results: Claude loggedIn true passes with version 2.1.296; false is not_signed_in; 'Welcome!' is status_unreadable; failed_to_start and timeout are their own failures; Codex 'Not logged in' exit 1 is not_signed_in and 'Logged in using ChatGPT' passes with 0.163.0. Stop cancels the port, resets the row and a late answer is not taken. A forbidden agent is never chosen or checked."}}, {"criterion": "AC3", "outcome": "pass", "revision": "a856809fa", "evidence": {"kind": "command", "command": "ctest --test-dir build/rel -R '^(application_agent_setup_host|sqlite_agent_setup_storage|qt_agent_launch_panel|application_review_cycle_send_public_path|core_profile_data_reset|sqlite_profile_data_reset_files|sqlite_release_maintenance_files|application_review_round_trip_composition_contract|command_correctness_registry_contract|architecture_boundaries)$'", "detail": "A second host over the first's stored preferences opens on the saved agent (test_choice_and_steps). sqlite_agent_setup_storage reads defaults, survives a restart, refuses over-long and control-character programs, refuses another store's identity and keeps the file owner-private. qt_agent_launch_panel's new case selects the saved profile on offer and again on re-offer, and the first profile when cleared; review_cycle_desktop::prefer_send_profile reorders the given profiles with the saved one first, and the Send to Agent course still passes. agent-setup is a settings store for resets and purge. Contracts are resealed and pass; format and tidy are clean."}}]
---

# BPR-PENDING-AQ2: The host detects the agents when setup shows, checks the chosen one and keeps the choice

RFC-BPR-0092 M2 ([RFC](../../rfcs/RFC-BPR-0092-agent-first-run-setup.md#milestones)).

## Scope

- Detection when setup shows and on Look again (Q2): file metadata only, no
  process, nothing written.
- The agent row of Check it works: the agent's status command and a
  `--version` read through the supervised launcher, bounded at 15 seconds,
  Stop-able, with the profile's allowlist and no review context (Q3 A).
- The chosen agent saved for the application profile, and Send to Agent's
  default.

## Acceptance criteria

- AC1: Showing setup starts no process (launcher probe unchanged) (RFC AC01,
  host share).
- AC2: The check's passed, not-signed-in and failed-to-start results reach the
  setup state from fixture agents, and Stop and the bound end it (RFC AC10,
  agent row).
- AC3: The saved agent survives a new window and a restart, and Send to Agent
  offers it first (RFC AC11, agent).

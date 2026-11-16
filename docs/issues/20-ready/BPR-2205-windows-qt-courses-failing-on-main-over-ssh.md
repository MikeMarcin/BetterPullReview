---
schema: "implement-rfc.card.v1"
id: "BPR-2205"
title: "Seven Windows Qt courses fail on main, run directly with ctest's environment"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"BPR-2200's Windows Qt run, checked against main on the Windows station over SSH, 2026-11-15","reservation":"docs/issues/_reservations/BPR-2205--BPR-2205--ec06f7cc61c1.json","related_cards":["BPR-2200"],"kind":"already broken"}
---

# BPR-2205: Seven Windows Qt courses fail on main, run directly with ctest's environment

## Found

BPR-2200 ran the whole Windows Qt set on the lane
(`claude/v0-5-0-prerelease-targets-52265d`). Eight entries failed on the same
line with and without the Windows fonts. Built from main at `ba6b79d67`
(Release) and run the same way, seven of them fail on main with the same
line, so the lane did not cause them:

| Entry | Failure line on main and on the lane |
|---|---|
| `qt_agent_finding_cards_restart` | `the fresh host reads the one kept card back` |
| `qt_agent_setup_view` | `an agent added in Settings is a card at once` |
| `qt_agent_steering_fixture` | `the fixture agent is built as claude` |
| `qt_claude_process_wrapper` | `a writing run, wrapped or not, starts in a fresh writing folder removed afterwards` |
| `qt_profile_management_window` | `Pull Request carries the window menu's own commands, on their keys, in the window menu's own order` |
| `qt_prompt_macro_chips` | `AC07 the store gives the name back after a restart` |
| `qt_scoped_prose_editor` | `whitespace has the selection's tooltip` |

How they were run: ctest's network sandbox cannot start a process in an SSH
session (0xc0000142). So each executable ran directly with the test's
`ENVIRONMENT` and `ENVIRONMENT_MODIFICATION`, under its pass and fail
expressions. Several of these courses start processes or restart a store, so
the direct run may be the cause rather than the product. The eighth,
`qt_source_search_panel_scale_1`, passes on main with ctest's environment.
It is reported separately (see BPR-2200).

## Scope and acceptance

- For each entry, find whether it fails under ctest from a Windows desktop
  session as well, or only in a direct run over SSH. Fix a product or test
  fault. If the fault is the harness, record the run that passes.
- Split into one card per cause when the causes differ.

## Reproduce on the Windows station

As BPR-2200's card describes: build main's targets in `windows-dev`, Release,
then run each executable directly with ctest's environment, which
`ctest --show-only=json-v1` lists.

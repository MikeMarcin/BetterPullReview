---
schema: "implement-rfc.card.v1"
id: "BPR-2205"
title: "Seven Windows Qt courses fail on main, run directly with ctest's environment"
status: "Done"
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

## Settled (2026-11-15)

ctest could not be run from a non-SSH session. The station is reachable only
over SSH, and no desktop may be driven. So every course ran directly with
ctest's environment, over SSH, before and after each fix. None of the seven
proved to be an SSH artefact. Each is a test that was wrong on Windows or
went stale on every platform, and none fails only over SSH.

| Entry | Cause | Fix (`787aaff66`) |
|---|---|---|
| `qt_agent_finding_cards_restart`, `qt_prompt_macro_chips` | The tests made their root with `QTemporaryDir`. On Windows that inherits the temporary directory's shared access, and the stores rightly refuse a root that is not owner-private (`open_state_directory` → `validate_private_security`). Nothing was written, and `load()` returned nothing. Not SSH: the folder's owner over SSH is the user, and ctest from a desktop makes the same folder. | Both use `bpr::tests::make_scratch_directory` and a `scratch_guard` (owner-private on Windows, resolved on macOS), and link `BetterPullReview::Platform` (and `Advapi32` on Windows) |
| `qt_agent_setup_view` | The card shows the program with native separators on Windows (`\opt\work\bin\wrapper`), as it should; the test expected `/` | `QDir::toNativeSeparators`, as the same test already does for folders |
| `qt_agent_steering_fixture` | The build copies the fixture in as `claude.exe` on Windows (`src/app/CMakeLists.txt`); the test looked for `claude` | `claude.exe` on Windows |
| `qt_scoped_prose_editor` | Stale on every platform (failed on macOS too): `1257a42c7` reworded the tooltip to "...; only the selection changes." | The test looks for the current wording |
| `qt_claude_process_wrapper` | Stale on every platform (failed on macOS too): [[BPR-2138]] (`726078731`) runs a tool-free request on a resumed path in the folder its host keeps. The test's writing request left the default Agent tab path, which is resumed, so it ran in the working directory instead of a fresh `bpr-writing-` folder. Every shipped tool-free request names its own path. | The test's writing request names `enhance_writing`, a fresh path, and still checks [[BPR-1375]]'s fresh folder, made and removed |
| `qt_profile_management_window` | No longer on the lane: the lane's Profiles page work (BPR-2198) removed the window and its test. It fails only on main until the lane lands. | None needed on the lane |

Verification, all after the fixes:

- Windows station, Release, Segoe UI 9 pt, run directly over SSH with
  ctest's environment: `qt_agent_finding_cards_restart`,
  `qt_agent_setup_view`, `qt_agent_steering_fixture`,
  `qt_claude_process_wrapper`, `qt_prompt_macro_chips` and
  `qt_scoped_prose_editor` pass.
- macOS `dev`: the same six pass under ctest.

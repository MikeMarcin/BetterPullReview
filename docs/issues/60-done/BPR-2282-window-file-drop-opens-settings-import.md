---
schema: "implement-rfc.card.v1"
id: "BPR-2282"
title: "Dropping a settings profile export or prompt pack onto the window opens its import dialog"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"enhancement","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2282--BPR-2283--8236500d0a43.json","related_cards":["BPR-2198","BPR-2149","BPR-2283"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"89ab71a51","evidence":{"kind":"command","detail":"qt_window_file_drop: one file over an editor is accepted with the highlight over the window, gone on leave; text drag stays the editor's; two files refused, said once; another window's drag ignored"}},{"criterion":"AC2","outcome":"pass","revision":"89ab71a51","evidence":{"kind":"command","detail":"application_window_file_drop_public_path: a dropped settings file and a dropped prompt pack open agent_configuration_import_dialog over the window, preloaded with the file's agent and pack"}},{"criterion":"AC3","outcome":"pass","revision":"89ab71a51","evidence":{"kind":"command","detail":"application_window_file_drop_public_path: after Cancel the pack prompt still plans Added; after Import it plans Unchanged and the status bar says Imported into this profile"}},{"criterion":"AC4","outcome":"pass","revision":"89ab71a51","evidence":{"kind":"command","detail":"application_window_file_drop_public_path: a JSON file that is not Profile Settings says The file is not a BetterPullReview Profile Settings file. Nothing was imported. and opens nothing"}},{"criterion":"AC5","outcome":"pass","revision":"89ab71a51","evidence":{"kind":"command","detail":"qt_window_file_drop (modal QDialog) and application_window_file_drop_public_path (a drop over the open import dialog opens no second dialog)"}},{"criterion":"AC6","outcome":"pass","revision":"89ab71a51","evidence":{"kind":"command","detail":"architecture_boundaries, qt_agent_configuration_dialogs, application_profile_settings_export_public_path, clang-format 22 and clang-tidy on the new files, verify_documentation"}}]
---

# BPR-2282: Dropping a settings profile export or prompt pack onto the window opens its import dialog

## Asked

Owner, 2026-11-19: "Can we make it so that if you drag and drop a setting
profile export file onto the app window it brings up the import dialog for
it."

A prompt pack rides the same `bpr.profile-settings` file (version 2,
[[BPR-2149]]), so a pack and a settings export are one kind of drop and open
the one import dialog.

## Acceptance

- AC1. Every workbench window accepts a drag of exactly one local file and
  shows a quiet drop highlight while it hovers. Anything else (no file, a
  remote URL, two or more files) shows the not-accepted cursor; two or more
  files also say "Drop one file at a time."
- AC2. Dropping the file reads it through the same host path Import Profile
  Settings… uses (off the UI thread, bounded, identified by its
  `bpr.profile-settings` header, not its extension) and opens the same import
  dialog preloaded with it, over the window it was dropped on.
- AC3. Nothing is imported until the dialog is confirmed; Cancel imports
  nothing.
- AC4. A file that is not Profile Settings says why, non-modally, in the
  window's status bar; nothing opens.
- AC5. A drop while a modal dialog is open is ignored.
- AC6. The Qt widget performs no filesystem effect; `architecture_boundaries`,
  `coding-format-check` and `verify_documentation.py` pass.

## Done (2026-11-19)

Built at `89ab71a51` on `claude/v0-5-0-prerelease-targets-52265d`. No look was
asked for (the coordinator's assignment: "No code review; land on main").

- Qt `bpr/qt/window_file_drop.hpp`: an application drag filter per window,
  the highlight, the several-files refusal and the modal refusal. Qt parses
  no address: the composition's `dropped_file` reads the drag's URLs.
- Composition: `import_agent_configuration(path)` reads a dropped file
  through `agent_configuration_host::read`, opens the dialog over the
  window and says refusals and outcomes in its status bar.
- A prompt pack is the version 2 Profile Settings file ([[BPR-2149]]), so it
  is the same drop and the same dialog.
- A directory or unreadable file is only known once the host reads it, so it
  is taken at the door and refused after the drop ("could not be read").

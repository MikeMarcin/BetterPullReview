---
schema: "implement-rfc.card.v1"
id: "BPR-2282"
title: "Dropping a settings profile export or prompt pack onto the window opens its import dialog"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"enhancement","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2282--BPR-2283--8236500d0a43.json","related_cards":["BPR-2198","BPR-2149","BPR-2283"]}
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

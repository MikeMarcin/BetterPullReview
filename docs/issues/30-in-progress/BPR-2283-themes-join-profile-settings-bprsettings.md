---
schema: "implement-rfc.card.v1"
id: "BPR-2283"
title: "Themes ride the one Profile Settings file, and every export is .bprsettings"
status: "In Progress"
rfcs: []
depends_on: ["BPR-2282"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"enhancement","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2282--BPR-2283--8236500d0a43.json","related_cards":["BPR-2198","BPR-2149","BPR-2282"]}
---

# BPR-2283: Themes ride the one Profile Settings file, and every export is .bprsettings

## Asked

Owner, 2026-11-19: "Can we unify themes into the same thing. And can we
change the extension to .bprsettings."

## Acceptance

- AC1. A theme is a section of the `bpr.profile-settings` format (version
  bumped by its rules; older versions still import).
- AC2. Exporting a theme from Appearance writes a `.bprsettings` file holding
  only the theme section; Export Profile Settings… can include the theme.
- AC3. Every export writes `.bprsettings`; open dialogs filter
  `*.bprsettings` and still accept legacy `*.json` and `*.bprtheme`.
  Content, not the extension, decides what a file is; a drop ([[BPR-2282]])
  accepts all three.
- AC4. Importing goes through the one import dialog, which shows the
  sections the file carries and imports only what is confirmed. A legacy
  `.bprtheme` file is read by a legacy reader and shown in the same dialog.
- AC5. The separate theme import path is removed.
- AC6. Tests round-trip a theme-only file and a full profile with a theme,
  and import a legacy `.bprtheme` and a legacy `.json`; docs updated;
  `architecture_boundaries` and `verify_documentation.py` pass.

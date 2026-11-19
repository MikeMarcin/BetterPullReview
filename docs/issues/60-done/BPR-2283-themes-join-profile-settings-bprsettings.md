---
schema: "implement-rfc.card.v1"
id: "BPR-2283"
title: "Themes ride the one Profile Settings file, and every export is .bprsettings"
status: "Done"
rfcs: []
depends_on: ["BPR-2282"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"enhancement","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2282--BPR-2283--8236500d0a43.json","related_cards":["BPR-2198","BPR-2149","BPR-2282"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7ab868a24","evidence":{"kind":"command","detail":"core_agent_configuration test_theme_section and test_one_format: Theme section round-trips; version 3 only, version 4 refused as newer, versions 1 and 2 and a .bprtheme document refused as not Profile Settings; core_prompt_pack, core_workflow"}},{"criterion":"AC2","outcome":"pass","revision":"7ab868a24","evidence":{"kind":"command","detail":"application_appearance_host test_export_round_trip (Appearance Export writes a Theme-only Profile Settings file); qt_agent_configuration_dialogs test_theme_section (Export ticks the theme on screen); application_profile_settings_export_public_path (the profile export carries the Theme)"}},{"criterion":"AC3","outcome":"pass","revision":"7ab868a24","evidence":{"kind":"command","detail":"qt_appearance_settings_panel (Export offers dark.bprsettings); composition export and open dialogs filter *.bprsettings only; application_window_file_drop_public_path drops .bprsettings files and a legacy .bprtheme and version 1 export, refused by content"}},{"criterion":"AC4","outcome":"pass","revision":"7ab868a24","evidence":{"kind":"command","detail":"qt_agent_configuration_dialogs test_theme_section (Theme listed Updated/Unchanged/Cannot be imported, swatches, Leave out imports nothing); application_appearance_theme_customization_contract and application_appearance_older_theme_contract (dialog and Cancel write nothing, Import writes once, export keeps every role and the filled ones); application_cpp_identifier_appearance (four variants imported by drop). Owner change 2026-11-19: no legacy reader, so no .bprtheme is read"}},{"criterion":"AC5","outcome":"pass","revision":"7ab868a24","evidence":{"kind":"command","detail":"appearance host import_theme_file, cancel_theme_import, ThemePreview, theme_import_rejected, theme_document_unavailable, import_appearance_theme_intent, the document port read and the panel's preview_import/request_theme_import removed; build and architecture_boundaries pass"}},{"criterion":"AC6","outcome":"pass","revision":"7ab868a24","evidence":{"kind":"command","detail":"fixtures migrated (contrast-safe-theme and older-theme .bprsettings, synthetic-org and team-agents .bprsettings); qt and core/sqlite suites 403/405 (2 failures in BPR-2281's Author description keys and agent chips, not this change); architecture_boundaries, clang-format 22, clang-tidy on changed files, verify_documentation"}}]
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

## Owner change, 2026-11-19

"don't keep legacy support. Drop import of old .bprtheme files and the old
.json profile export entirely: no legacy reader, no legacy filters. Only the
new .bprsettings format (with sections) is accepted, by dialog and by drop."
AC3 and AC4's legacy clauses are replaced by that: a  document and
a Profile Settings file before version 3 say "The file is not a
BetterPullReview Profile Settings file."

## Done (2026-11-19)

Built at `7ab868a24` on `claude/v0-5-0-prerelease-targets-52265d`. No look was
asked for (the coordinator's assignment: "No code review; land on main").

- Core: version 3 only; Theme section (base, colours by role) read by the
  theme format's rules through `import_theme_entries`; `plan_theme_import`,
  `encode_theme_settings`. The theme document codec stays as the appearance
  store's per-slot format; it is no longer a file anyone imports.
- Application: `resolve_theme` (fill from base, contrast on all variants);
  the appearance host's file import, preview publication and port read gone.
- Qt: import dialog Theme item with swatches; export dialog Theme section;
  Appearance Import… opens Import Profile Settings….
- Composition: the Profile Settings read uses the regular-file reader, so a
  FIFO or folder is refused rather than waited on (it was a plain QFile).
- A build without agent integrations has no Profile Settings import, so its
  Appearance page offers no Import… (it did offer the old theme import).

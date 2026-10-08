---
id: "RFC-DRAFT-0003"
title: "User-Owned Default Folder for Diagnostic Exports"
status: "Proposed"
---

# User-Owned Default Folder for Diagnostic Exports

## Problem and grounded current behavior

The owner reported on 2026-10-07 that exporting logs defaults to the root of
the hard drive and requested a folder such as Documents or Downloads instead.
An exported diagnostic file should start somewhere the reviewer can find and
save it without navigating away from an unsuitable system directory.

[The save chooser](../../../../../src/adapters/qt/src/release_maintenance_panel.cpp),
`release_maintenance_panel::choose_export`, sets the save mode, local-file
scheme, `.txt` filter and suggested `BetterPullReview-diagnostics.txt` filename,
but supplies no initial directory. The code therefore leaves folder selection
to dialog/platform behavior. The reported disk-root location has not been
reproduced during authoring; the absence of an explicit default is observed.

Both anonymized and identifying exports use that chooser. The
[composition controls](../../../../../src/app/src/release_maintenance_controls.cpp)
pass the chosen path to the existing local-output admission and write path.
[Current chooser tests](../../../../../tests/adapters_qt/release_maintenance_panel_tests.cpp)
check opening, cancellation and stale-preview dismissal, but not the starting
directory. [The operator guide](../../../../development/storage-and-diagnostics.md#preview-and-export-diagnostics)
requires a new local filename and preserves the exact frozen preview.

## Proposed behavior and boundaries

### Start in a familiar user folder

Every diagnostic-export Save chooser explicitly starts in the current OS
user's **Downloads** folder. If that folder is unavailable for export, use
**Documents**, then the user's **home folder**, in that order. This applies
to both privacy modes and entry through Help or Settings, including signed-out
use and a fresh application session.

Resolve these locations through the platform's standard-folder discovery;
do not construct English folder names under a guessed home path. Respect
localized and redirected locations when they satisfy the existing export
admission rules. An unavailable candidate includes an empty location, a
missing directory or a directory the current export policy cannot admit or
write to. Selecting a default creates no directory or file and changes no
permissions. Discovery examines only these candidate locations.

Supply the chosen directory explicitly before presenting the chooser, along
with the existing suggested filename and suffix. Launch directory, process
working directory, installation location and unrelated file-dialog history
must not determine the default. Never substitute a volume root, application
directory or private profile/log directory for an unavailable user folder.
If all three candidates are unavailable, show a concise explanation beside
Save and preserve the preview for retry after the location becomes available;
do not open a chooser in an arbitrary fallback directory.

### The reviewer still chooses the output

Within the open chooser the reviewer can navigate to another destination and
edit the filename normally. The default is an initial suggestion, not authority
to save automatically and not a restriction on explicitly selected locations.
Cancellation writes nothing and keeps the frozen preview, as it does today.
Existing-file refusal, local-only output, identifying-detail confirmation and
stale-preview handling retain their current contracts.

Do not turn this fix into a new Settings preference or a persistent directory
history. Each new invocation starts with the defined user-folder policy. If a
location becomes unavailable after selection, the existing output admission
and failure path remains authoritative; folder discovery does not promise
that a later write will succeed.

### Ownership and native presentation

The application/composition boundary resolves and validates candidate
locations and supplies the selected initial path or typed unavailability to
the Qt presentation. Qt sets and presents the native save chooser. Neither
core nor the widget discovers directories or performs filesystem checks or
writes. The existing host/output adapter still admits the exact selected path
and writes only the frozen preview under
[the maintenance contract](../../../../architecture/release-maintenance.md).

Use the existing native dialog and wording. No additional consent step or
redesigned export screen is needed. This proposal uses
`frontend-design:frontend-design` and preserves the repository's
[Qt presentation rules](../../../../../src/adapters/qt/AGENTS.md).

## Acceptance

- **AC01:** Through the composed Help and Settings export paths, both privacy
  modes open Save in the OS-reported Downloads directory with the existing
  filename, including when the application is launched from a disk root or
  another working directory and no PR/account session exists.
- **AC02:** Controlled unavailable-folder cases select Documents, then home;
  localized/redirected eligible locations are honored, and complete
  unavailability preserves the preview without an arbitrary/root fallback.
- **AC03:** An explicitly chosen alternate destination receives the exact
  previewed bytes through the existing output path; cancellation, an existing
  file and stale preview cannot cause an unintended write or overwrite.
- **AC04:** Folder discovery and chooser opening perform no directory/file
  creation or permission changes, preserve the architecture boundaries, and
  the actual native Save chooser shows the proposed location on each claimed
  platform. Automated chooser-state checks alone do not establish native
  dialog behavior.

## Handoff

This is an unadopted authoring draft, separate from live RFCs and issue cards.
The owner requested a user-folder default; choosing Downloads first and the
fallback order is this proposal's concrete design, not a recorded acceptance.
There are no material unanswered choices for this bounded fix.

The change adds a destination convention to RFC-BPR-0027's existing export
behavior without changing its accepted privacy, content, retention or write
requirements. Update the export operator guide when implementing. The live
RFC and its historical cards/plan remain unchanged during authoring; existing
acceptance does not claim the new default. Live registration, issue allocation,
implementation and platform evidence belong to the later authorized workflow.

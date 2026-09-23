---
schema: "implement-rfc.card.v1"
id: "BPR-1030"
title: "Windows: sqlite_release_maintenance_files and application_release_maintenance_public_path fail uncontained (export preview bytes; hostile legacy diagnostics in the native log)"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win under BPR-1020","found_at":"38cc26d7","rechecked_at":"7bf7de5b","related_cards":["BPR-1020","BPR-0900"],"related_rfcs":["RFC-BPR-0014"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1029--BPR-1031--9a8ec6194169.json"}
---

# BPR-1030: Windows: sqlite_release_maintenance_files and application_release_maintenance_public_path fail uncontained (export preview bytes; hostile legacy diagnostics in the native log)

## Defect

[[BPR-1020]] pinned both rows `WINDOWS_UNCONTAINED` for their contained
refusal, and found that they fail uncontained as well:

- `sqlite_release_maintenance_files` fails at "public host exports exactly
  preview bytes".
- `application_release_maintenance_public_path` fails at "hostile legacy
  diagnostics actually reach the selected native log".

Rechecked at `7bf7de5b` on the Windows station: both still fail at those
lines.

## Scope

- Find why the Windows export does not publish the previewed bytes.
- Find why the course's hostile legacy lines do not reach the log on Windows.
- Carry each row as far as it goes and name the next failure, if one follows.

## Acceptance

- AC1 `sqlite_release_maintenance_files` passes on the Windows station.
- AC2 `application_release_maintenance_public_path` passes its hostile-log,
  preview and export steps on Windows. The preview excludes the legacy bodies,
  and the export writes exactly the preview bytes to the non-ASCII save path.
- AC3 Any later failure in that row is named with its measured cause and
  handed to its own record.
- AC4 macOS and Linux keep their paths: every change is Windows-only code or
  a Windows-only course branch.

---
schema: "implement-rfc.card.v1"
id: "BPR-1031"
title: "Windows: the release-update courses name their data root in Qt's '/' spelling, so their own reopen of the recovery store is refused (application_release_update_public_path, release_update_recovery)"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win under BPR-1020","found_at":"38cc26d7","rechecked_at":"7bf7de5b","reserved_title":"application_attended_recent_replay and application_release_update_public_path fail on agent-workspace-recovery repair-required at quit (BPR-0875's mechanism in C++ courses)","related_cards":["BPR-1020","BPR-0875","BPR-0806","BPR-0833"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1029--BPR-1031--9a8ec6194169.json"}
---

# BPR-1031: Windows: the release-update courses name their data root in Qt's '/' spelling, so their own reopen of the recovery store is refused (application_release_update_public_path, release_update_recovery)

## The premise moved, so the title did

The ID was reserved (`15a976e8`) for "application_attended_recent_replay and
application_release_update_public_path fail on agent-workspace-recovery
repair-required at quit (BPR-0875's mechanism in C++ courses)". That was
[[BPR-1020]]'s observation at `38cc26d7`. Both rows printed
`PROFILE_QUIT_CLEANUP_SOURCE:agent-workspace-recovery:…:repair-required`, and
the update course failed at "all update shutdown flushes succeed".

At `7bf7de5b` neither half of that holds:

- Since `14a18b45` ([[BPR-0817]], recorded on [[BPR-0875]]), a refused
  recovery no longer fails the quit. The update course now prints
  `AGENT_WORKSPACE_RECOVERY_REFUSED:<temp>\agent-workspace-ownership-v1:repair-required`,
  quits with exit 0, and fails later at "both windows retain distinct text
  durably".
- `application_attended_recent_replay` passed at `c4130c3a` (25.52 s, recorded
  on BPR-0875).

This card takes the failure that is actually there.

## Defect

`application_release_update_public_path` saves both windows' draft text
through the update barrier and quits cleanly. It then opens the recovery store
itself, with `make_release_update_recovery_store(root)->load()`, and that load
fails. The drafts are on disk. The course's own open is refused.

The course builds its root as
`std::filesystem::path{ QFileInfo{…}.canonicalFilePath().toStdString() }`.
Qt spells Windows paths with '/', and the storage root validator
(`canonical_windows_root`) refuses any '/' as an invalid path. That refusal is
deliberate ([[BPR-0806]]). `bpr::app::native_filesystem_path` converts at the
point a path leaves Qt, and the application uses it, so the desktop admits the
root. The course's own reopen does not convert, so the store refuses it.
`make_storage_root_owner_private` normalises its argument first, which is why
the course's first step succeeds with the same spelling.

`release_update_recovery` builds its root the same way and fails the same
way. Registered contained, it stops earlier, at its first step.

## Scope

- Name the root natively in both courses.
- Measure `release_update_recovery` contained and uncontained, and register
  it for what it measures.

## Acceptance

- AC1 `application_release_update_public_path` passes on the Windows station.
- AC2 `release_update_recovery` passes on Windows as registered. If it is
  pinned, the pin names its measured first contained failure, and
  `test_network_isolation_manifest` reports no finding.
- AC3 macOS and Linux keep their paths: the conversions are Windows-only, or
  are the identity there.

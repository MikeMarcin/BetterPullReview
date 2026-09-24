---
schema: "implement-rfc.card.v1"
id: "BPR-1108"
title: "Windows: revert BPR-1020's four pins for rows that also failed uncontained, and leave them unpinned as BPR-0812 did"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"origin":"owner verdict on attended candidate BPR-1020-uncontained-pins-v001, session cbd48b91000b00f1e72e1ad62c7c384c, decision dcc2b167-d51d-4b47-9ccf-68b33f616640, 2026-09-24","related_cards":["BPR-1020","BPR-0812","BPR-1030","BPR-1031","BPR-0653"],"pinned_at":"41bb3229","reservation":"docs/issues/_reservations/BPR-1106--BPR-1110--72437bc3b205.json"}
---

# BPR-1108: Windows: revert BPR-1020's four pins for rows that also failed uncontained

## Why

[[BPR-1020]] (`41bb3229`) cleared the Windows manifest gate's 33 findings. It
contained two release audit rows and pinned nine rows as `dos-volume-name` in
`WINDOWS_UNCONTAINED_TESTS` (`scripts/verify_test_network_isolation.py`).
Four of those nine also failed when run uncontained at `38cc26d7`, each late in
its course:

- `sqlite_release_maintenance_files`
- `application_attended_recent_replay`
- `application_release_maintenance_public_path`
- `application_release_update_public_path`

[[BPR-0812]]'s precedent left such rows unpinned. Asked whether to keep the
four pins or revert them, the owner answered on 2026-09-24 in attended session
`cbd48b91000b00f1e72e1ad62c7c384c` (candidate
`BPR-1020-uncontained-pins-v001`, revision `41bb3229`, decision `dcc2b167`,
19:12:47Z), **select**, verbatim:

> Revert the four pins and leave those rows unpinned, as BPR-0812 did

The note is empty.

## Scope

- Remove the four entries from `WINDOWS_UNCONTAINED_TESTS`. Leave the other
  five pins and the two contained audit rows as BPR-1020 left them.
- Run `test_network_isolation_manifest` on the Windows station. The gate is
  then expected to report three findings for each of the four rows. They are
  this card's, in the way BPR-0812's two epoch rows were BPR-0812's until they
  were resolved, and not a regression.
- Since the pins were taken, the four rows' uncontained failures were worked as
  [[BPR-1030]] and [[BPR-1031]] (both Done). For each row, re-measure it
  uncontained and, by a reverted trial hook, contained:
  - if it now passes contained, register it contained, as BPR-1020 did for the
    two audit rows;
  - if it passes uncontained but not contained, record the contained cause and
    leave the row unpinned, as the owner chose;
  - if it fails uncontained, record why and leave it unpinned.
- Record each row's outcome here, with the gate's findings before and after.

## Acceptance

- AC1 The four pins are gone from `WINDOWS_UNCONTAINED_TESTS`, and nothing
  else in the pinned set changes except rows moved to contained under AC2.
- AC2 Each of the four rows has a recorded measurement on the Windows station,
  uncontained and contained. A row that passes contained is registered
  contained, and the others stay unpinned with the cause recorded.
- AC3 `test_network_isolation_manifest` on Windows reports exactly the findings
  of the rows left unpinned, three each, and no others;
  `test_network_isolation_structure_contract` passes; the same gate on macOS
  and Linux is unchanged.

## Boundaries

Test registration and the isolation script only. No product change, and no
weakening of the gate or of another row's containment.

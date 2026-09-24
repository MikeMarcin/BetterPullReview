---
schema: "implement-rfc.card.v1"
id: "BPR-1038"
title: "Windows: Purge profile refuses once the Git isolation exists, because its directories carry inherited permissions the purge inventory rejects"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"BPR-1030, application_release_maintenance_public_path on bpr-main-win","found_at":"4fd95fe5","reproduced_at":["4fd95fe5","04d22b4a"],"related_cards":["BPR-1030"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1038--BPR-1042--5b0f0a679128.json"}
---

# BPR-1038: Windows: Purge profile refuses once the Git isolation exists, because its directories carry inherited permissions the purge inventory rejects

## Defect

On Windows, a profile cannot be purged once the installed Git owner has
created its isolation directories. It creates them when it first runs Git
for the profile; in the course, that is when the diff is prepared. The two
adapters disagree about what a private directory is on Windows:

- **The Git owner creates the directories loosely.** `make_private_directory`
  (`src/adapters/git/src/installed_git_process.cpp:710`) calls
  `std::filesystem::create_directory` and then
  `std::filesystem::permissions(owner_all)`. On Windows that call only
  toggles the read-only attribute. Its check, `private_directory` (`:207`),
  asks only that the path is not a reparse point. So `home`, `xdg`, `tmp`,
  `work`, `templates`, `hooks`, `incoming` and `repositories` inherit the
  entries of the root above them.
- **The purge inventory is strict.** `held_identity(HANDLE, …)`
  (`src/adapters/sqlite/src/release_maintenance_files.cpp:859`) requires
  `win::validate_private_security` to accept the exact protected
  owner-private descriptor. An inherited descriptor fails that, and the
  inventory raises `invalid_authority`.

The purge panel shows `invalid_authority` as "The profile or prepared
selection changed". That is the same text it uses for `selection_changed`,
so the person sees a stale-selection message for a permissions refusal.

## Evidence (bpr-main-win)

[[BPR-1030]] found this. That card's two named failures are fixed, and the
course now runs further, to this step.
`application_release_maintenance_public_path` (offscreen, uncontained as
registered) passes the hostile-log check, preview and export. It signs in,
opens the fixture pull request and its diff, adds and retires a second
window, revalidates a changed inventory and clears diagnostics. Then it stops
at **`FAILED: native full-profile inventory returns`**.

- At `4fd95fe5`, a reverted `source_location` probe on `fail` located it:
  purge preparation raises `invalid_authority` from `held_identity`, because
  `validate_private_security(<root>\home, directory)` refuses the
  directory's descriptor.
- At `04d22b4a`, 6 runs: 4 stop at this step. The other 2 stop earlier at the
  epoch step ([[BPR-1040]]). The logs are station records under
  `build/logs/bpr-1038-1041-main-win/maint-*.log` and are not committed.

## Scope

- Decide which side moves. This is a security-policy question. Either the Git
  owner creates each directory with the protected owner-private descriptor
  the storage policy uses elsewhere, through `windows_private_storage.hpp`, or
  the purge inventory admits the Git isolation's directories under a stated,
  narrower rule. The first keeps one meaning of "private" on Windows.
- If the Git owner changes, existing profiles already hold loose
  directories. Say how purge treats them: repair, admit, or refuse with a
  message that names the cause.
- Give `invalid_authority` its own text in the purge panel, so this refusal
  no longer reads as a changed selection.

## Acceptance

- AC1 On Windows, a profile whose Git isolation exists can be purged, and
  `application_release_maintenance_public_path` passes "native full-profile
  inventory returns" and the purge steps after it.
- AC2 The chosen rule is enforced by a check that fails when a Git isolation
  directory's descriptor does not meet it (a negative control, not
  committed).
- AC3 A purge that is refused for authority says so in its own words, not as
  a changed selection.
- AC4 macOS and Linux keep their paths: POSIX `private_directory` and the
  purge inventory are unchanged there, or any shared change is run on those
  platforms.

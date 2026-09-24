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

## Decided: the inventory admits exactly the inherited owner-private descriptor, and says so when it refuses (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`, `build/dev` (MSVC 19.51 Debug, Qt 6.11.2),
serial and offscreen. The logs are station records under
`build/logs/bpr-1038-main-win/` and are not committed.

**Which side moves: the inventory, under an exact rule.** Moving the Git owner
alone cannot work. Protecting the owner's own directories would leave the ones
Git creates inside them, the repository's `objects`, `refs` and pack
directories, with the entries they inherit. The inventory walks into those as
well. A second creator whose descriptors this application does not write is
the case the rule has to cover.

So `validate_private_security`, when its caller allows inheritance, now
admits a directory as it already admitted a file. The descriptor must be
unprotected and must hold exactly the owner-private entries inherited from the
parent: the user, and the package SID when contained, each `FILE_ALL_ACCESS`,
flagged `INHERITED_ACE | OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE`, with the
user as owner. Any other entry, trustee, mask or flag still refuses the
directory. The purge inventory is the only caller that allows it for a
directory, in `held_identity`.

Why this is as private as the protected descriptor: the walk admits each
entry only after its parent, from a root that must be protected, and the rule
compares the entries themselves. An inherited descriptor that grants anyone
else, or grants the owner less, is refused. The one thing it does not pin is
that a later change to the parent's descriptor would flow down to it. The
parent is itself admitted only in the same exact form, in the same walk.

This keeps one meaning of "private" on Windows: exactly the owner's full
access, and nobody else's. The stores, the catalog and every other caller keep
requiring the protected descriptor they write.

**Existing profiles.** Nothing to repair. The directories that the Git owner
and Git already created are the inherited kind, and they are now admitted as
they are.

**The refusal says what it is (AC3).** A new reason,
`release_maintenance_reason::storage_not_private`, is raised when an entry's
descriptor is neither form. The core accepts it as an operational failure, the
application host passes it through rather than folding it into
`selection_changed`, and the purge panel says: "A file or folder in this
profile is not private to your account, so nothing was prepared or deleted."
Every other refusal the files adapter reports as `invalid_authority` (a path
that moved, an identity that changed, a second link) is a changed selection
and keeps its text.

**The checks, each with a negative control (AC2).**

| Row | What it proves | Negative control, reverted |
| --- | --- | --- |
| `sqlite_release_maintenance_files` | New Windows-only case: `home`, `xdg`, `tmp` and `work` made with `create_directory`, and `repositories/v1` nested two deep, as the owner and Git make them. The purge inventory admits them. One further entry on `home` (`Everyone` read) is refused as `storage_not_private` | Directories kept strict in the validator: `FAILED: inherited owner-private isolation directories are inventoried` |
| `application_release_maintenance` | A native `storage_not_private` keeps its reason through the host | The host folding it back into `io_failure`: `FAILED: a storage privacy refusal keeps its own reason through the host` |
| `qt_release_maintenance_scale_100` | A purge preparation refused for privacy fails under that reason, and the panel says "not private" and not "changed" | The panel text removed: `FAILED: the panel names a refusal for privacy, not a changed selection` |
| `core_release_maintenance` | The core takes `storage_not_private` as an operational failure before commit, with no recovery needed | (covered by the host and panel controls, which go through the core) |

All four pass with the change (`units-fix2.log`).

## Past the inventory, two more Windows refusals in the purge itself; the course passes (bpr-main-win, 2026-09-23)

With the inventory admitted, `application_release_maintenance_public_path` got
past "native full-profile inventory returns" and "complete real profile can be
purged". It then stopped at "exact profile purge completes through real
Settings", with "Cleanup may be partial. Recovery is required". The course now
prints the panel's status and the entries left behind when that step fails,
as it already did for the inventory. Reverted traces named two causes, one
after the other.

1. **Git's packs are read-only.** Git for Windows writes `pack-*.pack` and
   `pack-*.idx` with `FILE_ATTRIBUTE_READONLY`. This checkout's own
   `.git/objects/pack/*.pack` show `R` too. The purge's `remove_exact` set the
   plain `FileDispositionInfo`, which refuses a read-only file, so the purge
   stopped at the first pack. It now sets `FileDispositionInfoEx` with
   `FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE`.
   That deletes the file as it is, without first clearing the attribute, which
   would change it even if the delete then failed. Deletion otherwise keeps
   its old semantics, not POSIX's.
2. **`state/v1` is held open.** The desktop still holds the shared registry
   and updater stores, which the purge keeps. POSIX answers the removal of
   `state/v1` with "not empty", which the purge already accepts for exactly
   `state` and `state/v1`. Windows refuses the `DELETE` handle before that, with
   `ERROR_SHARING_VIOLATION` (traced: `open ...\data\state\v1 access=10000
   sharing=1 error=32`). So those two directories are now kept without being
   opened for deletion while they still hold anything. An empty one is still
   removed.

**AC1: the course passes on Windows,** in 19.11 s and again, without any
trace, in 14.33 s: inventory, purge, the purged stores gone, the exported
file kept, orderly shutdown, the lease released, and the startup recovery
refusal after it.

**The checks for the purge fixes.** `sqlite_release_maintenance_files`'s
populated-cache case now makes its synthetic pack read-only on Windows, as
Git does, and still purges it. Negative control: the plain delete flag alone
fails it, `FAILED: confirmed complete profile removes exact cache objects`.
Negative control for `state/v1`: without the rule, the course stopped at the
purge on the sharing violation above.

**macOS and Linux (AC4).** Every change on their paths is limited to what an
enum addition needs:

- `validate_private_security`, `held_identity`'s Windows branch,
  `remove_exact`'s Windows branch and the read-only fixture step are all
  Windows-only.
- The new reason is raised only on Windows.
- The core, the application host and the panel gain one enumerator, one
  operational failure and one text. No POSIX refusal changes, and POSIX
  `private_directory` and the POSIX inventory are untouched.
- The new unit cases, in the core, the host and the panel, run everywhere and
  need a run on bpr-main-mac and bpr-main-linux:
  `ctest --preset dev -R '^(core_release_maintenance|application_release_maintenance|qt_release_maintenance_scale_100|sqlite_release_maintenance_files|application_release_maintenance_public_path)$'`.

`clang-tidy` on the eight changed translation units reports nothing on a
changed line. The only findings are the naming style of
`windows_private_storage.hpp`, which is snake_case throughout. They appear
because the SQLite test includes that header by relative path. They are older
than this card, and the two locals it touches keep the file's own style.

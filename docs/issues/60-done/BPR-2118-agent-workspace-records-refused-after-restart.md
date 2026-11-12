---
schema: "implement-rfc.card.v1"
id: "BPR-2118"
title: "After a Mac restart, every retained agent workspace record is refused as unreadable, because each record stores the volume's mount-time device number"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-11: after a restart, opening BPR showed 'Disposable agent workspaces are unavailable: the workspace that record …/agent-workspace-ownership-v1/6b24f4dab44ea68a8e1f41c6709393ea.jsonl names could not be read back or cleaned up exactly, so it is kept.' All 14 records in the folder were intact and refused for the same reason","related_cards":["BPR-0794","BPR-1790","BPR-1857","BPR-1870"],"related_rfcs":["RFC-BPR-0033"],"reservation":"docs/issues/_reservations/BPR-2118--BPR-2118--431be47008ae.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4e36d27198ec33fce09c5d663d3583bb2fd81826","evidence":{"kind":"command","detail":"Simulated remount (macOS, scratch roots only): a retained phase-7 record rewritten as Version 1 with every Volume on one non-live mount number recovers as live=1 and is rewritten to the exact Version 2 bytes with the same inode; in git_process_contract the retained workspace, journal and the conversation's stored identities rewritten the same way recover live, and reattach accepts the old stored identities and returns the journal's durable ones (another inode refused). The launcher's guard check uses the same durable identity (qt_agent_launch_canary). A real Mac restart was not performed.","command":"ctest --preset dev -R '^(agent_workspace_ownership|git_process_contract|application_agent_invocation_host|qt_agent_launch_canary)$'"}},{"criterion":"AC2","outcome":"pass","revision":"4e36d27198ec33fce09c5d663d3583bb2fd81826","evidence":{"kind":"command","detail":"bpr/platform/durable_volume_identity.hpp is the one ATTR_VOL_UUID reader; installed_git_cache's manifest_identity, the ownership journal (ownership_id, journal_stat), the guarded Git launch and the agent launcher use it. git_pristine_remount_recovery checks the cache manifest's persisted lock identity equals the helper's for the same file; git_process_contract checks a retained workspace stores the UUID form and the guard equals the helper's.","command":"ctest --preset dev -R '^(git_pristine_remount_recovery|git_process_contract)$'"}},{"criterion":"AC3","outcome":"pass","revision":"4e36d27198ec33fce09c5d663d3583bb2fd81826","evidence":{"kind":"command","detail":"Version 1 on one mount after renumbering is accepted and rewritten in place (pwrite from 0, ftruncate, fsync on the same descriptor, inode kept). Two recorded volumes refuse as record-moved other-volume; a changed Registry inode refuses as record-moved replaced; refused records are not rewritten; the existing negative courses (phase, scope, authority, marker, substitution, hardlink, trailing, oversize) still refuse. Negative control: forcing the one-mount rule to refuse made the remount course fail with record-moved other-volume.","command":"ctest --preset dev -R '^agent_workspace_ownership$'"}},{"criterion":"AC4","outcome":"pass","revision":"4e36d27198ec33fce09c5d663d3583bb2fd81826","evidence":{"kind":"command","detail":"record_unreadable split into record_malformed, record_moved (finding missing/other_volume/replaced) and record_cleanup_failed; recovery checks every record before refusing and reports Count and the first path (three moved records -> records=3 with one of their paths); the terminal line gains :records=N; agent_composition sentences are distinct per check and finding and count all records.","command":"ctest --preset dev -R '^(agent_workspace_ownership|agent_composition_refusal_text)$'"}},{"criterion":"AC5","outcome":"pass","revision":"4e36d27198ec33fce09c5d663d3583bb2fd81826","evidence":{"kind":"observation","detail":"Code review of the platform branches: on Linux ownership_id and journal_stat produce {kind, st_dev, 0, ino} exactly as before and on Windows the identity code is untouched; a Version 1 line must equal the live identities exactly off macOS and only its Version changes. The ownership course's Linux/Windows branches of the new remount checks follow that path. Not executed on Linux or Windows in this round."}},{"criterion":"AC6","outcome":"pass","revision":"4e36d27198ec33fce09c5d663d3583bb2fd81826","evidence":{"kind":"command","detail":"architecture_boundaries and architecture_boundary_contract passed; coding-format-check (clang-format 22.1.8) passed over 2189 files; application_review_round_trip_composition_contract static pins pass after the BPR-2118 manifest reseal (its runtime probe needs the full app and was not run); verify_documentation reports only the NEXT_ID high-water error BPR-2119 vs BPR-2120 from other branches.","command":"ctest --preset dev -R '^(architecture_boundaries|architecture_boundary_contract)$'; cmake --build --preset dev --target coding-format-check; python3 scripts/verify_documentation.py --root ."}}]
---

# BPR-2118: After a Mac restart, every retained agent workspace record is refused as unreadable, because each record stores the volume's mount-time device number

## Observed

The owner, 2026-11-11, on macOS, opening BPR about 25 minutes after the Mac
restarted. Home reported:

```text
Disposable agent workspaces are unavailable: the workspace that record
~/Library/Application Support/BetterPullReview/BetterPullReview/agent-workspace-ownership-v1/6b24f4dab44ea68a8e1f41c6709393ea.jsonl
names could not be read back or cleaned up exactly, so it is kept. Move the
record and its workspace out of their folders, then restart.
```

Nothing had been moved, edited or shared. The record folder held 14 records
written between 2026-11-09 and 2026-11-10. Each one had a matching
`betterpullreview-agent-<token>` folder in
`BetterPullReview-agent-workspaces-v1/`. Every record had the same
well-formed history, phases `0, 1, 2, 3, 7`, so each was a retained
conversation workspace (phase 7).

Every inode in the records still matched the disk. The only field that
differed was the volume number:

| Identity | Recorded | On disk now |
|---|---|---|
| Record folder (`Registry`) inode | 61426300 | 61426300 |
| Workspace folder (`Neighbourhood`) inode | 61426298 | 61426298 |
| Record file `6b24f4da….jsonl` (`File`) inode | 115856686 | 115856686 |
| **Volume, all three** | **16777229** (`0x100000D`) | **16777233** (`0x1000011`) |

All 14 records were in the same state. The message named `6b24f4da…` only
because it was the first record the directory scan reached. Following the
message's advice would have taken 14 restarts, moving one record and one
workspace aside each time, and every retained conversation workspace would
have been lost to the app along the way.

## Cause

1. `exact_identity` (`src/adapters/git/src/installed_git_process.cpp`) builds
   `exact_artifact_identity{ Kind, Volume = st_dev, 0, ObjectLow = st_ino }`
   on Unix. `workspace_directory_identity` stores it as the four-number
   `Opaque` array. `journal_stat` builds the record file's `File` identity
   the same way, again from `st_dev`.
2. On macOS, `st_dev` identifies a mount, not a volume. The kernel hands out
   APFS device numbers when the volume mounts, in mount order. A restart, or
   a disk or disk image attached in a different order, can renumber the
   Data volume even though no file changed. Here it moved from
   `0x100000D` to `0x1000011`.
3. `load_workspace_record` requires each line's `Registry`, `Neighbourhood`
   and `File` to equal `ownership_id(…)` / `journal_stat(…)` of the live
   paths. After a renumbering, every line of every record fails, and the
   function returns `std::nullopt`.
4. `recover_workspaces_locked` treats that as
   `workspace_refusal_check::record_unreadable` with `repair_required` and
   stops at the first record. Phase 7 records are checked by
   `load_workspace_record` *before* the `Phase == 7` early-out, so even
   retained workspaces that recovery would leave alone block startup.
5. The same stored identities drive the rest of the lifecycle. They would
   fail the same way once loading got past them:
   - `directories_match` and `remove_planned` (cleanup) compare live
     `Parent` / `Child` against the record.
   - Reattaching a retained conversation workspace (the `Phase != 7U ||
     record->Child != aWorkspace.DirectoryIdentity.Opaque` check, and the
     `LifetimeGuard->FileIdentity` check before it) compares against
     identities the retained conversation persisted.
6. The message blames the wrong thing. "Could not be read back or cleaned up
   exactly" covers a parse failure, a broken phase transition, an identity
   mismatch and a failed cleanup, and it reports only one record.

This problem has already been fixed once, in another adapter. For the
retained Git cache manifest, [[BPR-0794]] (`cdfcce727`, "Recover pristine
remounted Mac caches") added `manifest_identity` to
`src/adapters/git/src/installed_git_cache.cpp`:

```cpp
// st_dev names a mount, not a durable volume. Keep it for live
// descriptor checks; persist the volume UUID and lock inode.
```

That function reads `ATTR_VOL_UUID` with `fgetattrlist` and persists
`{ Kind, uuid[0..8), uuid[8..16), inode }`. The agent workspace ownership
journal was written later and did not reuse it.

Windows is not affected the same way: its `exact_identity` uses
`dwVolumeSerialNumber`, which is set when the volume is formatted and does not
change across restarts.

## Change

### 1. Persist a durable volume identity on macOS

- Move the `ATTR_VOL_UUID` read out of `installed_git_cache.cpp`'s
  `manifest_identity` into one helper that both Git adapter files can use,
  `durable_identity(descriptor-or-path, kind)`. On macOS it returns
  `{ Kind, uuid_low, uuid_high, st_ino }`. Everywhere else it returns
  `exact_identity` unchanged. Keep exactly one implementation:
  `installed_git_cache` calls the helper instead of its own copy.
- A zero UUID, a short attribute buffer, or a descriptor whose `st_dev` /
  `st_ino` changed between the `fstat` and the `fgetattrlist` returns
  `std::nullopt`, as `manifest_identity` does today.
- Every identity the ownership journal **stores**, and every comparison
  against a stored one, uses the durable form:
  - `Registry`, `Neighbourhood`, `File`, `Parent`, `Child` when a line is
    written (`advance`, journal creation) and when it is read
    (`load_workspace_record`, `directories_match`, `remove_planned`, the
    phase 1 and phase 4 `Parent` checks in `cleanup_locked`).
  - The `DirectoryIdentity` and `LifetimeGuard->FileIdentity` a retained
    conversation persists and the reattach path compares.
- Checks within one process that compare two live `stat` results
  (`held.st_dev == named.st_dev`, the open-then-`lstat` races) keep using
  `st_dev`. Within a single mount it is correct and cheaper.

### 2. Read existing records once and upgrade them

Records already on disk hold `st_dev`. Discarding them would throw away every
retained workspace a second time, so read them and upgrade them:

- Bump the record `Version` to `2` for lines that store durable
  identities.
- When reading a `Version: 1` line on macOS, compare `Kind` and the inode
  exactly. Accept a `Volume` that differs from the live `st_dev` only when
  all of these hold:
  - every identity in the whole record (all lines, all five fields) carries
    the same recorded `Volume`, so the record was written on one mount;
  - the live registry, neighbourhood and record file all share one live
    `st_dev`, so they are on one mount now;
  - every other check (`Private`, owner, link count, token, phase chain,
    `Head`, `Scope`, fingerprints) passes unchanged.
- After a Version 1 record loads, rewrite it in place under the exclusive
  journal lock as the same phase chain with Version 2 lines, fsync it, and
  continue. Truncate and write the same descriptor rather than renaming:
  `File` is the journal's own inode and must not change.
- Upgrade the retained conversation's persisted `DirectoryIdentity` /
  `LifetimeGuard->FileIdentity` the same way the first time a retained
  workspace reattaches after this change. Accept a mismatch only in the
  volume part, and only when the journal that names it has just been
  accepted under the rule above.
- A Version 1 record that fails any other check is refused exactly as it is
  today.

On Linux and Windows, Version 1 and Version 2 lines hold the same values, so
the upgrade only changes the version number.

### 3. Report each refusal precisely and all at once

- Split `record_unreadable` into:
  - `record_malformed`: the line does not parse, the phase chain is
    invalid, or the token does not match;
  - `record_moved`: an identity does not match the live path;
  - `record_cleanup_failed`: `cleanup_locked` returned false.
  Keep the `record-unreadable` diagnostic name only as an alias, if
  existing log readers need it.
- Check every record before refusing. Report the count and the first path,
  for example: "14 workspace records could not be matched to their folders
  (first: …/6b24f4da….jsonl)". That way the user knows what to move before
  restarting, not one record per restart.
- For `record_moved`, say what was found. "The record names a folder with
  the same inode on a different volume" does not mean the same as "the
  folder is gone".

### 4. Tests

- `git_contract` agent-workspace tests, on macOS:
  - Write a retained (phase 7) workspace, quit, and rewrite every `Volume`
    in its Version 1 record to another single value, standing in for a
    remount. Recovery accepts it and reports one `Live`, the record now
    reads as Version 2, and reattach succeeds.
  - The same with two different recorded volumes inside one record is
    refused as `record_moved`.
  - The same with one inode changed is refused as `record_moved`.
  - A Version 2 record round-trips unchanged across recovery.
  - The `ATTR_VOL_UUID` helper and `installed_git_cache`'s manifest give
    the same identity for the same file, and the existing BPR-0794 remount
    tests still pass.
- Recovery with three records whose identities do not match reports the
  count and the first path in one refusal.
- `agent_composition` text test: each new check has its own sentence.

### 5. Documentation

- RFC-BPR-0033 (agent workspace ownership): stored identities use the
  durable volume identity, the Version 1 → 2 upgrade rule, and the new
  refusal checks.
- The troubleshooting text for "Disposable agent workspaces are
  unavailable": a restart alone must never cause it, and what to do when it
  appears.
- [[BPR-1790]] / [[BPR-1857]]: note that macOS had a second cause unrelated
  to Windows ACLs.

## Workaround until this ships

Quit BPR. Move **every** `*.jsonl` in `agent-workspace-ownership-v1/` (leave
`.git-process-boundary.lock`) and every `betterpullreview-agent-*` folder in
`BetterPullReview-agent-workspaces-v1/` into a backup folder outside both,
then restart. Nothing is deleted, but the app no longer knows about the
retained agent workspaces. Moving only the one named record just moves the
refusal to the next record.

## Acceptance

- AC1. On macOS, a retained agent workspace created before a restart that
  renumbers the Data volume is recovered as live and reattaches afterwards,
  with no refusal on Home.
- AC2. Agent workspace records store a volume identity that survives a
  remount (the APFS volume UUID on macOS). One helper produces it, and
  `installed_git_cache` and the workspace ownership journal both use that
  helper.
- AC3. Existing Version 1 records written on one mount are accepted after a
  renumbering and rewritten as Version 2 in place. A record whose inodes,
  owner, phase chain or scope do not match is still refused.
- AC4. A refusal names its specific check (malformed, moved, cleanup failed)
  and counts every affected record, not only the first.
- AC5. Windows and Linux behavior is unchanged apart from the version
  number.
- AC6. The tests above, `architecture_boundaries`, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Out of scope

- Linux device renumbering. `st_dev` on Linux is usually stable for one
  block device, but it can change with device-mapper or removable media. If
  it turns out to matter, a follow-up can use the filesystem UUID there
  (`statx` / `blkid`-style lookup) through the same helper.
- Other `st_dev` users outside the Git adapter (`local_workspace_source`,
  `release_maintenance_files`, the update adapter, `session_files`). Each
  should be checked for whether it **persists** `st_dev` across launches. If
  any does, file a separate card, since each owns its own format.
- A repair button on Home. With the volume fixed, a restart no longer needs
  one. Real tampering still needs the user's judgment.

## Done (2026-11-11)

Implemented as above, with these deviations from the Change section:

- The helper lives in the header-only platform target
  (`src/platform/include/bpr/platform/durable_volume_identity.hpp`,
  `of_descriptor`/`volume_uuid`), not in the Git adapter, because the agent
  launcher (`agent_process_launcher.cpp`) and the guarded Git launch compare
  the guard's `FileIdentity` against the live journal too; they would have
  refused every newly stored durable guard. `durable_identity` was already the
  name of the Linux inode-generation helper in `installed_git_cache.cpp`.
- The retained conversation's stored `DirectoryIdentity` and guard identity
  are not rewritten in the session store. `inspect_agent_workspace` now
  returns the workspace with the journal's durable identities and the run
  launches with those; an old stored identity is accepted on every reattach
  when kind and inode match the journal just checked against the live paths
  (macOS only). Conversations sharing one workspace are bound to each other by
  exact directory identity in `agent_setup_store`, so a partial rewrite would
  make them conflict.
- The ownership design lives in `docs/architecture/ai-agent-boundary.md`;
  RFC-BPR-0033 only owns where the refusal is reported, so it gets a dated
  note and the rules go in the architecture document.
- `record-unreadable` has no alias: no log reader used it.
- `record_not_private` refusals are counted with the others.

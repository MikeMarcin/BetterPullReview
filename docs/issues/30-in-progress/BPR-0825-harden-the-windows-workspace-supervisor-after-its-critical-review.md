---
schema: "implement-rfc.card.v1"
id: "BPR-0825"
title: "Harden the Windows workspace supervisor and recovery after BPR-0622's critical review"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"dsp-002 rfc-reviewer-critical of windows-lane 743c59d7..c94e02b1 (no blocking findings)","found_at":"2ba02ddbe59bf6e07302cbb7460f61cc9cad2627","run":"run-20260916-stations","dispatch":"dsp-002","origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred follow-up; not an RFC0013 closeout dependency","owner_decision":{"date":"2026-09-24","session":"cbd48b91000b00f1e72e1ad62c7c384c","candidate":"BPR-0825-descriptor-home-v001","decision":"12977aca-c423-404d-9ad8-07cd5b7d9c24","selected":"(c) neutral header-only target"}}
---

# BPR-0825: Harden the Windows workspace supervisor and recovery after BPR-0622's critical review

## Owner-deferred follow-up — 2026-09-21

Owner amendment r4 transfers this remaining work out of RFC-BPR-0013 terminal
acceptance. This card keeps its ID, path, status, dependencies and all historical
evidence; only the RFC0013 association is removed. The
[amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its origin and continuation.

Pending: Windows owner-private descriptor ownership, now decided as option (c) (owner, 2026-09-24, below); prior supervisor, ACE and junction repairs and their native evidence remain intact.
No owner decision, native observation or repair is supplied by this transfer.

## Origin

An independent critical review of [[BPR-0622]]'s native Windows
implementation (`743c59d7..c94e02b1`, merged into main at `2ba02ddb`) found
**no blocking finding**. It worked by source reading plus the recorded native
evidence, without running any Windows code. The items below are its
non-blocking findings and the limits it could not settle.

## Scope

1. **Supervisor error branches.**
   `src/adapters/agent/src/windows_agent_supervisor.cpp` (~135–153, ~289)
   returns failure when `TerminateJobObject` or `QueryInformationJobObject`
   fails, then exits without confirming the job empty. Process exit then
   releases the guard and triggers kill-on-close at the same moment. That is
   the same unguarded window as the recorded double kill. Retry, or keep the
   guard until emptiness is confirmed or the job handle is provably the last
   one.
2. **Parent rule ACE coverage.** `workspace_parent_trusted` in
   `src/adapters/git/src/installed_git_process.cpp` (~3290) walks only
   `ACCESS_ALLOWED_ACE_TYPE` entries. A callback or object ACE that grants
   write to another principal is not seen. The strict per-object privacy
   check covers the objects themselves, so this rule is defence in depth. Make
   it reject unknown allow-type ACEs, as `windows_private_to_current_user`
   does.
3. **Descriptor coupling.** `prepare_workspace_registry` (~3605–3618)
   requires `ApplicationDataRoot` to carry exactly the protected descriptor
   that `create_storage_root_owner_private` writes (`catalog_database.cpp`).
   Share that shape through one definition, so a change on the SQLite side
   cannot silently turn Windows workspace ownership into `repair_required`.
4. **Junction inside a quarantined tree.** No course exercises a directory
   junction inside a workspace being removed. Add a native Windows course:
   a junction to an external directory inside the materialized workspace,
   then removal or recovery. The external directory must be unchanged, and
   the junction itself must be removed or refused.
5. **Record, do not fix.** The exit-accounting note (`ActiveProcesses == 0`
   is accounting, not handle rundown; a lingering handle leads to a repair
   refusal, not data loss), the inheritable-pipe creation window, and the
   post-hoc refusal of a guarded Git start that bypassed the modifier. Keep
   them as limits in the Windows evidence file.

## Acceptance

- Items 1–4 are implemented, or explicitly declined with a stated reason,
  on the Windows station. Each change has a native course or a reverted trial
  that shows it matters.
- The BPR-0622 Windows courses still pass: `agent_workspace_ownership`,
  `git_process_contract`, `agent_parent_death`, `qt_agent_launch_canary`,
  `application_agent_crash_recovery` and
  `application_agent_materialization_crash_recovery`.
- The macOS and Linux runs of the same courses still pass if portable
  sources change.

## Windows station, 2026-09-17 (task-20260917-win-hardening)

The base is `de58ebc6`. Logs are under `build/logs/task-20260917-win-hardening/`.

**Item 1: implemented.** `terminate_and_confirm_empty` in
`windows_agent_supervisor.cpp` never returns before the job is confirmed
empty. A failed `TerminateJobObject` is retried, and so are a failed
`QueryInformationJobObject` and a non-zero `ActiveProcesses` count. The
function is now `void`, and the `!emptied` early exit is gone, so the guard is
released only after confirmation. A supervisor that can never confirm keeps
the guard, and recovery then sees a live owner and refuses.

Reverted trials, each with a persistent `TerminateJobObject` failure injected:

- **With this change:** `agent_parent_death` fails at 25.17 s with
  `guarded launch outcome 6`. The supervisor keeps its guard and never exits
  on its own; the launcher's teardown has to end it.
- **With HEAD's supervisor:** it fails at 6.87 s with `guarded launch outcome
  3`. The supervisor exits 125 immediately, and its exit releases the guard
  without any emptiness confirmation.

**Item 2: implemented.** `workspace_parent_trusted` in
`installed_git_process.cpp` skips only the four deny ACE types. Every other
type that is not `ACCESS_ALLOWED_ACE_TYPE` (object, callback, compound or
unknown) refuses the parent.

The native course is new: `agent_workspace_ownership` adds
`untrusted-object-ace`. A private root gets an `OA` entry granting Everyone
full access, set from SDDL because `icacls` cannot write one. Abandon must
return `failure=1` and create no neighbourhood.

Reverted trial: with non-allow types skipped as before, the course fails with
`neighbourhood created under a parent with a foreign object ACE`.

**Item 3: parked, pending a design decision.** One definition of the Windows
owner-private descriptor has to live somewhere both adapters can reach, and
the card does not say where. Today there are two:

- the SQLite adapter's `private_security_attributes` and
  `validate_private_security` in `catalog_database.cpp`;
- the agent adapter's header-only `windows_private_descriptor` and
  `windows_private_to_current_user`, which the Git adapter already includes.

The options:

- **(a)** The Git adapter calls a new public SQLite validator, such as
  `adapters::sqlite::storage_root_owner_private(path)`. This adds a new
  `AdapterGit → AdapterSQLite` dependency.
- **(b)** The SQLite adapter includes the agent header and builds and validates
  its roots with it. This adds `AdapterSQLite → AdapterAgent`.
- **(c)** A new neutral header-only target, for example
  `src/platform/windows_private_object.hpp`, that both adapters include.
  Architecture boundaries would need a new owner entry.

Which dependency direction, or new target, is acceptable is an architecture
decision, so nothing was changed.

**Item 4: covered by a native course; no product change was needed.**
`agent_workspace_ownership` adds `external-junction`:

1. A directory junction (`_winapi.CreateJunction`, which needs no privilege)
   inside the materialized workspace points at a user directory holding files
   and a nested directory.
2. Recovery runs.

Result: `recovered=1 live=0`. The junction and the workspace are gone, and
the user directory's listing and bytes are unchanged.

Reverted trial: `remove_planned` first walks the quarantined tree and follows
junctions (not symbolic links). The course then fails with `recovery changed
the directory a junction pointed at`.

**Item 5: recorded.** The limits are in
`docs/verification/BPR-0622-windows-native-evidence.md`.

**Acceptance run** after an incremental rebuild. The only build failure is the
pre-existing `bench_better_pull_review_large_review` C4702, reported
separately. These passed:

| Course | Time |
|---|---|
| `agent_workspace_ownership` | 9.10 s |
| `git_process_contract` | 39.12 s |
| `agent_parent_death` | 48.44 s |
| `qt_agent_launch_canary` | 104.85 s |
| `application_agent_crash_recovery` | 52.67 s |
| `application_agent_materialization_crash_recovery` | 46.80 s |
| `architecture_boundaries` | 15.31 s |

`test_network_isolation_manifest` reports only the two epoch rows
[[BPR-0812]] parked.

**Portable sources touched:** none change POSIX behaviour.

- `windows_agent_supervisor.cpp` is Windows-only.
- The `installed_git_process.cpp` change sits inside the Windows branch of
  `workspace_parent_trusted`.
- `tests/scripts/agent_workspace_ownership_tests.py` gains a Windows-only
  helper and two Windows-only courses.

## Integrated; item 3 waits on an owner decision (dsp-003)

Main merged `windows-lane` at `711d3ddc` as `902cda35`. Items 1, 2, 4 and 5
are done on Windows with native courses or reverted trials (above). Their
source changes sit inside Windows-only branches. At main `8a03a5be` on macOS,
`agent_workspace_ownership` and `git_process_contract` still pass
(`build/r2-logs/dsp3-int1-ctest.log`).

**Owner ask (item 3, architecture).** Where should the one definition of
the Windows owner-private descriptor live, so that a change on the SQLite
side cannot silently turn Windows workspace ownership into `repair_required`?

- (a) The Git adapter calls a new public SQLite validator
  (`AdapterGit → AdapterSQLite`).
- (b) The SQLite adapter includes the agent adapter's header
  (`AdapterSQLite → AdapterAgent`).
- (c) A new neutral header-only target that both adapters include, with a
  new owner entry in the architecture boundaries.

The integrator recommends (c). Adapters are narrow host capability ports,
and (a) and (b) each add an adapter-to-adapter edge for a platform
definition that neither adapter owns. Until the owner answers, the card is
parked on this question and nothing else in it is open.

## Owner decision, 2026-09-24: item 3 takes option (c) (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-0825-descriptor-home-v001`, revision
`902cda35`, fingerprint `8e0eeaec…`, decision
`12977aca-c423-404d-9ad8-07cd5b7d9c24`, sequence 573, 2026-09-24T18:56:43Z,
`actor: human`: **select**, verbatim:

> (c) A new neutral header-only target that both adapters include, with a new
> architecture-boundary owner (recommended)

The note is empty. Options (a), a Git-to-SQLite adapter edge, and (b), a
SQLite-to-agent adapter edge, are not chosen. The card leaves human-review for
**Ready** on the Windows station. Item 3's work, as the owner chose it:

- One definition of the Windows owner-private descriptor, and of its
  validation, moves into a new header-only target (for example
  `src/platform/windows_private_object.hpp`). It gets a new owner entry in
  `architecture_boundaries` and in the root `AGENTS.md` ownership table. It
  holds no adapter's state and no Qt, provider, filesystem-policy or process
  decision beyond the descriptor shape itself.
- The SQLite adapter's `private_security_attributes` and
  `validate_private_security` (`catalog_database.cpp`), and the agent
  adapter's `windows_private_descriptor` and `windows_private_to_current_user`,
  which the Git adapter already includes, are rebuilt on that one definition.
  No adapter-to-adapter include edge is added.
- `prepare_workspace_registry` in `installed_git_process.cpp` checks
  `ApplicationDataRoot` through the shared definition. A change on the SQLite
  side can then no longer turn Windows workspace ownership into
  `repair_required` silently. A reverted trial or a course shows the coupling
  holds.
- The card's acceptance applies unchanged: the BPR-0622 Windows courses pass,
  and macOS and Linux still pass if portable sources change.

## Windows station, 2026-09-24 (bpr-main-win): item 3 implemented as the owner chose

Run `bpr-main-win-20260923`. Setup:

- Windows 11 Home 10.0.26200, VS 18 MSVC 19.51, Qt 6.11.2;
- `build/dev` (Debug), and `build/core-only`;
- the base is `4d739d28`. The change is on main as `bc6328ff` (claim
  `b7a895a1`). It was rebased twice without conflict. Upstream touched
  none of its files, and on the rebased tree `architecture_boundaries`,
  `git_process_contract` and `agent_workspace_ownership` pass again.

Logs are under `build/logs/bpr-0825-main-win/` on the station and are not
committed.

### The change

**A new header-only target, `better_pull_review_platform`**
(`BetterPullReview::Platform`, `src/platform/`). It holds the one definition
of the Windows owner-private object,
`bpr/platform/windows_private_object.hpp`:

- `descriptor` builds the protected, owner-explicit descriptor: the token
  user as owner with one full-access grant, and one for the AppContainer
  when contained, with the directory's inheritance.
- `validate` is the exact check, read from the opened object. It includes
  [[BPR-1038]]'s inherited-file allowance.

The target links only `Advapi32`, depends on nothing first-party, and
includes no Qt, provider or adapter header.

**Both sides are rebuilt on it, with their call shapes unchanged:**

- The SQLite and logging side, through `src/adapters/native`.
  `private_security_attributes` wraps the platform descriptor.
  `validate_private_security` maps the platform verdict into the storage
  failure vocabulary. `windows_object_kind` is the platform kind.
- The agent side, in `windows_workspace_guard.hpp`.
  `windows_private_descriptor` wraps the platform descriptor, and
  `windows_private_to_current_user` is the platform check.

The Git adapter's `prepare_workspace_registry` still reaches
`windows_private_to_current_user` through `windows_object_private`. So it
now admits the `ApplicationDataRoot` the SQLite adapter created through the
same definition the SQLite side wrote it with. No adapter-to-adapter include
edge is added. The Git adapter's existing private link to the agent adapter
on Windows is older than this card, and is unchanged.

**Consumers link the platform target:**

- the agent adapter, `PUBLIC`, because its public guard header includes it;
- the Windows supervisor, the SQLite adapter and the logging adapter;
- the test targets that compile `installed_git_process.cpp` or include the
  native header directly.

**`architecture_boundaries` gains the owner entry**
(`cmake/VerifyArchitectureBoundaries.cmake`):

- a `verify_effect_owner` rule, "Windows owner-private descriptor
  construction". It allows `InitializeAcl`, `AddAccessAllowed*Ace*` and
  `SetSecurityDescriptor{Owner,Dacl,Control}` calls only in the platform
  header, and requires `src/platform/CMakeLists.txt` to own it;
- a neutrality check: the target must be the `INTERFACE`
  `better_pull_review_platform`, may not link a first-party, Qt or provider
  target, and its sources may not include Qt, provider or first-party layer
  headers.

`architecture_boundary_tests.py` adds two cases. Each proves a rule refuses
what it should:

- a second builder in the SQLite storage root, the agent guard or the native
  header;
- an unowned header;
- a first-party link;
- a Qt, adapter or application include;
- a non-`INTERFACE` target.

**The root `AGENTS.md` ownership table gains the `src/platform/` row.**
`docs/architecture/system-architecture.md` names the target and its rule,
and `release-maintenance.md` points the native primitives at it.

### Evidence that the coupling holds

**A new course step.** `agent_workspace_ownership` gains `sqlite-root`
(Windows):

- the course executable creates `data` with the SQLite adapter's own
  `create_storage_root_owner_private`, as the product creates its data
  root;
- it then prepares, marks ready and cleans a workspace whose registry is
  `data/agent-workspace-ownership-v1`, with a neighbourhood the Git adapter
  creates itself;
- expected: `cleaned`, and no journal or workspace left.

**A reverted trial, run on the `sqlite-root` step alone.** The trial changed
the directory inheritance to `CONTAINER_INHERIT_ACE` only:

| Tree | Result |
|---|---|
| This change, unaltered | `cleaned` |
| This change, with the trial edit made in the shared definition | `cleaned`: the SQLite root and the Git check move together |
| The agent guard header restored to `HEAD`, so its own definition is back, with the same trial edit (which then reaches only the SQLite side, as a SQLite-side change did before) | `failure=1`, which is `repair_required`. The registry was never created. This is the silent coupling the card names |

Both trial edits were reverted. The files were compared byte for byte with
the kept copies, and the tree rebuilt.

**A boundary negative control on the real tree.** With `HEAD`'s agent
header restored, `VerifyArchitectureBoundaries.cmake` stops with "Windows
owner-private descriptor construction ownership violation in
src/adapters/agent/include/bpr/adapters/agent/windows_workspace_guard.hpp".
With it removed, the check passes.

### Verification, at the final tree

- `cmake --build --preset dev`: exit 0, no warnings.
- The BPR-0622 courses, 6 of 6 pass:
  - `qt_agent_launch_canary` 105.6 s;
  - `agent_parent_death` 48.3 s;
  - `git_process_contract` 46.7 s;
  - `agent_workspace_ownership` 9.4 s, `sqlite-root` included;
  - `application_agent_crash_recovery` 52.5 s;
  - `application_agent_materialization_crash_recovery` 43.6 s.
- `architecture_boundaries` 18.1 s, `architecture_boundary_contract`
  332.3 s, `core_dependency_boundary` and `core_boundary_contract`: pass.
- Storage rows on the shared descriptor, 25 of 25 pass:
  - every `sqlite_*` row;
  - `quill_logging`, `application_logging`, `release_update_recovery`;
  - `application_release_maintenance`,
    `application_release_maintenance_public_path` and
    `application_profile_activation_public_path`.
  - The contained `sqlite_appcontainer_private_descriptor` prints
    `token_contained=1 product_contained=1 created=1 usable=1
    directory_valid=1 file_valid=1 directory_ace_count=2 file_ace_count=2`,
    so the AppContainer entry is still written and admitted.
- `test_network_isolation_manifest` shows exactly [[BPR-1108]]'s twelve
  findings and no new one; no registration was added.
  `test_network_isolation_structure_contract` passes.
- `core-only` preset: configure and build pass, and 157 of 160 rows pass.
  None of the three failures is this card's:
  - `documentation_structure`: the local `NEXT_ID` was behind origin while it
    ran;
  - `test_network_isolation_manifest`: [[BPR-1108]]'s
    `sqlite_release_maintenance_files` findings, the only one of its four
    rows in that preset;
  - `rfc0016_production_course_security`: a Windows path-separator
    comparison in `tests/scripts/rfc0016_production_course_tests.py`
    (`outcomes\x.json` against `outcomes/x.json`), reported separately.
- clang-format 22.1.3:
  - the new header is formatted;
  - the other touched C++ files are as clean as before.
- clang-tidy `--warnings-as-errors`, on `windows_agent_supervisor.cpp`,
  `quill_log.cpp` and the course executable:
  - no finding in any touched file or in the platform, native or agent
    headers;
  - the supervisor's five findings are older and are in lines this card
    did not touch.

### macOS and Linux

The portable edits are CMake and tests:

- the new `INTERFACE` target, and its links;
- the boundary script and its tests;
- the course's `neighbourhood` variable. On POSIX it keeps its old value,
  and the `sqlite-root` step is `_WIN32` only.

The header's content is `_WIN32` only. Nothing in the POSIX behaviour of
these courses should change. But portable CMake and the boundary gate did
change, so the criterion asks for runs, not reasoning. The macOS and Linux
runs are owed from those stations; this one cannot make them:

```sh
ctest --preset dev --output-on-failure -R '^(agent_workspace_ownership|git_process_contract|agent_parent_death|qt_agent_launch_canary|application_agent_crash_recovery|application_agent_materialization_crash_recovery|architecture_boundaries|architecture_boundary_contract)$'
```

### Where the criteria stand

- **Items 1–4: pass.** Items 1, 2 and 4 were done on 2026-09-17 (above).
  Item 3 is done here, as the owner chose, with the course step and the
  reverted trial.
- **The BPR-0622 Windows courses: pass**, 6 of 6 at the final tree.
- **macOS and Linux: pending**, for the runs above. The card stays In
  Progress until those stations supply them. Only then can it finish.

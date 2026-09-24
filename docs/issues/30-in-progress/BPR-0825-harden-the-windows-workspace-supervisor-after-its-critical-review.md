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

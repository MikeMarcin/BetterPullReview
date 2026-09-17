---
schema: "implement-rfc.card.v1"
id: "BPR-0825"
title: "Harden the Windows workspace supervisor and recovery after BPR-0622's critical review"
status: "In Progress"
rfcs: ["RFC-BPR-0013"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"dsp-002 rfc-reviewer-critical of windows-lane 743c59d7..c94e02b1 (no blocking findings)","found_at":"2ba02ddbe59bf6e07302cbb7460f61cc9cad2627","run":"run-20260916-stations","dispatch":"dsp-002"}
---

# BPR-0825: Harden the Windows workspace supervisor and recovery after BPR-0622's critical review

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

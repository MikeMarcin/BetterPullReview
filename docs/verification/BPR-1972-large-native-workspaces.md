# BPR-1972 large native workspaces

Original mandate `retained-workspace-continuation-r1`, owner amendment
`retained-large-projects-r2`, implementer `/root/retained_workspace_fix`, branch
`main`. Immutable baseline `ad032464aa76213bb1e22483267c5ceb7fbe9c48`;
plan/import `c36cfcf6e`, claim `7e2eb24fe`, product `3f233964dfe59461b4e347c757fa6e8fd72c7dd4`.
BPR-1969 and its accepted historical evidence are unchanged.

## Owner policy amendment and result

After reporting a continued refusal at the retained storage gate, the owner said:
“I mean the workspace is an extremely large project. There's no reason we shouldn't
be able to operate on extremely large projects though.” This supersedes the first
follow-up proposal to preserve the fixed aggregate limits. The original proposal
is retained in Git and externally as `original-plan-r1.json`; the live card/plan
and renewed binding implement r2. The original adoption `source.json` is preserved.

The production change deletes the entire profile-neighbourhood inventory helper
and its pre-selection call: 36 lines removed, with no larger replacement cap. New
native sessions and continued sessions are no longer refused because retained
projects exceed 16 GiB or 262144 filesystem entries. An unrelated unreadable
session directory is no longer inventoried before the selected conversation.

Selected-workspace ownership, journal, pinned HEAD, no-alternates and required
commit/root-tree checks remain. Actual Git/filesystem/cancellation errors still
fail through materialization/inspection. Catalog/journal count bounds, native
storage failure classification and protected exact cleanup remain. No automatic
pruning, deletion, clean replacement, permission expansion or background request
was introduced.

## Acceptance and direct evidence

| Criterion | Observation and judgment |
|---|---|
| AC1 | Direct host dispatch starts, continues and starts another native scope while a test-owned sparse file has logical length 16 GiB+1; the large file remains intact. A dedicated serial course creates 263169 real filesystem entries with zero file-data bytes, using empty hard links under a private random root. First turn, reuse and new-scope dispatch all succeed. A POSIX unreadable unrelated directory also leaves selected continuation working. Linking the previous production host object against the new regression fails the first-turn assertion for both the byte and entry fixtures, exit1 as expected. Pass: both old thresholds are directly distinguished, without claiming the corporate machine's exact trigger. |
| AC2 | Existing host refusal rows still reject retained missing objects/ownership with no process start, rematerialization or cleanup; real Git retained/sparse contract passes. Native select/pre-send store/append/post-send store faults remain private_storage_failure. Production selected-workspace validation is unchanged. Pass. |
| AC3 | Materialization/error/cancellation and cleanup courses in the invocation host still pass; the scoped production diff removes only the admission walk/call and changes no materializer or cleanup authority. First/new-scope creation is admitted above the old threshold while actual workspace failures retain their typed outcomes. Pass; no disk-filling test or new permission is used. |
| AC4 | The post-commit host and six-course native/tour/replay composition pass. The large-project turn reaches the real composed stub launcher, answers in the visible conversation and reuses its workspace. The replay course now expects the additional successful source turn while retaining the missing-marker/no-dispatch negative control. Architecture, real Git, isolation manifest, format/tidy and documentation checks pass; the owner amendment and actual verification limits are recorded. Pass. |

The entry fixture records `created_entries=263169 logical_bytes=0` from successful
creation calls. It is not an injected size/count fact. Each seed has 512 hard links,
and all entries are inside the course's private owned root. The serial180-second
watchdog bounds fixture creation and cleanup; it is not a per-turn timing assertion.
Successful completion and both negative controls remove their own fixtures. The
sparse-file helper marks Windows files sparse before extending them using the
[documented FSCTL_SET_SPARSE API](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_set_sparse);
its Windows branch is unqualified here, not a claimed native Windows pass.

## Verification and exact candidate

- At the product revision above, rebuild of the host/public-composition targets
  succeeded. `application_agent_invocation_host` and
  `application_native_agent_sessions`:2/2 pass,24.13s total (0.29/23.81s).
  The latter runs native first/reopen, tour first/reopen and replay first/reopen.
- Integration before the bookkeeping commit, using the same relevant product
  contents:4/4 pass,67.70s total: `architecture_boundaries`20.50s,
  `application_agent_invocation_host`0.15s, `git_process_contract`22.45s,
  `application_native_agent_sessions`24.56s.
- `application_agent_workspace_entry_limit`:pass,62.58s including creation and
  cleanup of263169 entries. The observed count and pass sentinel are retained in
  `entry-count-observation.log`.
- Old production from `7e2eb24fee1b9a06425123b60fb7b4fc27c3df7f`, compiled as a
  separate object and linked only into an external negative-control binary,
  fails the new byte/entry first-turn assertions as expected. Working source and
  ordinary build outputs are not swapped. `negative-result.log` and
  `negative-entries.log` retain exit1 and the distinguishing assertions.
- `test_network_isolation_manifest`:pass,0.78s. The new entry course is serial and
  network isolated; no new uncontained registration or loopback port is added.
- Clang-format22.1.8 checks all four touched C++ files; scoped clang-tidy22 passes
  the sole changed production translation unit, with one non-user warning
  suppressed. `git diff --check`:pass.
- `python3 scripts/verify_documentation.py --root .`:DOCUMENTATION_OK.
  `python3 tests/scripts/verify_documentation_tests.py`:67/67 pass.

The first composed attempt failed `replay-separate-projection`: its assertion
still expected one source turn after the old size refusal. The changed policy
correctly answered a second turn, so the expected history and no-inference process
count were updated. The original failing `public-tests.log` is retained; the final
composition passes. No failed command is represented as passing.

Debug wall times are observations only, not performance acceptance. No dedicated
Debug timing course or optimized Release/resource qualification is claimed.

## Remaining overhead and evidence limits

The removed gate traversed every retained/crash-recovery checkout. Selected
ownership/no-alternates validation does not recurse the checkout or traverse its
commit history. It reads exact ownership/HEAD files, one journal capped64KiB, and
the fixed head/base commit/root-tree object probe. Actor scheduling, filesystem
latency/locks and Git pack-index lookup remain.

The Git cache adapter's `inspect_agent_workspace` still calls
`validated_inventory(false)` on the separate application bare-cache repository.
That recursively validates cache filesystem artifacts (loose files, refs and
pack/index pairs), up to its existing1000000-entry bound. Packed history objects
are not individually enumerated, but this scan can scale with cache artifacts.
This existing integrity path is unchanged; no constant total send-time guarantee
is claimed. The architecture contract explicitly records that residual.

The actual corporate Enterprise machine is unavailable. The earlier
100800-entry/8.4GiB measurement described one workspace, not a current aggregate
inventory. Standard local default/profile neighbourhoods are empty and cannot
attribute the corporate refusal. Bytes, entries or traversal failure could have
triggered the old generic gate; no definitive trigger is invented. The owner's
new policy removes that gate for all those cases while preserving selected-state
integrity. Tests are macOS arm64 source/composition evidence; no corporate-project,
Windows/Linux native, installed-agent or release/trust result is claimed.

## Durable checkpoint and ownership

External state: `/Users/jaeger/.codex/tmp/bpr-1972-retained-budget/` contains
resolved selection, original/amended plans, `plan-binding-r2.json`,
`checkpoint-r2.json`, acceptance/observation records, source/binary identities,
command logs and `handoff.md`. Global checkpoint bind still refuses the unrelated
historical release-v041 plan's `amendments` field. Explicit-plan binding succeeds;
the documented checkpoint fallback uses it plus the official repository-state
helper. The stock final check evaluates the completed observation directly.
Historical plans outside this scope are unchanged.

The untracked `.codex-attachments/` and another task's BPR-1973 reservation are
preserved and excluded from commits. Concurrent1973 allocation advanced NEXT_ID
before this task's import staged the shared counter; the valid1974 value was kept,
and that receipt remains its owner's responsibility. No counter is rewound.

No review server or protected attended candidate exists. All task-owned commands
finish before handoff; consumed negative-control source/object/binary outputs are
removed, with logs and reproduction commands retained. No remote mutation, push,
owner-desktop access or installed-agent request was performed. Final finish commit,
checkpoint result, preserved foreign state and next action are in `handoff.md`.

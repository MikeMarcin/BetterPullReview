# RFC-BPR-0035 verification ledger

RFC baseline: `1b4c24154bd1d4f1d4bea3d84c593e2fb4c3d47f`.
Mandate `r1`, 2026-09-20: implement the RFC. Milestone 1 is
`rfc0035-auto-updater/m1-core-contract`, child `/root/rfc0035_m1`.
The [live closeout](../issues/10-backlog/BPR-0912-close-rfc-0035.md) retains all
full RFC acceptance; this ledger describes bounded component evidence only.

## Milestone 1 candidate and checks

Accepted source candidate: `42a85cf71bfe826a76c44dfe98e982a0bdc8c76b`, on
macOS 26.6.2 arm64 (CMake 4.4.3, Python 3.14.7). [[BPR-0946]] and the closeout
bind completion/upkeep to that source. The public fixture entry is
`release_update_application_host::dispatch(check)`, followed by its accepted
candidate-bearing Download gesture. The terminal observable is the public
projection at `ready`, with the installer still unavailable. Removing the release
capability prevents the offer, which is the focused negative control.

`core_release_update` covers stable grammar/overflow/numeric precedence, equal
build metadata, drafts/prereleases, fixed source, exact ZIP selection and duplicate
identities, digest and size bounds, daily/manual/rate policy, Later/Skip, persisted
clock normalization, capability refusals, all completion context fields,
invalidation/duplicate terminals and the exact download/verification course.

`application_release_update` drives fake ports through separated manual executors:
typed metadata/staging failures, changed opaque archive/bundle identity,
out-of-order account completions, revocation during effects and before decision
delivery, all active-phase shutdowns, disable/cancel, coalesced progress, four-job
admission, bounded reentrant feedback, projection exceptions, effect queue refusal
and destruction with cancelled work still pending.

Early focused course: 2/2 passed. Initial compile warnings for a test range copy
and a lambda capture shadow were corrected. The expanded focused pass also
passed; its architecture scanner failed in the working checkout because it
recursively found the existing unrelated
`.claude/worktrees/agent-a17cae88115dbbb4e` target declarations. That worktree is
preserved. `architecture_boundary_contract` and `core_boundary_contract` passed.
The first exact candidate export at `64bbf9c1` then exposed a real boundary
violation: provider endpoint constants were placed in the application header.
They were moved into the GitHub adapter's private contract header; this does not
add a transport implementation. Both failures and the corrected-candidate checks
remain part of the evidence.

Final verification at the candidate:

- `cmake --preset core-only`; `cmake --build --preset core-only`: pass.
- `ctest --preset core-only -R '^(core_release_update|application_release_update|core_dependency_boundary|application_host)$' --output-on-failure`: 4/4 pass; the final added core cases also pass their focused rerun.
- `core_boundary_contract` and `architecture_boundary_contract`: pass; unchanged scanner-contract evidence reused after the provider-constant move.
- Clean Git export of `42a85cf7` at `/tmp/bpr-rfc0035-m1.Dd9kcw`: `cmake -DBPR_ROOT=/tmp/bpr-rfc0035-m1.Dd9kcw -P /tmp/bpr-rfc0035-m1.Dd9kcw/cmake/VerifyArchitectureBoundaries.cmake` and the same command with `VerifyCoreBoundary.cmake`: pass. The export is disposable, not retained as evidence.
- Eight touched C++ files pass the canonical format check. Four changed translation units pass clang-tidy with warnings-as-errors; the final expanded core test passes a focused tidy rerun. The GitHub private contract header passes `c++ -std=c++23 -x c++ -fsyntax-only`.
- `python3 scripts/verify_documentation.py --root .`: pass. `python3 tests/scripts/verify_documentation_tests.py`: 64 tests pass. `git diff --check`: pass.

No complete Qt/native/release graph is claimed by this core-only evidence.

## Semantic documentation disposition

| Surface | Disposition |
|---|---|
| Public core/application APIs, identity and lifetime | New [updater component contract](../architecture/release-updater.md); system architecture links the allocation |
| Preferences and scheduling | Pure policy and snapshot contract documented; persistent store/timer composition remains milestone 2 |
| Authentication, network and native validation | Narrow contracts and bounds documented; no credential access or production transport exists in this candidate |
| UI, packaging, helper, native installation, recovery | No runtime impact in milestone 1; owning RFC-0014/security/packaging claims stay unchanged until implementation and qualification |
| Existing review data, publication and credentials | No affected composition or schema; no migration and no remote mutation |

No attended or independent acceptance is required for this component milestone.
There is no review server or protected updater candidate. Native signing,
private-service access, real menu/Settings operation, persistent preferences,
multi-process installation, recovery and packaged Apple Silicon/Intel upgrade
evidence remain unverified. The existing RFC dependency order still gates closure.

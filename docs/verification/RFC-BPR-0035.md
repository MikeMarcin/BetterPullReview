# RFC-BPR-0035 verification ledger

RFC baseline: `1b4c24154bd1d4f1d4bea3d84c593e2fb4c3d47f`.
Mandate `r1`, 2026-09-20: implement the RFC. Accepted bounded milestones are
`rfc0035-auto-updater/m1-core-contract` (child `/root/rfc0035_m1`) and
`rfc0035-auto-updater/m2-download-staging` (child `/root/rfc0035_m2`).
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

## Milestone 1 semantic documentation disposition

| Surface | Disposition |
|---|---|
| Public core/application APIs, identity and lifetime | New [updater component contract](../architecture/release-updater.md); system architecture links the allocation |
| Preferences and scheduling | Pure policy and snapshot contract documented; persistent store/timer composition remains milestone 2 |
| Authentication, network and native validation | Narrow contracts and bounds documented; no credential access or production transport exists in this candidate |
| UI, packaging, helper, native installation, recovery | No runtime impact in milestone 1; owning RFC-0014/security/packaging claims stay unchanged until implementation and qualification |
| Existing review data, publication and credentials | No affected composition or schema; no migration and no remote mutation |

No attended or independent acceptance was required for this component milestone.
There is no review server or protected updater candidate. Native signing,
private-service access, real menu/Settings operation, persistent preferences,
multi-process installation, recovery and packaged Apple Silicon/Intel upgrade
evidence remained unverified at the milestone-1 boundary. The existing RFC
dependency order still gates closure.

## Milestone 2 candidate and public composition

Accepted source candidate: `54535c1e3638af99c5e3560ce3033f4cf1ff5a06`, on macOS
26.6.2 arm64. Source commits are `5ed24a8c` (desktop, persistence, native transport
and staging), `1f229029` (exact signing-effect owner and negative boundary test),
and `54535c1e` (writable mounted-image refusal). Claim: `a8578fcc`; [[BPR-0947]]
records the four passing card criteria. The authored plan and mandate are unchanged.

`application_release_update_public_path` first starts the actual `profile_desktop`
under its existing fixture, invokes its application menu and opens General Settings,
then observes the projection and orderly desktop drain. The same updater desktop
composition with injected fixture capabilities drives explicit account selection,
manual discovery, bounded literal notes, Later, Skip, SQLite preference/account
restart, daily timer discovery, manual override, download, verification, revocation
and shutdown. Verified staging never enables Install and Restart. Managed,
unsupported, untrusted, read-only, translocated and mounted-image fixtures acquire
no credential and expose the fixed manual-release link. This is a real Qt-to-host-
to-projection course, not a native credential or live private-service observation.
The offer screenshot `build/rfc0035-run/m2-offer.png` was visually checked: literal
notes, progress/status and controls fit without clipping; the fixture's old check
date is its intentional deterministic clock.

The app-wide store contains only automatic/skip preferences, attempt/success
timestamps and opaque account binding. Foreign schema, malformed state and storage
failure refuse destructive fallback. The native vault bridge is compiled into the
desktop and returns a release-only lease; accepted GitHub.com authority, selected
binding and request identity are checked around acquisition. No actual credential
was acquired in this milestone's tests. Bounded account acquisition and lease
revocation are retained when review windows close or accounts change.

## Milestone 2 transport and native staging

`release_update_transport_contract` and the 24-scenario
`release_update_native_https` course exercise the native transport with loopback
TLS, fixture certificates and random credential canaries. Cases include direct
metadata/assets, the one allowed CDN origin, forbidden metadata/HTTP/userinfo/
cross-origin/back-to-API redirects, the three-hop limit, 401/403/404/rate limits,
bounded and duplicate headers, content type/encoding, declared/streamed sizes,
refusing sink, cancellation, deadlines and untrusted TLS. Only the first API hop
receives the canary; redirect hops receive no authorization or cookies. Provider
factory checks refuse GHES and revoked authority. Inaccessible is never current.
The actual GitHub.com CDN route is not asserted from these fixtures.

The native staging course passes 17 refusal cases: traversal, absolute entry,
escaping symlink, symlink write alias, duplicate, device, wrong top-level bundle,
wrong bundle ID/version, future OS, missing universal architectures, unsigned
signer, wrong digest, changed archive, cancellation, excessive declared expansion
and excessive entry count. Short/excess stream writes, the private staging lock,
transaction-only asynchronous cleanup and bounded mount-inventory parsing are also
checked. Mounted-image parsing compares exact device identity, refuses malformed
observations, and does not mistake an unrelated writable volume for an image.

The genuine local source archive was
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/releases/0.1.3/BetterPullReview-0.1.3-macos-universal.zip`,
SHA-256 `72b71ebd642f83f86ff7ec889bd4e201a0586af40d32d9be9a15201e8d91a4a9`.
On disposable copies, the network-denied native course verifies the installed
production identity and eligibility, the bounded actual mount inventory, exact
version/bundle identity, both Mach-O slices and minimum-OS commands, deep strict
all-architecture signatures against the installed designated requirement and
pinned team, system execution trust, exact-tree revalidation and rejection after
a staged-tree mutation. Cleanup removes only its transaction. It does not launch,
replace or publish an app. This proves native staging on Apple Silicon, not an
older-to-newer install or Intel execution.

Initial failures were diagnosed and corrected before acceptance: the SDK lacked
LibArchive headers (now a centrally pinned 3.8.9 provider, SHA-256
`888c934f9d95648ecb9163dc8e23ab80a476ecb81a8f1154704a227b5b676dde`, static ZIP
reader with system zlib and bundled upstream notices); the real bundle's absent
plist minimum OS required checking both Mach-O deployment commands; materialized
AppleDouble sidecars broke framework signature sealing; codesign required its
literal-requirement `=` prefix. The device fixture uses a character-device entry
because LibArchive normalizes the earlier FIFO fixture. TLS header matching was
made case-insensitive and the expected untrusted-certificate refusal was handled
by the harness. The first clean source export exposed an omitted signing-owner
declaration; the exact source allowance and neighboring-source refusal now pass.
Final self-review added the missing writable-image check and tightened descriptor
failures, account acquisition and redundant Settings wiring.

## Milestone 2 commands and limits

- `cmake --preset dev`; focused build of `test_release_update_public_path`,
  `test_release_update_transport`, `test_release_update_host`,
  `test_release_update_core`, and `test_release_update_staging`: pass.
  `cmake --preset dev -DBPR_DEPENDENCY_OFFLINE=ON`: pass with cached pinned inputs.
- `ctest --preset dev -R '^(core_release_update|application_release_update|release_update_transport_contract|release_update_native_https|release_update_native_staging|application_release_update_public_path)$' --output-on-failure`:
  6/6 pass at the final source candidate. Final outputs:
  `build/rfc0035-run/m2-final-candidate-tests.log` and
  `build/rfc0035-run/m2-final-candidate-signed.log`.
- Signed course command (all artifact paths explicit):
  `python3 -I -S /Users/jaeger/Documents/GitHub/BetterPullReview/scripts/run_network_isolated_test.py --sandbox-exec /usr/bin/sandbox-exec --profile-template /Users/jaeger/Documents/GitHub/BetterPullReview/cmake/sandbox/macos-network-deny.sb --loopback-port-count 0 -- /opt/homebrew/Frameworks/Python.framework/Versions/3.14/bin/python3.14 /Users/jaeger/Documents/GitHub/BetterPullReview/tests/scripts/verify_release_update_staging.py --executable /Users/jaeger/Documents/GitHub/BetterPullReview/build/dev/tests/end_to_end/release_update_staging --signed-archive /Users/jaeger/Documents/GitHub/BetterPullReview/build/releases/0.1.3/BetterPullReview-0.1.3-macos-universal.zip`:
  pass, no network allowance.
- `python3 tests/scripts/architecture_boundary_tests.py`: 138 pass.
  `python3 tests/scripts/core_boundary_tests.py`: 4 pass. Both native boundary
  scans pass on a clean Git export of `54535c1e`. The ordinary checkout retains
  the prior unrelated `.claude` worktree scan interference; that worktree is not
  modified. The export and the earlier diagnostic signed copy are disposable.
- Canonical clang-format 23.1.1: 22 selected new/changed files plus the final
  four-file mount change pass (23 distinct files); existing large composition
  files received changed-hunk formatting only. Focused clang-tidy: task-owned
  diagnostics corrected and the final three-TU mount course passes with the
  host SDK arguments and release-update header filter. Full-header focused tidy
  retains the preexisting unrelated signed-bitwise finding in
  `home_view_state.hpp:35`; it is not a whole-repository tidy pass. An initial
  direct tidy call without SDK arguments failed to resolve standard headers;
  the host-configured rerun passes.
- `python3 scripts/verify_documentation.py --root .`,
  `python3 tests/scripts/verify_documentation_tests.py` (64 tests), and
  `git diff --check`: pass. The boundary contract suites' unchanged evidence is
  reused after the final bounded mount change.

Semantic documentation now covers production composition, authority, persistence,
fixed network/redirect paths, archive dependency and native validation in the
[component contract](../architecture/release-updater.md) and system architecture.
Milestone 3 still owns the signed helper, cross-window/process shutdown barrier,
pre-install release identity refresh and exact revalidation, journal, recovery,
relaunch receipt and transaction-qualified enablement. Milestone 4 owns package
and release/security operator documentation and Apple Silicon/Intel upgrade
evidence. Live private-service/native-vault operation, an actual mounted-image
installation, Intel execution and a packaged upgrade were not performed here.
No attended or independent acceptance is required by milestone 2; none is claimed.
No release publication, developer-app replacement or live mutation occurred.

## Milestone 3 implementation and pending qualification

Run `rfc0035-20260920`, segment `m3-implementation`, mandate revision `r1`.
Assigned scope: `m3-install-recovery` / [[BPR-0948]], sole engineering/bookkeeping
writer `/root/rfc0035_m3`, on `main`. Initial HEAD was
`f39850514b5449471fbff11b20a4998cad2560e5`; claim was
`5ca786979432273b56303c1a7d11d66b3b363dc4`. Source candidate:
`d1e0f46c77b02c07e6ca9ebc4d87f964f94035d3` (49 explicit paths).
The unchanged authored plan and original M3 obligation binding remain in
`build/rfc0035-run/m3-checkpoint.json`. Prior milestone acceptance is unchanged.

This is partial evidence, **not M3 acceptance**. The native installer capability
is unselected, so production Install and Restart remains disabled. No publication,
notarization submission, credential inspection, live service mutation, forced
termination or developer-app replacement was performed. The code-simplifier pass
kept the touched helper's refusal paths explicit; self-review also tightened
partial cleanup and true process-exit ownership. No independent review or human
acceptance is claimed.

### Direct behavior and limits

The public Qt composition exercises the existing menu and General Settings,
preferences/restart/daily checks, staging, revocation and manual fallback. An
injected installer extends that public path through exact refreshed-candidate
handoff and truthful recovered status. This proves composition, not native trust.
Two actual desktop windows save different inline editor text through
`quit_for_update`, drain orderly and expose both saved records in the read-only
recovery dialog. Obstructing the recovery database forces save refusal and leaves
the windows usable. SQLite tests prove append-only/deduplicated versions,
indeterminate markers, restart, aggregate/per-field limits, atomic refusal and
foreign/symlink refusal. The recovered text grants no publication authority.
No native active-publication course is inferred from these text/store fixtures.

The real filesystem transaction course uses fake trust/launch effects around the
production journal/lease engine. It covers all durable prepare/rename/launch/
receipt/rollback boundaries, wrong version/transaction, changed staged tree,
failed launch, false or missing flush acknowledgment, and retained external state.
Actual child processes hold kernel leases, exit cooperatively or exit without an
acknowledgment, and prove that process disappearance alone cannot authorize a
swap. A separate case destroys its lease object after acknowledgment but retains
native locks until OS exit; the parent cannot replace during that interval.
No signal or PID-reuse inference grants ownership. Helper timeout is covered as
a typed host refusal, not claimed as an observed 30-second native helper course.

Matching startup receipt is required before backup deletion. Cleanup first writes
a durable inode inventory; an injected stop after the first unlink resumes
idempotently. An unrecorded file added after that stop is retained, cleanup stays
recovery-required, and the healthy replacement is not rolled back. Active
interrupted transactions refuse normal startup until explicit helper `--recover`;
user-facing recovery launch and a missing-receipt native package course remain
unqualified. Terminal journals and failed candidate copies are retained; there
is no age/name sweep. A real power-loss/filesystem-durability experiment was not
performed.

### Exact signed artifact probe

Input (unchanged from M2):
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/releases/0.1.3/BetterPullReview-0.1.3-macos-universal.zip`,
SHA-256 `72b71ebd642f83f86ff7ec889bd4e201a0586af40d32d9be9a15201e8d91a4a9`.
The final disposable root was
`/private/tmp/bpr-rfc0035-m3-native-final.Vr2dT7`; the earlier probe used
`/private/tmp/bpr-rfc0035-m3-native.HNgoyF`. The course extracts two copies, uses
actual native signature/trust validation, copies onto the target volume,
interrupts after the old-bundle rename, restarts the journal engine and restores
the exact original content/inode manifest. Native validation passes again.
No application is launched. Both copies are version 0.1.3; this is rollback
evidence, **not an older-to-newer upgrade**. The old archive's missing helper is
explicitly refused as an installer trust source.

Final command:

```sh
python3 /Users/jaeger/.agents/skills/implement-rfc/scripts/qualification_guard.py \
  --allowed-root /private/tmp/bpr-rfc0035-m3-native-final.Vr2dT7 \
  --target-root /private/tmp/bpr-rfc0035-m3-native-final.Vr2dT7 \
  --live-root /Users/jaeger/Documents/GitHub/BetterPullReview \
  --live-file /Users/jaeger/Documents/GitHub/BetterPullReview/.mask.json \
  --live-file /Users/jaeger/Documents/GitHub/BetterPullReview/.mask.lock.json \
  --live-file /Users/jaeger/Documents/GitHub/BetterPullReview/.mask/skill-config/implement-rfc/project.json \
  -- python3 -I -S /Users/jaeger/Documents/GitHub/BetterPullReview/scripts/run_network_isolated_test.py \
  --sandbox-exec /usr/bin/sandbox-exec \
  --profile-template /Users/jaeger/Documents/GitHub/BetterPullReview/cmake/sandbox/macos-network-deny.sb \
  --loopback-port-count 0 -- \
  /Users/jaeger/Documents/GitHub/BetterPullReview/build/dev/tests/end_to_end/release_update_transaction \
  --native-signed-recovery /private/tmp/bpr-rfc0035-m3-native-final.Vr2dT7 \
  /Users/jaeger/Documents/GitHub/BetterPullReview/build/releases/0.1.3/BetterPullReview-0.1.3-macos-universal.zip
```

Result: child exit 0, `native-signed-recovery:pass;
upgrade-and-new-helper-receipt:not-qualified`. Guard reports all three selected
live files byte-identical. This cooperative guard compares those selected files;
it is not a whole-repository filesystem sandbox. The nested network-denial
harness grants no network/loopback exception. Final records:
`build/rfc0035-run/m3-final-signed-guard.json` and
`build/rfc0035-run/m3-final-signed-native.log`.

### Builds, checks and corrections

- Earliest shortest public path: `ctest --preset dev -R
  '^application_release_update_public_path$' --output-on-failure`, 1/1 pass.
- `cmake --preset dev` and focused builds of `test_release_update_core`,
  `test_release_update_host`, `test_release_update_transaction`,
  `test_release_update_recovery`, `test_release_update_public_path` and
  `tool_bpr_update_helper`: pass. Final corrections are in
  `m3-self-review-build.log` and `m3-lifetime-build.log` under the run directory.
- `ctest --preset dev -R '^(core_release_update|application_release_update|release_update_transport_contract|release_update_native_https|release_update_native_staging|release_update_transaction|release_update_recovery|application_release_update_public_path)$' --output-on-failure`:
  8/8 pass; `build/rfc0035-run/m3-integration-tests.log`. Unchanged M2 native
  transport/staging checks are included once at this integration boundary.
- `cmake --preset core-only`, focused core/host build and
  `ctest --preset core-only -R '^(core_release_update|application_release_update)$' --output-on-failure`:
  2/2 pass; `m3-core-final-build.log` and `m3-core-final-tests.log`.
- `cmake --preset app-release -DBPR_MACOS_CODESIGN_IDENTITY=-` and
  `cmake --build --preset app-release --target app_better_pull_review`: pass with
  tests disabled; final output `m3-production-final-build.log`. This local
  Apple Silicon build is ad-hoc signed, not notarized or universal release
  evidence. Helper SHA-256:
  `af355133d5b470ace4af4387b0580acc777e14dad412e4b9e42243708133707f`;
  main executable SHA-256:
  `806ba04fe243589d2276aee9e6db40b7d42b4d5e5fcfac74ab385020382b4415`.
  The built helper runs under an empty environment and returns expected exit 1
  for recovery from its ad-hoc trust source, without mutating the target.
- `python3 tests/scripts/architecture_boundary_tests.py`: 139 pass;
  `python3 tests/scripts/core_boundary_tests.py`: 4 pass. The exact helper spawn
  owner is allowed and a neighboring source is refused. Both CMake boundary
  scans pass on a clean Git export of `d1e0f46c` at
  `/private/tmp/bpr-rfc0035-m3-export.OmYiXQ`. Outputs are
  `m3-exact-architecture.log` and `m3-exact-core-boundary.log`. The ordinary
  checkout's previously observed unrelated nested Claude worktree interference
  is unchanged; that worktree was neither edited nor removed.
- Canonical clang-format 23.1.1: 29 selected files plus changed ranges in nine
  existing large files pass. `m3-format.log` records the full-file check.
  Canonical clang-tidy passes 11 complete selected translation units
  (`m3-tidy.log`); ten composition/Qt/adapter units pass with host SDK arguments
  and changed-line filtering (`m3-changed-surface-tidy.log`). Existing
  full-header `home_view_state.hpp:35` signed-bitwise evidence from M2 is
  retained; this is not a whole-repository tidy pass.
- `python3 scripts/verify_documentation.py --root .`: `DOCUMENTATION_OK`;
  `python3 tests/scripts/verify_documentation_tests.py`: 64 pass, including
  expected negative-fixture diagnostics. Scoped RFC board validation and
  `git diff --check` pass. Logs: `m3-documentation.log` and
  `m3-documentation-tests.log`. Live rebinding confirms the original M3
  definition, obligation and prerequisite fingerprints are unchanged.

Initial build errors (shadowed captures and optional-root unwrapping) were fixed
before tests were used as evidence. The first two-window barrier check exposed
that appearance `is_quiescent()` requires Closing and is not a pre-quit durability
test; explicit durable-state queries fixed that refusal. Lazily unloaded
preferences no longer cause false refusal. Temporary diagnostics were removed.
The actual release artifact exposed a development-only executable-name assumption,
corrected to `Contents/MacOS/BetterPullReview` before the native rollback probe.
Final self-review added authority-transfer semantics, partial-cleanup recovery and
OS-exit lock retention with their regression cases. Earlier stale-binary test
runs after failed builds are not credited to corrected source. Build logs retain
existing unrelated CMake warnings. A first board dry run used symbolic `blocked`
instead of the required `50-blocked` lane and refused without a write.

The required complete signed course remains blocked: an authorized participating
baseline and newer signed/notarized target containing this exact helper and
startup protocol are unavailable. Neither the old 0.1.3 archive nor an ad-hoc
development build can satisfy it. Continue M3 with disposable roots, retained
draft/publication state and exact source/artifact identities; establish native
timeout, restart receipt, recovery and cleanup before selecting the production
installer. Do not promote M4 or infer RFC AC-05/06/07 acceptance. Intel execution,
live private-service/vault operation and package/security closeout remain open.

### Ownership and cleanup

Self-review and all independent local engineering/checks are complete for this
candidate; native acceptance remains pending. All task-owned command sessions and
peer/helper processes are joined. No attended session, review server or protected
candidate exists. The two disposable native roots and exact source export named
above were moved to `/Users/jaeger/.Trash/` (recoverable); shared build outputs and
run evidence remain. The user's `.mask.lock.json` modification and nested Claude
worktree were preserved. [[BPR-0948]] is Blocked with its full obligations intact;
[[BPR-0949]] stays dependent. [[BPR-0912]] is the durable continuation checkpoint.

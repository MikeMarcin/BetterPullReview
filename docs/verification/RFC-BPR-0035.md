# RFC-BPR-0035 verification ledger

RFC baseline: `1b4c24154bd1d4f1d4bea3d84c593e2fb4c3d47f`.
Mandate `r1`, 2026-09-20: implement the RFC; amendment `r2` permits disposable
local signing/notarization, not publication or live release/vault access.
Owner amendment `r3` separately defers Intel Mac updater qualification to
[[BPR-0950]], outside M4 and RFC-0035 closeout. Intel execution remains unobserved;
the amendment changes acceptance rather than supplying native evidence.
Accepted bounded milestones are
`rfc0035-auto-updater/m1-core-contract` (child `/root/rfc0035_m1`) and
`rfc0035-auto-updater/m2-download-staging` (child `/root/rfc0035_m2`), followed by
`m3-install-recovery` (child `/root/rfc0035_m3_resume`). M4 available-target
engineering is committed at `518c384a`; its live-service/dependency exit is pending.
The [live closeout](../issues/10-backlog/BPR-0912-close-rfc-0035.md) retains all
full RFC acceptance; this ledger describes bounded component evidence only.

## Milestone 4 owner acceptance amendment and reassessment (mandate r3)

Run `rfc0035-20260920`, segment `m4-post-block-intel-deferral`, sole owner
`/root/rfc0035_m4_resume`, on `main` from
`fe728286093326839b63f49f83487d61b5d88ad8`. BPR-0950 is allocated with the shared
allocator and remains Backlog for later native Intel qualification. It is outside
the authored milestone plan and cannot block BPR-0949, BPR-0911, BPR-0912, M4 or
RFC-0035 closeout. No Intel execution, live credential/service use or new signed
artifact is observed in this round.

The plan amendment changes only M4's boundary to record the owner decision;
`check-amendment` preserves scope, work selectors, exit text and M1–M3. RFC AC-07
and BPR-0949:AC-01 now explicitly require Apple Silicon execution. The original
criteria and r2 blocked result remain below as historical records.

| Obligation | Current disposition under r3 |
|---|---|
| BPR-0949:AC-01 / RFC AC-07 | Pass for the amended Apple Silicon scope using the exact signed M3 package, receipt, retained-state and native UI evidence below. Intel evidence remains unobserved on nonblocking BPR-0950. |
| BPR-0949:AC-02 | Pass: operator/recovery/security/package documentation is complete; follow-up references distinguish the deferral from remaining acceptance gaps. |
| BPR-0949:AC-03 | Pass for truthful reconciliation of all RFC criteria; the existing text permits unavailable external evidence to remain open and forbids premature parent closure. |
| BPR-0911 / RFC AC-03 | Pending: no actual native-vault release credential, live private-release asset route or live revocation has been observed. M2's 24 TLS scenarios and menu/host fixtures remain bounded evidence. No access authority is granted. |
| BPR-0912 / M4 exit | Pending: full parent acceptance plus RFC-0014's Proposed status and canonical order. RFC-0014 retains its separate wider packaging/security requirements and RFC-0013 dependency; the index's first closure frontier is RFC-0008/BPR-0242. |

BPR-0949 is Done at `d6b5ed528b6646830debf5173de2f77c35c83144` after amendment/
allocation `1ded36b9` and resumed claim `9933db57`. BPR-0911 is Blocked on
`private-service-qualification`; BPR-0912 remains Backlog; RFC-0035/index remain
Proposed. Full M4 is blocked by the remaining live-service and closeout gates,
not Intel. BPR-0950 is Backlog outside the adopted plan, with no dependency from
these remaining acceptance units.

No product source changed from `518c384a285ef3cb873d12ea167488d105aa91b7`.
Accepted M3 source remains `b7e2df46cc178bbcff88b49dd2170ecf27a8c764`; signed
package source remains `5db1d104a2436a2b0df811ac2e40cff0c58c2319`. The package
does not contain M4's separately tested native menu correction. Reuse applies only
to unchanged installer/helper/state/authority behavior, not a new final signed
package claim. Prior evidence, failures and resource limits remain intact.

The shortest public composition ran before edits:
`ctest --preset dev -R '^application_release_update_public_path$' --output-on-failure`:
1/1 pass, 3.97 seconds. One focused integration pass at `9933db57` used:
`ctest --preset dev --output-on-failure -R '^(core_release_update|application_release_update|release_update_transport_contract|release_update_native_https|release_update_native_staging|release_update_transaction|release_update_recovery|application_release_update_public_path|application_profile_empty_desktop_native_menu_bar)$'`:
9/9 pass, 21.50 seconds, including real Cocoa menu and loopback TLS cases.
`python3 scripts/verify_documentation.py --root .` passes and
`python3 tests/scripts/verify_documentation_tests.py` passes all 64 tests.
Receipts are `build/rfc0035-run/m4-r3-{integration,documentation,documentation-tests}-evidence.json`.
Scoped board/plan checks and `git diff --check` pass. Product/test/build inputs
have no diff from `518c384a`; there is no new build or native signing artifact.
Final lane/documentation checks and the bound r3 observation are recorded in the
live closeout. All task commands/fixture processes are quiescent; the unrelated
lock-file edit and worktrees remain intact. No new temporary installation,
attended session, review server or protected candidate exists. No attended or
independent verdict is required for this documentation/acceptance amendment;
none is claimed.

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

## Milestone 3 resumed signed qualification (mandate r2)

Run `rfc0035-20260920`, segment `m3-post-block-signing`, sole owner
`/root/rfc0035_m3_resume`, on `main` from `14efff7ee60bc1a10c5122402e48aca2f44c690a`.
The user explicitly authorized local production signing/notarization credentials
for disposable qualification only. No release, tag, push, GitHub mutation or
installed/developer-application replacement was performed. The prior writer was
quiescent, and the unrelated lock-file amendment and Claude worktree were preserved.
Claim: `182733b3105db797fbf345bc4ac099053f18e005`. Strategy and milestone binding:
`build/rfc0035-run/execution-strategy.json` and `m3-r2-checkpoint.json`.

### Corrected and qualified candidate

`5cf310c42ac392c704d4722f344afe81886ea89e` fixes a concrete native relaunch gap:
an empty helper environment could lose the explicit application-state root. The
journal now binds its canonical owner-private path and inode; startup must accept
that root before opening stores. Aliased/replaced roots fail closed, and a missing
startup root cannot silently select live default state. Regression cases cover
the refusal and exact restored-root receipt.

After the four signed native courses passed,
`b7e2df46cc178bbcff88b49dd2170ecf27a8c764` selected the installer only for eligible
trusted macOS composition and added guarded native test drivers/harness. Automated
fixture, managed, untrusted and unsupported boundaries remain unavailable. The
final package source `5db1d104a2436a2b0df811ac2e40cff0c58c2319` differs from that
candidate only in `CMakeLists.txt`'s qualification version. It passes a repeated
native upgrade with the installer-enabled source. No M4 acceptance is inferred.

### Exact local artifacts

The existing signing identity and `BetterPullReview-notary` keychain profile
passed preflight without displaying credential material. The authoritative
`tools/release.sh` procedure built tests-disabled universal code, signed nested
Mach-O/helper code with hardened runtime and secure timestamps, checked a relocated
application, notarized and stapled the application and DMG, and passed native
signature/execution assessment. Broad tests were skipped only in this packaging
command; the focused exact-source suite ran separately. No remote release was
created. Qualification versions are stable SemVer below the latest `v0.1.3`, so
they cannot become upgrades to that released production version. Main's version
and release notes remain unchanged.

| Version | Qualification source | ZIP SHA-256 |
|---|---|---|
| 0.0.350 | `d88e744149c3c62ada87dfbbfd5dc1ac35113d27` | `2163f5cc21b9116c4b0571386e34b88cb33a9a26cccaa32a4ababa285c6e34f4` |
| 0.0.351 | `8a09847250dabc1ae90ae26631c226771c01455e` | `f4e0dfbaa8a0e0c2f3a9ada48cbf9ccffd6394fbdce53b69cb9da56cc93d959f` |
| 0.0.352 | `5db1d104a2436a2b0df811ac2e40cff0c58c2319` | `29ab8fbfc786864ca7f27bdbe95315bcce254fabdb4e5c0695973c2bf1bc3fa7` |

Packages and checksum sidecars are retained under
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/rfc0035-run/packages/<version>/`;
names are `BetterPullReview-<version>-macos-universal.zip` and `.dmg`.
DMG SHA-256 values, in the same version order:
`d5e5e66be83f6d6c463bcffce84e20f454efc52bba5ed496dccc790ea399c05b`,
`cf1bc18a2f7aa2a8a613bd272b6d57d003610ae21b47949cf0df0d1a9d7a42ce`,
`98ec6b7172887014501e92cda038c6e686277aaf712c405e9296eb427330508a`.
Executable SHA-256 values are respectively
`721e51b8c63e96e15fc748454066859f10e7d4ed5f33abd079ecb7174c960086`,
`c125970d4a8d5a7a97339f16f41910227e28a3ab3ab788ee7cd99f04d7414b7f`,
`1882523982a47a6c0abaa528f09fb19bb6bf8913629252d22f507b22bc5028c5`.
Package logs are `m3-r2-package-0.0.350.log`, `m3-r2-package-0.0.351.log`,
and `m3-r2-package-0.0.352.log` in the run root.

The clean detached worktree was
`/private/tmp/bpr-rfc0035-m3-r2.g1eYKY/source`. For each committed version it ran
the following absolute command (substitute the exact version from the table in
both places); all three exited zero:

```sh
/private/tmp/bpr-rfc0035-m3-r2.g1eYKY/source/tools/release.sh --version 0.0.352 --skip-tests --output-dir /Users/jaeger/Documents/GitHub/BetterPullReview/build/rfc0035-run/packages/0.0.352
```

### Actual native acceptance

The courses ran serially on Apple Silicon macOS, never through a product test
hook. Signed applications used a canonical disposable `CFFIXED_USER_HOME`;
helper relaunch used its empty environment plus the new journal-bound state root.
A sandbox denied network access, live BetterPullReview state reads and writes
outside the exact disposable root. No real account or review publication was used.
The public driver supplies local fixture release authority/archive bytes, but uses
the actual menu/Settings/service, native staging/trust, installer and signed helper.
The exact candidate metadata is refreshed before handoff. Store qualification
uses real private SQLite stores with synthetic drafts and a possibly-transmitted
publication record, including its exact baseline and fingerprints.

| Native course | Actual result |
|---|---|
| 0.0.350→0.0.351 | Passed in 12.998 s including staging/handoff. Old signed app drained and exited normally; two native window IDs reopened at generation 2. Matching receipt removed the owned backup. |
| Uncooperative peer | Passed in 38.245 s including staging. The real helper waited its 30-second bound, preserved the exact old executable and did not terminate the peer. Only the supervisor's later explicit stdin request made that fixture peer exit itself. |
| Missing receipt | Passed in 63.228 s helper time. A disposable required-store obstruction after old-app drain caused the actual new startup to fail without a receipt. The helper waited 60 seconds, then restored the exact old executable. Restoring the test store allowed the old app to launch, show its draft, and display preserved/restored rather than successful installation. |
| Interrupted first rename | Passed. The signed helper inside the exact journal-owned backup restored the old application via `--recover`; no new version launched. |
| Final 0.0.350→0.0.352 | Passed in 13.603 s including staging/handoff. Native UI showed 0.0.352, healthy-start completion and an installer gated by absent release authority rather than absent capability. Logical windows 1/2 retained profile 1, ordinals 0/1 and advanced generation 1→2. |

Every course retained both draft bodies, the indeterminate marker, and the exact
unresolved publication record. No operation was retried against a server. The
final receipt binds transaction `85361061-405d-4135-ba11-df5abb00dd5b`, version
`v0.0.352`, the final executable digest above, participant
`process-4912a5a7-827b-41e7-8397-5352dadafc32` and inode `16777231:333252169`.
All result files report `passed: true` and `applications_quiescent: true`.
Evidence snapshots are `build/rfc0035-run/native-r2-evidence/{success,timeout,missing-receipt,rollback,final-upgrade}.json`.

The final course command was:

```sh
python3 /Users/jaeger/.agents/skills/implement-rfc/scripts/qualification_guard.py --allowed-root /Users/jaeger/Library/Caches/bpr35.6aKkXi --target-root /Users/jaeger/Library/Caches/bpr35.6aKkXi/final-upgrade --live-root /Users/jaeger/Documents/GitHub/BetterPullReview --live-file /Users/jaeger/Documents/GitHub/BetterPullReview/.mask.json --live-file /Users/jaeger/Documents/GitHub/BetterPullReview/.mask.lock.json --live-file /Users/jaeger/Documents/GitHub/BetterPullReview/.mask/skill-config/implement-rfc/project.json -- python3 /Users/jaeger/Documents/GitHub/BetterPullReview/tests/scripts/verify_release_update_install.py --root /Users/jaeger/Library/Caches/bpr35.6aKkXi/final-upgrade --baseline /Users/jaeger/Documents/GitHub/BetterPullReview/build/rfc0035-run/packages/0.0.350/BetterPullReview-0.0.350-macos-universal.zip --target /Users/jaeger/Documents/GitHub/BetterPullReview/build/rfc0035-run/packages/0.0.352/BetterPullReview-0.0.352-macos-universal.zip --public-driver /Users/jaeger/Documents/GitHub/BetterPullReview/build/dev/tests/end_to_end/release_update_public_path --transaction-driver /Users/jaeger/Documents/GitHub/BetterPullReview/build/dev/tests/end_to_end/release_update_transaction --scenario success --observe-ui
```

Earlier courses used the same guarded command with the 0.0.351 target and sibling
roots `success`, `timeout`, `missing-receipt`, `rollback`; only success paused for
agent UI observation. Result logs use `m3-r2-{success3,timeout,missing-receipt,rollback,final-upgrade}-{guard.json,course.log}`.
The supplemental recovered-app UI launch is recorded by
`m3-r2-recovered-ui-guard.json`/`.log`. Every successful guard reports all three
selected live files unchanged. Network denial also refuses the local activation
socket; native in-process window creation and updater shutdown still pass.

### Integration, self-review and limitations

The shortest public-path check passed before editing. Final source passes
`cmake --build --preset dev --target release_update_public_path release_update_transaction release_update_recovery`
and `ctest --preset dev --output-on-failure -R release_update` (8/8, including
the transport, native HTTPS/staging, transaction, recovery and public-path rows).
Logs: `m3-r2-enabled-build.log`, `m3-r2-final-integration.log`.
The existing M1/M2/core-only and full fault/foreign-cleanup evidence remains
applicable; native results above supply the previously missing M3 acceptance.

`python3 tests/scripts/architecture_boundary_tests.py` passes 139 cases;
`python3 tests/scripts/core_boundary_tests.py` passes four. Both CMake boundary
scans pass against the clean export of `b7e2df46`, using that export's own verifier
scripts. Logs: `m3-r2-architecture-contract.log`, `m3-r2-core-boundary-contract.log`,
`m3-r2-exact-architecture.log`, `m3-r2-core-boundary.log`.
Canonical clang-format checks six complete files and changed ranges in both
desktop composition files. Three complete native/test translation units and
three changed-surface composition/public-path units pass clang-tidy with the host
SDK; logs include `m3-r2-format.log`, `m3-r2-format-ranges.log`,
`m3-r2-tidy.log`, `m3-r2-changed-surface-tidy.log`, `m3-r2-profile-tidy.log`.
Full public-path tidy still reports the preexisting `home_view_state.hpp:35`
signed-bitwise warning; it is not silently treated as a whole-file pass.
The documentation gate and all 64 documentation tests pass.

Initial probes under `/private/tmp` failed because Qt exposed the state root as
`/tmp`, correctly triggering canonical-root refusal. Moving the fixture to its
canonical cache path resolved this without weakening product checks. An initial
driver compile used the wrong command enum/Qt cast and was corrected before any
credited run. A guard refused an absent target directory before any child ran.
An initial clean-export architecture invocation used the checkout verifier and
therefore scanned the export verifier's own forbidden-token table; invoking the
exact export's verifier fixed the invocation. These failed attempts do not count
as passing evidence. Self-review kept shutdown/cleanup bounded and replaced the
harness's generic native quit adapter with explicitly observed UI quit; the
code-simplifier pass extracted version/sandbox setup without changing behavior.

M3 is accepted; Intel execution, live native private-release/vault operation,
self-service recovery/operator documentation and dependency-ordered RFC closeout
remain M4 work. A native two-window menu displayed duplicate Check for Updates
entries; preserve this UI finding for that public-path closeout. It did not alter
candidate authority, installation, retained-state or recovery results. No attended
or independent reviewer verdict is claimed and no such M3 decision is pending.

### Final ownership and continuation

All task commands and helper/application processes are quiescent. Normal native
Quit was observed; no application or peer was force-terminated. No review server
or protected candidate exists. The qualification commits are retained by local
branch `codex/rfc0035-m3-r2-qualification`; the clean detached checkout was removed.
The disposable installation root, earlier probes/export and qualification build
were moved, recoverably, to `/Users/jaeger/.Trash/bpr35.6aKkXi`,
`/Users/jaeger/.Trash/bpr-rfc0035-m3-r2.g1eYKY` and
`/Users/jaeger/.Trash/bpr-rfc0035-m3-r2-build-g1eYKY`. Signed packages, result copies
and command logs remain in the run root. Shared dependency/build caches and all
unrelated worktrees remain intact. [[BPR-0948]] closes with its original criteria;
[[BPR-0949]] is promoted to Ready without being claimed. [[BPR-0912]] and the
runtime `m3-r2-observation.json` bind the M3 continuation to mandate r2.

## Milestone 4 available-target result (mandate r2)

Run `rfc0035-20260920`, segment `m4-native-closeout`; sole owner
`/root/rfc0035_m4` on `main`, initial HEAD
`c8b16090473c694d70470a1e86f51e952b1d22a8`.
Claim `79217b124758880524ac6a7e81471b1b680bee37`;
engineering source `518c384a285ef3cb873d12ea167488d105aa91b7`.
Host: macOS 26.6.2, arm64, native Qt Cocoa; Qt 6.10.3. No Intel Mac was found in
the available station inventory. Windows/Linux self-installation is outside this
milestone; unsupported/managed capability paths retain manual fallback.

### Native menu correction and public path

The signed M3 two-window observation exposed duplicate Check for Updates entries.
Per-window Qt ApplicationSpecificRole actions were each merged into the macOS
application menu. M4 now owns one AppKit item for the application-wide updater's
lifetime, with the same bounded host check gesture. Window, Settings and resident
desktop bars cannot create another native item. The adapter owns focus/state
presentation repair and clears its callback/item at destruction. Non-Cocoa
platforms retain a per-window Help action; repeated attachment is idempotent.
No network, credential, installer, state or core policy moved into presentation.

The shortest existing public desktop/menu-to-projection test passed before edits.
The expanded `application_profile_empty_desktop_native_menu_bar` runs under Cocoa
and inspects the real AppKit application menu, not a Qt action tree. It requires
exactly one visible enabled update entry, invokes it and observes the actual
Updates status panel with the original window, a second window, the original
refocused, the sibling closed, Settings focused, no workbench window and reopened
windows. Existing resident menu/activation/Dock/Command-N/Profiles checks still
pass. The fixture denies network; no live account or private release is used.
The offscreen `application_release_update_public_path` also asserts repeated
attachment creates only one action and retains the full menu/Settings, preferences,
authority/revocation, staging, two-window draft/save refusal and install course.

Initial designs were rejected by those oracles: toggling visibility of per-window
merge actions still duplicated the empty-desktop entry; moving one QMenu between
bars failed native enablement and caused a Qt-owned menu destruction crash in the
public fixture. Neither design remains. Logs `m4-menu-first-tests.log`,
`m4-shared-menu-tests.log` and `m4-menu-lifetime-tests.log` retain those failures;
`m4-menu-build.log` also retains a corrected QPointer comparison compile error.
The single native item passes the complete final course.

### Candidate verification

Commands ran from `/Users/jaeger/Documents/GitHub/BetterPullReview` using Python
3.14.7. Runtime outputs are retained in `build/rfc0035-run/`; they are not source
or release assets.

- Build: `cmake --build --preset dev --target test_profile_empty_desktop_native_menu_bar test_release_update_public_path test_release_update_transport`, with the remaining unchanged updater test targets built during the integration pass. Final log `m4-candidate-build.log`: pass.
- Integration: `ctest --preset dev -V --output-on-failure -R '^(core_release_update|application_release_update|release_update_transport_contract|release_update_native_https|release_update_native_staging|release_update_transaction|release_update_recovery|application_release_update_public_path|application_profile_empty_desktop_native_menu_bar)$'`: **9/9 pass**, 19.71 seconds, log `m4-candidate-integration.log`. All tests are network-isolated; the HTTPS row alone permits its allocated loopback fixture.
- Tests-disabled composition: `cmake --build --preset app-release --target app_better_pull_review`, `BUILD_TESTING=OFF`, Release, native arm64 configuration: pass (`m4-tests-disabled-final-build.log`). This is an ordinary ad-hoc developer build, not a new signed/notarized universal artifact. Product paths match the final candidate; the subsequent last edits only strengthen native test assertions.
- Boundary contracts: `python3 tests/scripts/architecture_boundary_tests.py` passes **139**; `python3 tests/scripts/core_boundary_tests.py` passes **4**. Logs `m4-architecture-contract.log`, `m4-core-contract.log`. Their gate/scanner source is unchanged in the candidate.
- Exact export of `518c384a` at `/tmp/bpr-rfc0035-m4-export.AJhcpB`: `cmake -DBPR_ROOT=/tmp/bpr-rfc0035-m4-export.AJhcpB -P /tmp/bpr-rfc0035-m4-export.AJhcpB/cmake/VerifyArchitectureBoundaries.cmake` and the same command with `VerifyCoreBoundary.cmake`: pass (`m4-candidate-architecture.log`, `m4-candidate-core.log`). This avoids traversing the preserved unrelated nested worktree; it does not delete or hide it.
- Canonical `format.py --dry-run`: seven changed C++/header files pass (`m4-candidate-format.log`). `diff -u src/adapters/qt/src/release_update_menu_mac.mm <(clang-format --assume-filename=release_update_menu_mac.cpp < src/adapters/qt/src/release_update_menu_mac.mm)` passes (`m4-candidate-native-format.log`); the repository C++-only format configuration otherwise refuses Objective-C++ input.
- `tidy.py --build-dir build/dev --warnings-as-errors --paths src/adapters/qt/src/release_update_panel.cpp` passes. Full-header checks for desktop/transport retain the preexisting `home_view_state.hpp:35` signed-bitwise error (`m4-tidy.log`). They are not claimed as global passes. Five changed translation units pass with the native SDK and `--header-filter=(release_update|release_update_menu_mac) --warnings-as-errors=*` (`m4-changed-surface-final-tidy.log`); the final native test additionally passes `m4-candidate-native-test-tidy.log`. New native-menu localizability and touched test naming warnings were corrected, not suppressed.
- `python3 scripts/verify_documentation.py --root .`, `python3 tests/scripts/verify_documentation_tests.py`, scoped `issue_board.py validate --rfc RFC-BPR-0035` and `git diff --check`: pass after final upkeep. Documentation tests: **64**. Final logs use `m4-final-documentation.log`, `m4-final-documentation-tests.log`, `m4-board-validation.json`.

The first nine-row integration pass was 8/9: the existing HTTPS cancellation
fixture requested cancellation at 50 ms, before the request reached the origin,
while its server asserted one complete origin request. It correctly returned
cancelled, but the fixture reported `inbound-header-truncated`. M4 now sends one
response byte then waits; the consumer cancels on that byte and asserts exactly
one byte plus the same cancelled terminal. Exact origin/count/header/canary checks
are unchanged. The 24-scenario course and all nine integration rows pass with
that deterministic in-flight cancellation (`m4-integration-tests.log` preserves
the failure; `m4-candidate-integration.log` is the final passing candidate).
The first documentation normalization omitted the required `Closeout checklist`
heading; the gate refused it and the heading was restored without changing any
criterion. Final document/board checks below use the restored heading.

### Documentation and exact acceptance boundary

The new [operator/recovery guide](../development/application-updates.md) explains
explicit release-account selection, manual/daily checks, Later versus Skip,
gesture-gated replacement, recovered text/indeterminate publication, managed/
unsupported fallback, pre-helper manual bootstrap, signed-helper recovery with
absolute paths and exact transaction identity, retained stores and no general
purge UI. The [security inventory](../security/release-updater.md) binds fixed
network destinations/credentials, process paths, limits, trust, state and cleanup.
README, component/package/foundation-security documentation and RFC-0014's scoped
no-updater exception are synchronized. RFC-0014's original wider acceptance and
status are unchanged. The code-simplifier pass removed obsolete prototype lookup
code and kept single native presentation ownership separate from host policy.

No installation/helper/receipt code changed. Reuse the exact signed M3 Apple
Silicon 0.0.350→0.0.352 upgrade, timeout, missing receipt and recovery evidence and
digests above, with its fixture authority and synthetic review state limits.
The final signed source remains `5db1d104`, not `518c384a`: it does **not** contain
M4's menu correction. The correction has actual native Cocoa test evidence; no new
production signing/notarization was necessary or performed for this bounded fix.

[[BPR-0949]] AC-02 (documentation) and AC-03 (truthful reconciliation) pass at the
source candidate. AC-01 remains pending for actual Intel older-to-newer signed
upgrade execution. Universal Mach-O slices and Rosetta cannot replace that hardware
course. RFC AC-03's live private-release route, actual native-vault credential
acquisition and live revocation remain unobserved; no account/target authority was
granted. Signing permission does not grant it. The explicit CLI recovery guide
does not claim an unbuilt wizard, attended usability verdict or Intel behavior.
No required human or independent review is being silently marked accepted.

BPR-0949 is Blocked on `intel-and-private-service-qualification`; BPR-0911/0912
remain Backlog. The [closeout](../issues/10-backlog/BPR-0912-close-rfc-0035.md)
reconciles all eight criteria. RFC-0035/index stay Proposed: RFC-0014 is Proposed
and depends on Proposed RFC-0013; canonical closure begins at RFC-0008/BPR-0242.
M4's exit and all its whole-card obligations remain pending. No plan criterion,
RFC acceptance, user amendment or unfinished obligation was removed.

### Final ownership and continuation

Self-review is complete and all task commands/native test processes are joined.
No M4 helper/app installation, review server, attended session or protected
candidate remains. The exact source export was moved recoverably to
`/Users/jaeger/.Trash/bpr-rfc0035-m4-export.AJhcpB`; shared build/log outputs and all
M3 signed packages/qualification commits remain intact. The unrelated
`.mask.lock.json` and all separately owned worktrees, especially
`/Users/jaeger/.codex/worktrees/14a8/BetterPullReview`, were not modified.
No publication, tag, push, live private-service/vault access or developer-installed
application replacement occurred.

The initial M4 checkpoint and refreshed final checkpoint are retained in
`build/rfc0035-run/`; final normalization only separates immutable parent/closeout
acceptance from mutable evidence. The strategy, plan and M4 exit are unchanged.
`m4-observation.json` binds the final upkeep revision with quiescent ownership,
`blocked` disposition and `wait` continuation for the same milestone. Resume only
with Intel hardware and authorized live account/target evidence, then bind the
final signed candidate and reconcile canonical dependency closure. No safe
independent local engineering work remains for this assigned milestone.

## HTTPS fixture timeout follow-up — 2026-10-18

The owner requested a direct repair of the intermittent
`release_update_native_https` failure and a push to main. The baseline is
`560dd3799`. Six quiet baseline runs passed (53.98 s); controlled delays in
proxy setup then reproduced the fixture's incorrect observation requirement:
the native client correctly returned `timed_out` at its unchanged 500 ms
total deadline, while the fixture required a complete origin request. That
deadline includes CONNECT and TLS setup, so a deadline-triggered disconnect
can leave either header incomplete. TLS setup also reported `EINVAL` after
the peer closed on this Mac.

The fixture now accepts an incomplete header for a confirmed timeout and
allows zero or one complete origin request in that case. `EINVAL` and
`ENOTCONN` are allowed only during TLS setup in timeout scenarios. Every
complete request still has its exact path, API-version, encoding, credential
and cookie checks; healthy and cancellation scenarios retain their request
counts. Verification runs after the proxy thread drains, so the final errors
and observations are checked together. No product transport policy or timeout
budget changed.

Two additional native scenarios withhold the CONNECT response and the TLS
handshake. All three timeout scenarios require the `timed_out` terminal,
at least the configured 500 ms to elapse, and zero response bytes. The course
now has 26 scenarios. The new `release_update_https_fixture_contract` runs
six controls covering truncated CONNECT/origin headers, TLS shutdown,
unrelated socket failures, wrong authority, header limits, completed-request
credentials and duplicate requests. The two disconnect controls fail in
four subcases against the original fixture. Separate native negative
controls replace each new stall with a successful response: both fail the
required timeout classification as intended.

Evidence on the baseline plus this follow-up:

- `cmake --build --preset dev --target test_release_update_transport --parallel 4`
  passes.
- The focused CTest selector
  `^(release_update_transport_contract|release_update_native_https|release_update_https_fixture_contract|test_network_isolation_manifest)$`
  passes 4/4 in 11.73 s. The manifest covers 797 configured tests and 728
  source registrations under `macos-sandbox-exec`.
- `ctest --preset dev --parallel 1 --no-tests=error --output-on-failure -R '^release_update_native_https$' --repeat until-fail:8`
  passes eight consecutive 26-scenario runs in 80.79 s.
- The controlled setup probe passes with 0, 350 and 600 ms of proxy delay
  after the fix, retaining the 500 ms client deadline. The latter two finish
  with zero origin requests and no fixture errors.
- Clang-format 22 and clang-tidy 22 pass the changed C++ test translation
  unit. The documentation gate and `git diff --check` pass.

Raw logs are outside Git at
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-release-https-fix-20261018-1yfz980j`.
This is macOS loopback fixture evidence; existing installed-updater,
private-service, signing and other-platform qualification limits remain.

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

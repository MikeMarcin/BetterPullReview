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

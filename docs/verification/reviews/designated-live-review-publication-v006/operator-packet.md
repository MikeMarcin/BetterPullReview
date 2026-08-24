# Designated one-shot live review publication operator packet

Status: **PENDING CAPABILITY-CRITICAL REVIEW; do not run or approve**

This v006 packet is the complete current-source successor to immutable
invalidated [v005](../designated-live-review-publication-v005/operator-packet.md).
The owner approved v004, but its only attempted lane failed closed before GUI
creation. Its unconsumed approval was deleted, and that packet forbids retry.
Capability-critical review later rejected v005 because its verifier did not
retain one capsule root and inventory identity and its live command bypassed
the read-only image. v005 has no response or approval and remains rejected
history.

BPR-0448 repaired the retained root, inventory, verifier, and shared production
launch controller. BPR-0449 repaired the image-to-runtime topology and bounded
stage-one cleanup. Both repairs have clean independent capability-critical
review. v006 regenerates every current artifact from committed tree `77ade3e`
and source identity
`4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc`.
It does not widen the one-comment authority or waive any runtime boundary.

No public target observation was repeated during v006 preparation. The packet
retains the last recorded public identity from 2026-08-23: pull request 1 was
open at base `e2a44aa8fef704de7373463222b1872e796918c6` and head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`. The attended supervisor must
observe and match that complete identity before GUI creation. Any current
difference is an abort, not authority to update this packet in place.

One value is inherited deliberately: `approval_id`
`aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`.
Preparation inspected the replay namespace read-only. Its identity remains
`473412d4ffb9031cb291694cf7bd89b504a020f26e2a5b36c7e64fa90225b87c`,
and its only file is the namespace identity record; no spent record exists.
Resetting or initializing the namespace would mint a different identity and
invalidate this packet.

After a clean review, this packet will ask one bounded decision: whether to
authorize this exact tests-disabled BetterPullReview build and exact stored
image to publish one ordinary source-line review comment to the disposable
pull request `MikeMarcin/BPR-Test#1` at the head above.

Approval can authorize at most the first async-host request whose application
session, workbench generation, designated target, freshly authenticated actor,
comparison, file, right-side semantic anchor, resolved path, and current head
all match. It does not authorize another repository, pull request, head,
executable, source build, image, comment, retry after an indeterminate POST, or
any non-review mutation. It does not close BPR-0142, BPR-0235, BPR-0256, or
RFC-BPR-0008.

## Why a person is required

The application can verify exact build and target identity and enforce one-use
admission. It cannot decide that a public comment should be created or judge
the resulting GitHub UI. A person must approve the exact subject and image,
enter any credential through the Qt sensitive prompt, select the disposable
source line, publish once, and inspect that first comment in GitHub's web UI.
Posting a reply is outside this one-comment authority.

## Bound evidence

Do not use these artifacts until a fresh capability-critical review accepts
this packet and the owner approves its exact decision:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/bpr0256-v006-release/designated-live/4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc/capsule/BetterPullReview.app/Contents/MacOS/BetterPullReview` | `sha256:a0ff772d65a3ae55a65aee664dd6883f415d7783396c7c5f266dda28aeac37bb` | 7,718,720 |
| `build/bpr0256-v006-release/designated-live/4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor` | `sha256:c46f8e69c5265c2b57afe3e7a6243fd89c9f86e9076d8604c47e7525e6f3df5c` | 240,496 |
| `build/bpr0256-v006-release/designated-live/4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc/capsule.inventory` | `sha256:deae04e7bf4f24cfc4ea34bcdc578bf37a83bb763a7e37fdbc5206b6c41ca986` | 11,112 |
| `build/bpr0256-v006-release/designated-live/4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc/capsule.dmg` | `sha256:2601e22323168ca82ea173789fb92eafb3dc3698d159acb5107643eabdbf72fb` | 30,013,110 |
| `build/bpr0256-v006-release/designated-live/4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc/capsule.dmg.sha256` | `sha256:437a11700a19b9d25a5652d804d8f7f21b7c1e95d1854854f844fcb65881f678` | 65 |
| `scripts/designated_live_macos_capsule.py` | `sha256:b969b90a26b87d862e04c5c2064636ffd875ef3361ff392cfa5f4f8df998da05` | 60,716 |
| `approval-subject.txt` | `sha256:984a1ab0ce7971a369052be1be60ef3d9c23b7161cb2e29ab368e5e9cc4dc1bd` | 1,496 |
| `source-manifest.txt` | `sha256:4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc` | 4,301 |

The 37-entry source manifest is byte-identical to the producer-generated
manifest. Its digest names the artifact directory and is compiled as
`BPR_ATTENDED_REVIEW_SOURCE_SHA256` into both capsule binaries. The source
surface includes the retained supervisor identity/controller sources, the
stage-zero preloader module, the live-isolation CMake policy, and the Python
launcher. The producer cache records `Release`, `BUILD_TESTING=OFF`,
`BPR_BUILD_QT=ON`, `BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE=ON`, offline
dependency selection, and this repository as its source root.

Changing any bound source entry, application, supervisor, inventory, image,
digest file, launcher, approval subject, generated CTest plan, or packet byte
invalidates the corresponding claim. DMG creation is not byte-reproducible.
Another fresh build may have a different image SHA even when every source entry
matches; only the exact stored `2601e223…f72fb` image belongs to v006.

## Runtime topology and limits

The sole opt-in command starts `/usr/bin/python3 -I -S` with a preloader
captured in the generated CTest execution plan. The preloader opens the
producer-bound launcher without following a symlink, captures one bounded byte
sequence, verifies its literal SHA-256, and executes those captured bytes.
Configuration likewise captures `source-manifest.txt` once and uses that same
byte sequence for its digest and entries.

Stage zero retains the exact image descriptor, verifies the literal image
digest, and mounts that image read-only below the owner-private mount parent
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v006-live-mount-parent`.
It verifies and retains the mounted inventory and runtime identity before
starting the supervisor/application from the mount. Stage one runs in its own
process group with a 1,500-second bound inside CTest's 1,800-second timeout.
Normal exit, failure, timeout, `SIGTERM`, `SIGINT`, and `SIGHUP` all pass through
post-child image/runtime revalidation, bounded group TERM/KILL, complete reap,
and ordinary non-force detach.

These mechanisms have three deliberate limits:

- the mounted filesystem is read-only, but the invoking owner can still open
  the backing DMG for writing; post-child identity/digest checks detect
  persistent drift but cannot prevent transient modified backing bytes from
  influencing execution;
- `hdiutil attach` consumes a pathname rather than the retained image
  descriptor, so the DiskImages attach boundary is not descriptor-atomic; and
- the generated CTest manifest and cache are the trusted same-UID execution
  plan; repository-local code cannot protect that plan from an actor allowed
  to rewrite it.

Approval of v006 accepts these limits for this one disposable attended course.
It does not convert them into stronger guarantees.

## Approval subject

The schema-3 approval subject binds the application, source, supervisor,
inventory, replay namespace, and exact target. Schema 3 does not contain image,
launcher, or mount-parent fields; the packet and exact operator response bind
those outer stage-zero values without changing the application parser.

The retained replay values are:

- `replay_namespace_path=/Users/jaeger/Library/Application Support/BetterPullReview/BetterPullReview/designated-live-review-replay-state-v1`
- `replay_namespace_id=473412d4ffb9031cb291694cf7bd89b504a020f26e2a5b36c7e64fa90225b87c`
- `approval_id=aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`

The namespace directory is owner-private mode `0700`; its identity record is
mode `0600`, schema `bpr.designated_live_review_replay.v1`, version 1, and has
the exact path and namespace ID above. No spent record exists. Do not run
`--initialize-or-reset-designated-live-replay-state` under this packet.

The exact approval statement embedded in `approval-subject.txt` is:

> I authorize this exact BetterPullReview build to publish one source-line
> review comment to MikeMarcin/BPR-Test#1 at head
> 17990ba0c2fb50338141f35c0717f0f99639c7ba.

The subject binds repository database ID `1328277631`, repository node ID
`R_kgDOTyvofw`, pull-request database ID `4246446933`, pull-request node ID
`PR_kwDOTyvof879G6NV`, base-repository ID `1328277631`, open state, base `main`
at `e2a44aa8fef704de7373463222b1872e796918c6`, and head branch
`codex/disposable-live-review-20260810` at the head above.

## Unattended verification

The v006 producer was configured and built with this exact command shape:

```sh
cmake -S . -B build/bpr0256-v006-release --fresh -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DBPR_BUILD_QT=ON \
  -DBPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE=ON \
  -DBPR_DEPENDENCY_OFFLINE=ON \
  -DBPR_ENABLE_CODING_TARGETS=OFF \
  -DCPM_SOURCE_CACHE:PATH=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache \
  -DBPR_QT_ROOT:PATH=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache/bprqt/6.10.3/6.10.3/macos \
  -DCMAKE_PREFIX_PATH:PATH=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache/bprqt/6.10.3/6.10.3/macos
cmake --build build/bpr0256-v006-release \
  --target designated_live_macos_capsule --parallel 6
```

The target built 138 steps, verified the capsule, generated the exact image and
digest file, mounted the image, launched the production-shaped probe only from
that mount, and detached ordinarily. No attached v006 device remained. The
protected development executable remained byte-identical before and after at
52,067,088 bytes and SHA-256
`cac2d05d5c951e58d0d5144ca8bb9eebd60428db46a8c4bbf2c20fb084d406ff`;
no development configure or build command ran during capsule production.

The safe local matrices passed:

- 12/12 authority, composition, capsule, provenance, retained-identity,
  isolation, and architecture selectors in 259.43 seconds;
- 6/6 provider, native-HTTPS, SQLite recovery, restart, and hermetic public
  observer selectors in 13.60 seconds;
- 44/44 hostile capsule/stage-zero tests in 26.698 seconds;
- 37/37 hostile network-isolation tests in 0.239 seconds;
- 126/126 hostile architecture tests in 65.803 seconds; and
- the direct CMake architecture gate and 399-file formatting gate.

The default development manifest contains 199 tests, zero
`live_review_mutation_designated_target` entries, and zero
`bpr_live_mutation` labels. No safe matrix performs public observation,
credential access, an attended launch, or mutation.

The repository-wide warnings-as-errors tidy target exits nonzero on the same
six unique pre-existing diagnostics previously recorded in untouched files:

- `tests/end_to_end/public_path_poll.hpp:1:1` and
  `tests/end_to_end/public_path_wait.hpp:1:1` use `#pragma once`;
- `tests/git_contract/windows_system_root.hpp:1:1` uses `#pragma once`;
- `tests/git_contract/git_cache_contract_tests.cpp:689:8` copies an unmodified
  `QStringList`;
- `tests/git_contract/git_process_contract_tests.cpp:3281:14` retains the
  nonconforming local name `repeated_identity_hex`; and
- `tests/platform/macos_service_slot_handshake_tests.cpp:428:13` retains the
  nonconforming local name `self_code`.

Header findings repeat for translation units that include the same header;
the six locations above are the complete unique baseline. None is in the v006
documentation-only change set. The final documentation validator reports
`DOCUMENTATION_OK`, all 107 documentation self-tests pass, and
`git diff --check` passes.

## Static manifest inspection

Preparation created a private mode-`0700` directory containing a mode-`0600`,
deliberately invalid `not-an-approval` placeholder. It configured the opt-in
graph with the exact v006 image, literal image SHA, mount parent, producer build,
and placeholder, then ran only `ctest --show-only=json-v1`. The placeholder was
deleted immediately afterward. It was not an approval and could not authorize
the application.

The inspected graph contains 200 tests, exactly one test named
`live_review_mutation_designated_target`, and exactly one
`bpr_live_mutation` label. That test has a 1,800-second timeout and the exact
command:

1. fixed `/usr/bin/python3 -I -S -c` captured-byte preloader;
2. launcher path and literal `b969b90a…98da05` launcher digest;
3. `stage-zero` and exact v006 image path;
4. literal `2601e223…f72fb` image digest;
5. exact owner-private mount parent and `4f5a9d57…faf36fc` source identity; and
6. only `--attended-launch --approval <absolute approval path>` after those
   outer bindings.

The command removes Python, dynamic-loader, and GitHub credential variables and
has no CTest pass/skip/disable or exit-verdict override. The test was not run.

After a valid approval is materialized, reconfigure the graph with its absolute
path and inspect it again without executing it:

```sh
cmake -S . -B build/bpr0256-v006-live-manifest --fresh -G Ninja \
  -DBUILD_TESTING=ON \
  -DBPR_BUILD_QT=ON \
  -DBPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST=ON \
  -DBPR_DESIGNATED_LIVE_MUTATION_APPROVAL=/absolute/resolved/private/approval.txt \
  -DBPR_DESIGNATED_LIVE_MUTATION_IMAGE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v006-release/designated-live/4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc/capsule.dmg \
  -DBPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256=2601e22323168ca82ea173789fb92eafb3dc3698d159acb5107643eabdbf72fb \
  -DBPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v006-live-mount-parent \
  -DBPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v006-release \
  -DBPR_DEPENDENCY_OFFLINE=ON \
  -DCPM_SOURCE_CACHE:PATH=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache \
  -DBPR_QT_ROOT:PATH=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache/bprqt/6.10.3/6.10.3/macos
ctest --test-dir build/bpr0256-v006-live-manifest --show-only=json-v1
```

Abort if any property differs from the inspected shape, if another live entry
or label exists, or if the command names a plain supervisor/inventory, direct
GUI, regression runner, fixture/tests-enabled app, digest file in place of the
literal image digest, verdict override, or credential environment.

## Independent capability-critical review

Pending. A fresh critical reviewer must recompute every checked-in and runtime
identity, replay the safe Release and manifest evidence, try to substitute the
manifest/launcher/image/runtime during their capture and child lifetime, and
adjudicate the explicit backing-DMG, pathname-attach, and trusted-plan limits.
No operator response is valid before that review accepts the exact packet.

## Decision and approval materialization

Inactive while this packet is pending review. After a clean review, first
verify every artifact hash and size above. If the owner does not return the
exact unqualified approval alternative below, stop without creating a file.

If approved, create a fresh private directory and materialize exactly the
`approval-subject.txt` bytes followed by this one final line:

```text
subject_sha256=984a1ab0ce7971a369052be1be60ef3d9c23b7161cb2e29ab368e5e9cc4dc1bd
```

The resulting approval must end in one newline, be a new regular file owned by
the operator with mode `0600`, have exactly one hard link, and have an immediate
parent owned by the operator with mode `0700`. Use an absolute resolved path
with no symlink component. Do not put a credential, nonce, comment body, shell
expansion, or additional text in the file. The application consumes and
unlinks that exact inode before any grant exists; it cannot be reused.

## Attended procedure

Inactive until this packet is capability-critical clean, the owner returns the
exact approval below, the approval artifact is materialized byte-exactly, and
the final static manifest inspection passes.

1. Close other BetterPullReview processes. Confirm the designated PR remains
   disposable and intended for this check.
2. Start only the fixed lane:

   ```sh
   ctest --test-dir build/bpr0256-v006-live-manifest \
     --output-on-failure \
     -R '^live_review_mutation_designated_target$'
   ```

3. If startup refuses or stage zero reports any identity, mount, child, or
   detach failure, stop. Do not recreate the approval or retry this packet.
4. In the application, choose the GitHub.com profile for the authorized actor.
   Enter any write-scoped credential only into the Qt sensitive prompt. Never
   place it in Terminal, CTest configuration, argv, environment, the approval,
   a log, or this packet.
5. Open only `MikeMarcin/BPR-Test#1`. Confirm its open state, base, and head are
   the exact bound values. Select one right-side source line in the disposable
   text fixture and enter exactly
   `BetterPullReview attended round-trip verification.` as the comment body.
6. Press Publish once. Do not click Publish again, retry an indeterminate
   result, edit, delete, resolve, or publish a second comment.
7. Open `https://github.com/MikeMarcin/BPR-Test/pull/1` in GitHub's web UI.
   Record the full comment text, author shown, source location, and whether it
   has any BetterPullReview marker, badge, or footer. Do not reply, edit,
   resolve, or delete under this packet.
8. Close BetterPullReview normally and retain the complete non-secret result
   record. Never return a credential or secret-bearing log.

## Abort conditions

Abort without publishing if any artifact hash/size, source entry, image digest,
launcher digest, manifest property, mount-parent safety property, target
identity, PR state/head, source line, comment body, or credential surface
differs. Also abort on redirect, public-observation failure, approval refusal or
substitution, supervisor-authentication refusal, image/runtime drift, forced or
failed detach, unexpected network/credential prompt, a missing exact Qt prompt,
an already-consumed approval, more than one enabled Publish action, target/head
drift, or an indeterminate POST result. Do not materialize a second approval,
modify the stored DMG, rewrite the generated CTest plan, or reset the replay
namespace under this packet.

The minted authority expires 15 minutes after the approval is consumed. The
1,800-second CTest timeout bounds the lane; it does not extend authority. A
session that consumes the approval and pauses can lose authority mid-run with
no retry admitted. Do not begin without 15 uninterrupted minutes.

Target-native Linux and Windows evidence remains deferred. This packet is for
the exact bound macOS build and image only.

## Operator response

No response is valid while the status is pending capability-critical review.
After the packet becomes operator-ready without any bound byte changing,
return exactly one of these decisions:

> I approve the exact one-use approval subject
> sha256:984a1ab0ce7971a369052be1be60ef3d9c23b7161cb2e29ab368e5e9cc4dc1bd
> for BetterPullReview binary
> sha256:a0ff772d65a3ae55a65aee664dd6883f415d7783396c7c5f266dda28aeac37bb
> and signed-capsule supervisor
> sha256:c46f8e69c5265c2b57afe3e7a6243fd89c9f86e9076d8604c47e7525e6f3df5c
> and capsule inventory
> sha256:deae04e7bf4f24cfc4ea34bcdc578bf37a83bb763a7e37fdbc5206b6c41ca986
> and source manifest
> sha256:4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc
> and stage-zero launcher
> sha256:b969b90a26b87d862e04c5c2064636ffd875ef3361ff392cfa5f4f8df998da05
> and exact stored read-only image
> sha256:2601e22323168ca82ea173789fb92eafb3dc3698d159acb5107643eabdbf72fb
> mounted below
> /Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v006-live-mount-parent
> under the backing-DMG, pathname-attach, and trusted-CTest-plan limits stated
> in this packet, to publish one source-line review comment to
> MikeMarcin/BPR-Test#1 at head
> 17990ba0c2fb50338141f35c0717f0f99639c7ba.

or:

> I reject this exact one-use live review publication.

After an approved run, return the command result and the full non-secret UI
observations requested in step 7. If blocked, return the exact obstruction.
Packet preparation or a partial response is not an approval or verdict.

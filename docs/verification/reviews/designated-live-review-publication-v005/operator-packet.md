# Designated one-shot live review publication operator packet

Status: **INVALIDATED by capability-critical review; do not run or approve**

This v005 packet is the current-source successor to immutable answered
[v004](../designated-live-review-publication-v004/operator-packet.md). The
owner approved v004, but its one attempted lane failed closed before GUI
creation, its unconsumed approval file was deleted, and its procedure forbids a
retry. Subsequent committed source changes moved three of v004's 31 bound
files. Its response remains valid only for the historical bytes it names and
cannot authorize today's build.

v005 bound a fresh tests-disabled Release capsule built from committed tree
`bb8212a` and source digest
`0608914ff88f395a07b5de937e18a9191c6cd0a6f6fef3ea9be38db9d3941114`.
The designated target was recorded publicly observable on 2026-08-23, with PR
1 open and its head unchanged from v004. No new live observation, approval,
credential access, supervisor launch, or mutation occurred while preparing
v005.

Fresh capability-critical review found that these exact binaries and command
topology do not maintain one capsule-root identity, one inventory identity, or
the read-only image through the runtime. This packet is immutable rejected
history. Every imperative section below is retained only to document the
candidate that was reviewed; none is authority to materialize an approval,
configure a live manifest, launch the supervisor, or publish.

The mechanism is unchanged. BPR-0261 answers replay authority, BPR-0351
supervisor launch provenance, BPR-0352 supervisor/inventory identity binding,
BPR-0354 the nested check/use window, and BPR-0263 indeterminate recovery. v005
regenerates the digest-bound artifacts after the current-tree drift; it does
not widen authority or change production source.

One value is inherited deliberately: `approval_id`
`aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`.
The replay namespace was inspected read-only during preparation. It retains its
existing identity and contains zero spent records, so the approval ID remains
unspent. Any prior consume would make the application refuse as replayed.
Minting a fresh ID would require resetting the namespace, which this packet
forbids.

The rejected candidate would have asked one bounded decision after a clean
review: whether to authorize this exact tests-disabled
BetterPullReview build to publish one ordinary source-line review comment to
the already designated disposable pull request `MikeMarcin/BPR-Test#1` at head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`.

Approval can authorize at most the first async-host request whose application
session, workbench generation, designated target, freshly authenticated actor,
comparison, file, right-side semantic anchor, resolved path, and current head
all match. It does not authorize another repository, pull request, head,
executable, source build, comment, retry after an indeterminate POST, or any
non-review mutation. It does not close BPR-0142, BPR-0235, BPR-0256, or
RFC-BPR-0008.

## Why a person is required

The application can prove exact build/target identity and enforce one-use
admission, but it cannot decide that a public comment should be created or
judge the resulting GitHub UI. A person must approve the exact subject, enter
any credential through the Qt sensitive prompt, select the disposable source
line, publish once, and inspect that first comment in GitHub's web UI. Posting
a reply is outside this one-comment authority.

## Bound evidence

Run only these exact immutable artifacts after both the capability-critical
review and the exact operator approval:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/bpr0256-release/designated-live/0608914ff88f395a07b5de937e18a9191c6cd0a6f6fef3ea9be38db9d3941114/capsule/BetterPullReview.app/Contents/MacOS/BetterPullReview` | `sha256:b071b432f4682803ec0c63ed8977cfa6d469f2cb8a5d0ccf40dabcc0bb1fa18e` | 7,718,720 |
| `build/bpr0256-release/designated-live/0608914ff88f395a07b5de937e18a9191c6cd0a6f6fef3ea9be38db9d3941114/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor` | `sha256:1b6778a292377ead1d12a11a71cd316f7e63a1bf19d5583800cb796619a21e1d` | 197,200 |
| `build/bpr0256-release/designated-live/0608914ff88f395a07b5de937e18a9191c6cd0a6f6fef3ea9be38db9d3941114/capsule.inventory` | `sha256:09afbd31d5893e699a1c861e2db8544bc9a21539bda72b3186b197379b25129e` | 11,112 |
| `approval-subject.txt` | `sha256:6f158a0801c22219b77a3dfe44dac390de6d77fc520ab419565678ab72847bd3` | 1,496 |
| `source-manifest.txt` | `sha256:0608914ff88f395a07b5de937e18a9191c6cd0a6f6fef3ea9be38db9d3941114` | 3,615 |

The source-manifest digest is the exact `BPR_ATTENDED_REVIEW_SOURCE_SHA256`
compiled into both capsule binaries. The 31 paths were read from
`bpr_attended_review_source_surface` in `src/app/CMakeLists.txt`, each current
file was hashed, and the `path=sha256` lines were concatenated in list order.
Those exact 3,615 checked-in bytes hash to
`0608914ff88f395a07b5de937e18a9191c6cd0a6f6fef3ea9be38db9d3941114`,
which is also present in the fresh Release build graph and capsule binaries.
The producer cache records `BUILD_TESTING=OFF`, `BPR_BUILD_QT=ON`,
`BPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE=ON`, `Release`, and source root
`/Users/jaeger/Documents/GitHub/BetterPullReview`.

Rebuilding the capsule, changing any source-manifest entry, changing the
supervisor or inventory, or changing the approval subject invalidates this
packet. Before launching the GUI, the supervisor verifies the complete signed
capsule, performs the unauthenticated public target observation, creates the
app suspended, checks the child's pid-keyed signing identity, and revalidates
the retained capsule identities through its retained descriptor before
resuming it. The audit-token Mach handshake completes after resume and before
`QApplication` exists on the child side, because a suspended process cannot
answer.

The approval subject is schema version 3 and binds the supervisor, inventory,
and replay namespace:

- `replay_namespace_path=/Users/jaeger/Library/Application Support/BetterPullReview/BetterPullReview/designated-live-review-replay-state-v1`
- `replay_namespace_id=473412d4ffb9031cb291694cf7bd89b504a020f26e2a5b36c7e64fa90225b87c`
- `approval_id=aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`

The namespace and its identity record were read without modification. The
directory is owner-private mode `0700`; its identity record is mode `0600`,
schema `bpr.designated_live_review_replay.v1`, version 1, and has the exact path
and namespace ID above. No spent record exists. **Do not run
`--initialize-or-reset-designated-live-replay-state`**: reset mints a fresh
namespace identity and invalidates this subject and packet by construction.

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

A fresh configure and build used this exact command shape:

```sh
cmake -S . -B build/bpr0256-release -G Ninja --fresh \
  -DBUILD_TESTING=OFF \
  -DBPR_BUILD_QT=ON \
  -DBPR_BUILD_DESIGNATED_LIVE_MACOS_CAPSULE=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache
cmake --build build/bpr0256-release --target designated_live_macos_capsule
```

The graph compiled and linked 47 steps, generated the capsule under the exact
source digest, and returned `designated-live-macos-capsule:pass`.

The exact eleven-selector replay passed 11/11 in 244.79 seconds:

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(application_designated_review_authority_bootstrap|application_review_mutation_application_host|application_review_round_trip_composition_contract|application_composition_contract|designated_live_macos_capsule_contract|macos_supervisor_provenance|macos_capsule_identity|test_network_isolation_(structure_contract|manifest)|architecture_(boundaries|boundary_contract))$'
```

The existing development executable was measured before the Release build and
after the capsule/image and matrix verification. It remained 52,067,088 bytes
with SHA-256
`cac2d05d5c951e58d0d5144ca8bb9eebd60428db46a8c4bbf2c20fb084d406ff`.
No `build/dev` configure or build command ran.

The adjacent provider/native-HTTPS/SQLite recovery matrix passed 6/6 in 13.48
seconds, and the hermetic designated-public-observer fixture passed 1/1 in 0.32
seconds. A direct JSON inspection of the default configured manifest found 198
tests, zero entries named `live_review_mutation_designated_target`, and zero
`bpr_live_mutation` labels. These were hermetic regressions only; they did not
observe GitHub or enable the live lane.

## Independent capability-critical review

The fresh independent review returned **three blocking findings**:

1. The supervisor runs the static verifier before opening and capturing the
   capsule root. A whole-root substitution in that interval is therefore
   captured as the new baseline and remains invisible to the final
   `unchanged()` check.
2. The Python verifier and the later application-bound inventory digest reopen
   the inventory pathname separately. Inventory A can be verified and then
   replaced by operator-approved inventory B before the digest is computed.
3. `stage-zero` mounts, verifies, and detaches the read-only image without
   launching stage one. The registered live command still reaches the writable
   plain-directory capsule, so the read-only mount does not protect the
   supervisor/application lifetime or post-resume resource loads.

The exact prescribed gates still passed: authority/composition 11/11,
provider/native-HTTPS/SQLite 6/6, hermetic public observer 1/1, a 198-test
default manifest with zero live entries and labels, capsule and stage-zero
verification, documentation, 107 documentation self-tests, and diff hygiene.
Those tests are false-green for the three injected interleavings above.

The reviewer independently recomputed all five bound identities, the 31-file
manifest, Release/tests-disabled cache selection, compiled source digest,
owner-private replay identity, and zero spent records. No response, approval,
attempt receipt, credential access, live launch, or mutation exists. Passing
artifact checks do not make the defective execution topology safe.

## Read-only image evidence

The digest-bound read-only capsule image was produced and verified from these
exact capsule bytes as separate release evidence. `capsule.dmg` is
`sha256:a6f55ee4cd1a1d39a0c238af931fc14c5df453a2b423a073298c8f85e71d3be9`
at 29,983,469 bytes. `stage-zero` checked that exact image digest, mounted it
read-only under a fresh owner-private temporary parent, verified the inventory
and source binding from inside the mount, detached without force, and returned
`designated-live-macos-capsule:pass`. The registered CTest lane still launches
the plain-directory capsule. Capability-critical review determined that this
is a blocking runtime-topology defect, not a limit that packet prose can waive.

## Decision and approval materialization

This section is permanently inactive for v005. Do not create an approval file
from this subject. A successor requires repaired production code and command
topology, a complete rebuild and identity regeneration, fresh independent
review, and a new exact operator decision.

The rejected procedure would then have created a fresh private directory and
materialized exactly the subject bytes plus this one final line:

```text
subject_sha256=6f158a0801c22219b77a3dfe44dac390de6d77fc520ab419565678ab72847bd3
```

The resulting approval must end in one newline, be a new regular file owned by
the operator with mode `0600`, have exactly one hard link, and have an immediate
parent owned by the operator with mode `0700`. Use an absolute resolved path
with no symlink component. Do not put a credential, nonce, comment body, shell
expansion, or additional text in the file. The application consumes and
unlinks that exact inode before any grant exists; it cannot be reused.

## Static manifest inspection

Historical candidate procedure only; **do not run it**. The rejected packet
would have configured the opt-in manifest with an approval's absolute path:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
cmake -S . -B build/bpr0256-live-manifest -G Ninja \
  -DBUILD_TESTING=ON \
  -DBPR_BUILD_QT=ON \
  -DBPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST=ON \
  -DBPR_DESIGNATED_LIVE_MUTATION_APPROVAL=/absolute/resolved/private/approval.txt \
  -DBPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release/designated-live/0608914ff88f395a07b5de937e18a9191c6cd0a6f6fef3ea9be38db9d3941114/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor \
  -DBPR_DESIGNATED_LIVE_MUTATION_INVENTORY=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release/designated-live/0608914ff88f395a07b5de937e18a9191c6cd0a6f6fef3ea9be38db9d3941114/capsule.inventory \
  -DBPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release \
  -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache
ctest --test-dir build/bpr0256-live-manifest --show-only=json-v1
```

Confirm the manifest contains exactly one test named
`live_review_mutation_designated_target`, its only label is
`bpr_live_mutation`, its timeout is 1,800 seconds, and its command is the exact
bound supervisor followed only by `--attended-launch`, `--inventory`, the bound
inventory, `--approval`, and the approval path. Abort if it names the GUI
executable directly, the regression runner, a fixture or tests-enabled app,
another live test or label, a verdict override, or any credential environment.

This inspection proves the CTest wiring only. The pre-`QApplication` Mach
handshake and post-verification capsule-identity revalidation are enforced
inside the supervisor and application and are not visible to `--show-only`.

## Attended procedure

This rejected procedure is permanently inactive. It is retained only to show
what the reviewer assessed; v005 can never become operator-ready.

1. Close other BetterPullReview processes. Confirm the designated PR remains
   disposable and intended for this check.
2. Start only the fixed lane:

   ```sh
   ctest --test-dir build/bpr0256-live-manifest \
     --output-on-failure \
     -R '^live_review_mutation_designated_target$'
   ```

3. If the application refuses startup, stop. Do not recreate the consumed
   approval or retry under this packet. A supervisor-authentication refusal
   deliberately prints the same `STARTUP_FAILED` text as a malformed approval;
   either way the answer is stop.
4. In the application, choose the GitHub.com profile for the authorized actor.
   Enter any write-scoped credential only into the existing Qt sensitive
   credential prompt. Never place it in Terminal, CTest configuration, argv,
   environment, the approval, a log, or this packet.
5. Open only `MikeMarcin/BPR-Test#1`. Confirm its head is the exact bound head.
   Select one right-side source line in the disposable text fixture and enter
   exactly `BetterPullReview attended round-trip verification.` as the comment
   body.
6. Press Publish once. Do not click Publish again, retry an indeterminate
   result, edit, delete, resolve, or publish any second comment.
7. Open `https://github.com/MikeMarcin/BPR-Test/pull/1` in GitHub's web UI.
   Record the full comment text, author shown, source location, and whether it
   has any BetterPullReview marker, badge, or footer. Do not reply, edit,
   resolve, or delete under this packet.
8. Close BetterPullReview normally and retain the complete non-secret result
   record. Never return a credential or secret-bearing log.

## Abort conditions

Abort without publishing if any artifact hash/size, manifest property, target
identity, PR state/head, source line, comment body, or credential surface
differs. Also abort on redirect, public-observation failure, approval refusal
or substitution, supervisor-authentication refusal, unexpected network or
credential prompt, a missing exact Qt prompt, an already-consumed approval,
more than one enabled Publish action, target/head drift, or an indeterminate
POST result. Do not materialize a second approval and do not reset the replay
namespace under this packet.

**The minted authority expires 15 minutes after the approval is consumed.**
The 1,800-second CTest timeout is the lane's bound, not the authority's. A
session that consumes the approval and then pauses can lose authority mid-run
with no retry admitted. It fails closed, and the approval is spent either way.
Do not begin the attended procedure without 15 uninterrupted minutes.

Target-native Linux and Windows evidence remains deferred. This packet is for
the bound macOS build only and makes no cross-platform claim.

## Operator response

No operator response is valid for v005. Do not approve, reject, execute, or
reuse any text from this invalidated packet. Its exact subject remains only as
evidence of the bytes the reviewer rejected. A regenerated successor will ask
its own digest-bound decision after its own clean capability-critical review.

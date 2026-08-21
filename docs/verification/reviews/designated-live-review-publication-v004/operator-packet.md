# Designated one-shot live review publication operator packet

Status: **operator-ready after a no-blocker capability-critical review; exact operator decision pending**

This v004 packet replaces immutable invalidated
[v003](../designated-live-review-publication-v003/operator-packet.md), which
must not be run or approved after the product owner authorized BPR-0410 to
change its bound `src/app/main.cpp`. The approval subject, capsule, inventory,
and source manifest below describe committed tree `a83bfd0`. They passed their
unattended gates and an independent capability-critical review of these exact
bytes returned no blocking finding.

The mechanism is unchanged. BPR-0261 answers replay authority, BPR-0351
supervisor launch provenance, BPR-0352 supervisor/inventory identity binding,
BPR-0354 the nested check/use window, and BPR-0263 indeterminate recovery. v004
regenerates only the digest-bound artifacts after BPR-0410's authorized source
change.

One value is inherited deliberately: `approval_id`
`aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`.
The replay namespace was inspected read-only during preparation. It retains its
existing identity and contains zero spent records, so the approval ID remains
unspent. Any prior consume would make the application refuse as replayed.
Minting a fresh ID would require resetting the namespace, which this packet
forbids.

This packet asks one bounded decision: whether to authorize this exact tests-disabled
BetterPullReview build to publish one ordinary source-line review comment to
the already designated disposable pull request `MikeMarcin/BPR-Test#1` at head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`.

Approval can authorize at most the first async-host request whose application
session, workbench generation, designated target, freshly authenticated actor,
comparison, file, right-side semantic anchor, resolved path, and current head
all match. It does not authorize another repository, pull request, head,
executable, source build, comment, retry after an indeterminate POST, or any
non-review mutation. It does not close BPR-0142, BPR-0235, or RFC-BPR-0008.

## Why a person is required

The application can prove exact build/target identity and enforce one-use
admission, but it cannot decide that a public comment should be created or
judge the resulting GitHub UI. A person must approve the exact subject, enter
any credential through
the Qt sensitive prompt, select the disposable source line, publish once, and
inspect that first comment in GitHub's web UI. Posting a reply is outside this
one-comment authority.

## Bound evidence

Run only these exact immutable artifacts after the exact operator approval:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/bpr0256-release/designated-live/c5023d4f81b02c6663afa813a2a3e1876ddfcce481537feb59ee24f974fa9f79/capsule/BetterPullReview.app/Contents/MacOS/BetterPullReview` | `sha256:2a1c65bf1249ba25333f949deac674ded204cd9ce0653d7fdbb165fcbb4a2216` | 7,682,464 |
| `build/bpr0256-release/designated-live/c5023d4f81b02c6663afa813a2a3e1876ddfcce481537feb59ee24f974fa9f79/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor` | `sha256:6ff0d7c7949dc446e8d4e63d04d2eda762d1d6155aab84fdbb6beaae97f35a6f` | 197,200 |
| `build/bpr0256-release/designated-live/c5023d4f81b02c6663afa813a2a3e1876ddfcce481537feb59ee24f974fa9f79/capsule.inventory` | `sha256:ca4fbe45e2a6ed6198a90b1b114b4617c1008bc8739c3662c2f4e7f1665a25ee` | 11,112 |
| `approval-subject.txt` | `sha256:7daa46aa18dfc0cd804c9060f1b790089e17a1db972d074b38e61a78769dcb39` | 1,496 |
| `source-manifest.txt` | `sha256:c5023d4f81b02c6663afa813a2a3e1876ddfcce481537feb59ee24f974fa9f79` | 3,615 |

The source-manifest digest is the exact `BPR_ATTENDED_REVIEW_SOURCE_SHA256`
compiled into both capsule binaries. The 31 paths were read from
`bpr_attended_review_source_surface` in `src/app/CMakeLists.txt`, each file was
hashed, the `path=sha256` lines were concatenated in list order, and those exact
3,615 bytes were hashed. That reproduces
`c5023d4f81b02c6663afa813a2a3e1876ddfcce481537feb59ee24f974fa9f79`,
which is also present in the fresh Release build graph and capsule binary. The
producer cache records `BUILD_TESTING=OFF`, `BPR_BUILD_QT=ON`, `Release`, and
source root `/Users/jaeger/Documents/GitHub/BetterPullReview`.

Rebuilding the capsule, changing any source-manifest entry, changing the
supervisor or inventory, or changing the approval subject invalidates this
packet. Before launching the GUI, the supervisor verifies the complete signed
capsule, performs the unauthenticated public target observation, creates the app
suspended, checks the child's pid-keyed signing identity, and revalidates the
retained capsule identities through its retained descriptor before resuming it.
The audit-token Mach handshake completes after resume and before `QApplication`
exists on the child side, because a suspended process cannot answer.

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

The first exact 11-selector replay passed 9/11 and exposed a stale expected
production-source aggregate in
`tests/end_to_end/application_review_round_trip_composition_contract.cmake`.
BPR-0410 had changed `src/app/main.cpp`; the contract still expected
`075589a92d9f051718bbaa38ae6f2d62abfc8bbfd6fcfbfc843b58578e1f1c0b`
instead of the directly computed current aggregate
`f6d1c03d7fe4f50abcdaea2e93fec93d341f4325882f992f7f3d629643c2595c`.
Both composition selectors failed rather than accepting the drift.

The contract pin was corrected without changing any of the 31 bound production
sources. The two failed selectors then passed 2/2 in 122.28 seconds, and the
complete exact replay passed 11/11 in 217.92 seconds:

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(application_designated_review_authority_bootstrap|application_review_mutation_application_host|application_review_round_trip_composition_contract|application_composition_contract|designated_live_macos_capsule_contract|macos_supervisor_provenance|macos_capsule_identity|test_network_isolation_(structure_contract|manifest)|architecture_(boundaries|boundary_contract))$'
```

The development executable was measured before the Release build, after the
capsule/image verification, and after the matrix. It remained 51,303,392 bytes
with SHA-256
`333e88c9298f5a57182b7b3dda9994974485ffc77f07fb7496ac9ee43f40204a`.
No `build/dev` configure or build command was run.

## Independent capability-critical review

The fresh independent review of v004 returned **no blocking finding**. It
recomputed the 31-entry manifest and all bound hashes from the current tree and
reran the authority, provider, transport, persistence, public-observer,
default-manifest, documentation, capsule, image, replay, and dev-identity
surfaces rather than carrying v003's verdict forward.

Its exact results were:

- the eleven-selector authority/composition matrix passed 11/11 in 211.50
  seconds;
- the focused provider/native-HTTPS/SQLite matrix passed 6/6 in 12.97 seconds;
- the hermetic public-observer selector passed 1/1 in 0.33 seconds;
- the default configured manifest contained 194 tests and zero live entries;
- documentation verification, `git diff --check`, capsule verification,
  read-only image/stage-zero verification, replay-state inspection, and the
  protected dev-executable identity all remained clean.

The reviewer found one high-risk sequencing fact, not a defect in these bytes:
any BPR-0334 edit under `src/` makes the live-lane timestamp guard reject this
v004 producer as stale. Resolve this attended v004 checkpoint before BPR-0334
changes `src/`; otherwise stop and perform a complete successor regeneration,
capability-critical review, and exact operator decision for the new bytes.
Never weaken or bypass the timestamp guard.

## Read-only image evidence

The digest-bound read-only capsule image was produced and verified from these
exact capsule bytes as separate release evidence. `capsule.dmg` is
`sha256:cb45d813452913f142f6128c1ede5010c5c4361ae146612f9e61a997bd0d6769`
at 29,978,636 bytes. `stage-zero` checked that exact image digest, mounted it
read-only under a fresh private temporary parent, verified the inventory and
source binding from inside the mount, detached, and returned
`designated-live-macos-capsule:pass`. The registered CTest lane still launches
the plain-directory capsule; this packet makes no claim that the live lane runs
from the mounted image.

## Decision and approval materialization

First verify the five artifact hashes and sizes above. If the decision is not
an unqualified approval of the exact statement and evidence, stop without
creating an approval file.

If approved, create a fresh private directory and materialize exactly the
subject bytes plus this one final line:

```text
subject_sha256=7daa46aa18dfc0cd804c9060f1b790089e17a1db972d074b38e61a78769dcb39
```

The resulting approval must end in one newline, be a new regular file owned by
the operator with mode `0600`, have exactly one hard link, and have an immediate
parent owned by the operator with mode `0700`. Use an absolute resolved path
with no symlink component. Do not put a credential, nonce, comment body, shell
expansion, or additional text in the file. The application consumes and
unlinks that exact inode before any grant exists; it cannot be reused.

## Static manifest inspection

Only after the approval has been materialized, configure the opt-in manifest
with its absolute path:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
cmake -S . -B build/bpr0256-live-manifest -G Ninja \
  -DBUILD_TESTING=ON \
  -DBPR_BUILD_QT=ON \
  -DBPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST=ON \
  -DBPR_DESIGNATED_LIVE_MUTATION_APPROVAL=/absolute/resolved/private/approval.txt \
  -DBPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release/designated-live/c5023d4f81b02c6663afa813a2a3e1876ddfcce481537feb59ee24f974fa9f79/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor \
  -DBPR_DESIGNATED_LIVE_MUTATION_INVENTORY=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release/designated-live/c5023d4f81b02c6663afa813a2a3e1876ddfcce481537feb59ee24f974fa9f79/capsule.inventory \
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

This procedure is inactive until the operator returns the exact approval below
and the approval artifact is materialized exactly as specified.

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
differs. Also abort on redirect, public-observation failure, approval refusal or
substitution, supervisor-authentication refusal, unexpected network/credential
prompt, a missing exact Qt prompt, an already-consumed approval, more than one
enabled Publish action, target/head drift, or an indeterminate POST result. Do
not materialize a second approval and do not reset the replay namespace under
this packet.

**The minted authority expires 15 minutes after the approval is consumed.**
The 1,800-second CTest timeout is the lane's bound, not the authority's. A
session that consumes the approval and then pauses can lose authority mid-run
with no retry admitted. It fails closed, and the approval is spent either way.
Do not begin the attended procedure without 15 uninterrupted minutes.

Target-native Linux and Windows evidence remains deferred. This packet is for
the bound macOS build only and makes no cross-platform claim.

## Operator response

Return exactly one of these decisions:

> I approve the exact one-use approval subject
> sha256:7daa46aa18dfc0cd804c9060f1b790089e17a1db972d074b38e61a78769dcb39
> for BetterPullReview binary
> sha256:2a1c65bf1249ba25333f949deac674ded204cd9ce0653d7fdbb165fcbb4a2216
> and signed-capsule supervisor
> sha256:6ff0d7c7949dc446e8d4e63d04d2eda762d1d6155aab84fdbb6beaae97f35a6f
> and capsule inventory
> sha256:ca4fbe45e2a6ed6198a90b1b114b4617c1008bc8739c3662c2f4e7f1665a25ee
> and source manifest
> sha256:c5023d4f81b02c6663afa813a2a3e1876ddfcce481537feb59ee24f974fa9f79
> to publish one source-line review comment to MikeMarcin/BPR-Test#1 at head
> 17990ba0c2fb50338141f35c0717f0f99639c7ba.

or:

> I reject this exact one-use live review publication.

After an approved run, return the command result and the full non-secret UI
observations requested in step 7. If blocked, return the exact obstruction.
Packet preparation or a partial response is not an approval or verdict.

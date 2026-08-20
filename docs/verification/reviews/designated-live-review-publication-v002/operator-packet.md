# Designated one-shot live review publication operator packet

Status: **INVALIDATED BY CAPABILITY-CRITICAL REVIEW — DO NOT APPROVE OR RUN**

The review this packet was pending has run, and its finding is that the packet's
bound source surface does not describe this repository. Six of the 31 entries in
`source-manifest.txt` do not match the tree, and three of them did not match at
the commit that introduced this packet — it was stale the day it was written,
because the capsule was built from a side-line commit that was never merged as
written. An approval minted against subject `sha256:929a013d…` would authorize a
binary whose `main.cpp`, application composition, and two mutation-path files are
not what any reviewer of this tree reads.

Nothing in the mechanism is wrong. The three counterexamples that invalidated
v001 are repaired in live source and were re-verified against the code rather
than against the cards that claim them. This is regeneration work, and
[[BPR-0406]] carries it.

This v002 packet regenerates the invalidated v001 packet after its blocking
counterexamples were answered: replay authority by BPR-0261, supervisor
launch provenance by BPR-0351, supervisor/inventory identity binding by
BPR-0352, the nested check/use window by BPR-0354, and indeterminate
recovery by BPR-0263. It binds a fresh tests-disabled Release capsule built
at commit `b04b43c` after BPR-0385 unblocked the graph and BPR-0388/BPR-0389
repaired the capsule builder against the renamed executable and universal
Qt. No operator decision is requested until the fresh capability-critical
independent review completes with no blocker.

This packet requests one bounded decision: whether to authorize this exact
tests-disabled BetterPullReview build to publish one ordinary source-line
review comment to the already designated disposable pull request
`MikeMarcin/BPR-Test#1` at head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`.

Approval authorizes at most the first async-host request whose application
session, workbench generation, designated target, freshly authenticated
actor, comparison, file, right-side semantic anchor, resolved path, and
current head all match. It does not authorize another repository, pull
request, head, executable, source build, comment, retry after an
indeterminate POST, or any non-review mutation. It does not close BPR-0142,
BPR-0235, or RFC-BPR-0008.

## Why a person is required

The application can prove exact build/target identity and enforce one-use
admission, but it cannot decide that a public comment should be created or
judge the resulting GitHub UI. A person must approve the exact subject,
enter any credential through the Qt sensitive prompt, select the disposable
source line, publish once, and inspect that first comment in GitHub's web
UI. Posting a reply is outside this one-comment authority.

## Bound evidence

Run only these exact immutable artifacts:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/bpr0256-release/designated-live/1a6c971bd786de1689f06f8c1c8a3399a216bf2bff4be6988294863440266d06/capsule/BetterPullReview.app/Contents/MacOS/BetterPullReview` | `sha256:26719adf08838229e47070c2f8ad30d578a23c222e468b9805ffe89749702c84` | 7661744 |
| `build/bpr0256-release/designated-live/1a6c971bd786de1689f06f8c1c8a3399a216bf2bff4be6988294863440266d06/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor` | `sha256:d54900073bb322a5c25af8ad3b1c9491f68814d3f52e7014dc9b29d280e23205` | 197200 |
| `build/bpr0256-release/designated-live/1a6c971bd786de1689f06f8c1c8a3399a216bf2bff4be6988294863440266d06/capsule.inventory` | `sha256:053f5e99ebf427b4bc8e3eb2838ad982461168588d5cef2a46a99c1c68adc78f` | 11112 |
| `approval-subject.txt` | `sha256:929a013d44ee7ad516f0d25167f23de6a8d6f07c9dca8c52a2138b2632d4c235` | 1496 |
| `source-manifest.txt` | `sha256:1a6c971bd786de1689f06f8c1c8a3399a216bf2bff4be6988294863440266d06` | 3615 |

The source-manifest digest is the exact `BPR_ATTENDED_REVIEW_SOURCE_SHA256`
compiled into the executable, recomputed over the 31-file attended source
surface and reproduced byte-exactly during packet preparation. The producer
cache records `BUILD_TESTING=OFF`, `BPR_BUILD_QT=ON`, `Release`, and source
root `/Users/jaeger/Documents/GitHub/BetterPullReview`. Rebuilding the
capsule, changing any source-manifest entry, or changing the supervisor or
inventory invalidates this packet. Before launching the GUI, the supervisor
verifies the complete signed capsule, performs the unauthenticated public
target observation, creates the app suspended, verifies the child's dynamic
signing identity through the pre-`QApplication` Mach handshake, revalidates
the retained capsule identities after static verification, and only then
resumes it.

The approval subject is schema version 3. Beyond v001 it binds the
supervisor and inventory digests (BPR-0352) and the replay namespace
(BPR-0261):

- `replay_namespace_path=/Users/jaeger/Library/Application Support/BetterPullReview/BetterPullReview/designated-live-review-replay-state-v1`
- `replay_namespace_id=473412d4ffb9031cb291694cf7bd89b504a020f26e2a5b36c7e64fa90225b87c`
- `approval_id=aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`

That namespace already exists and was loaded read-only during preparation.
**Do not run `--initialize-or-reset-designated-live-replay-state`**: reset
mints a fresh namespace identity, which invalidates this subject and the
whole packet by construction.

The exact approval statement embedded in `approval-subject.txt` is:

> I authorize this exact BetterPullReview build to publish one source-line
> review comment to MikeMarcin/BPR-Test#1 at head
> 17990ba0c2fb50338141f35c0717f0f99639c7ba.

The subject binds repository database ID `1328277631`, repository node ID
`R_kgDOTyvofw`, pull-request database ID `4246446933`, pull-request node ID
`PR_kwDOTyvof879G6NV`, base-repository ID `1328277631`, open state, base
`main` at `e2a44aa8fef704de7373463222b1872e796918c6`, and head branch
`codex/disposable-live-review-20260810` at the head above.

## Read-only image evidence

The digest-bound read-only capsule image (BPR-0353) was produced and
verified from these exact capsule bytes as separate release evidence:
`capsule.dmg` is `sha256:4603539d821fd554ec9dc409412dee93941f64bae8e173c145822a1275024fba`
at 29,967,975 bytes, and `stage-zero` mounted it read-only, verified the
inventory from inside the mount, and detached. The registered CTest lane
still launches the plain-directory capsule; this packet makes no claim that
the live lane runs from the mounted image.

## Decision and approval materialization

First verify the five artifact hashes and sizes above. If the decision is
not an unqualified approval of that exact statement and evidence, stop
without creating an approval file.

If approved, create a fresh private directory and materialize exactly the
subject bytes plus this one final line:

```text
subject_sha256=929a013d44ee7ad516f0d25167f23de6a8d6f07c9dca8c52a2138b2632d4c235
```

The resulting approval must end in one newline, be a new regular file owned
by the operator with mode `0600`, have exactly one hard link, and have an
immediate parent owned by the operator with mode `0700`. Use an absolute
resolved path with no symlink component. Do not put a credential, nonce,
comment body, shell expansion, or additional text in the file. The
application consumes and unlinks that exact inode before any grant exists;
it cannot be reused.

## Static manifest inspection

Before executing anything, configure the opt-in manifest with the
materialized approval path:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
cmake -S . -B build/bpr0256-live-manifest -G Ninja \
  -DBUILD_TESTING=ON \
  -DBPR_BUILD_QT=ON \
  -DBPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST=ON \
  -DBPR_DESIGNATED_LIVE_MUTATION_APPROVAL=/absolute/resolved/private/approval.txt \
  -DBPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release/designated-live/1a6c971bd786de1689f06f8c1c8a3399a216bf2bff4be6988294863440266d06/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor \
  -DBPR_DESIGNATED_LIVE_MUTATION_INVENTORY=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release/designated-live/1a6c971bd786de1689f06f8c1c8a3399a216bf2bff4be6988294863440266d06/capsule.inventory \
  -DBPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release \
  -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache
ctest --test-dir build/bpr0256-live-manifest --show-only=json-v1
```

Confirm the manifest contains exactly one test named
`live_review_mutation_designated_target`, its only label is
`bpr_live_mutation`, its timeout is 1800 seconds, and its command is the
exact bound supervisor followed only by `--attended-launch`, `--inventory`,
the bound inventory, `--approval`, and the approval path. Abort if the
manifest names the GUI executable directly, the regression runner, a fixture
or tests-enabled app, another live test or label, a verdict override, or any
credential environment.

This inspection proves the CTest wiring only. The pre-`QApplication` Mach
handshake (BPR-0351) and the post-verification capsule-identity
revalidation (BPR-0354) are enforced inside the supervisor and application
and are not visible to `--show-only`.

## Attended procedure

1. Close other BetterPullReview processes. Confirm the designated PR is
   still disposable and intended for this check.
2. Start only the fixed lane:

   ```sh
   ctest --test-dir build/bpr0256-live-manifest \
     --output-on-failure \
     -R '^live_review_mutation_designated_target$'
   ```

3. If the application refuses startup, stop. Do not recreate the consumed
   approval or retry under this packet. A supervisor-authentication refusal
   deliberately prints the same `STARTUP_FAILED` text as a malformed
   approval; either way the answer is stop, not retry.
4. In the application, choose the GitHub.com profile for the authorized
   actor. Enter any write-scoped credential only into the existing Qt
   sensitive credential prompt. Never place it in Terminal, CTest
   configuration, argv, environment, the approval, a log, or this packet.
5. Open only `MikeMarcin/BPR-Test#1`. Confirm its head is the exact bound
   head. Select one right-side source line in the disposable text fixture
   and enter exactly `BetterPullReview attended round-trip verification.` as
   the comment body.
6. Press Publish once. Do not click Publish again, retry an indeterminate
   result, edit, delete, resolve, or publish any second comment.
7. Open `https://github.com/MikeMarcin/BPR-Test/pull/1` in the GitHub web
   UI. Record the full comment text, author shown, source location, and
   whether it has any BetterPullReview marker, badge, or footer. Do not
   reply, edit, resolve, or delete under this packet. BPR-0142's
   reply-return evidence remains pending for a separately authorized
   attended checkpoint.
8. Close BetterPullReview normally and retain the complete non-secret result
   record. Redact no structural failure, but never return a credential or
   secret-bearing log.

## Abort conditions

Abort without publishing if any artifact hash/size, manifest property,
target identity, PR state/head, source line, comment body, or credential
surface differs. Also abort on redirect, public-observation failure,
approval refusal or substitution, supervisor-authentication refusal,
unexpected network/credential prompt, a missing exact Qt prompt, an
already-consumed approval, more than one enabled Publish action,
target/head drift, or an indeterminate POST result. Do not materialize a
second approval and do not reset the replay namespace under this packet.

Target-native Linux and Windows evidence is explicitly deferred. This
packet is prepared for the bound macOS build only and makes no
cross-platform claim.

## Operator response

Before any run, return one of these exact decisions:

> I approve the exact one-use approval subject
> sha256:929a013d44ee7ad516f0d25167f23de6a8d6f07c9dca8c52a2138b2632d4c235
> for BetterPullReview binary
> sha256:26719adf08838229e47070c2f8ad30d578a23c222e468b9805ffe89749702c84
> and signed-capsule supervisor
> sha256:d54900073bb322a5c25af8ad3b1c9491f68814d3f52e7014dc9b29d280e23205
> and capsule inventory
> sha256:053f5e99ebf427b4bc8e3eb2838ad982461168588d5cef2a46a99c1c68adc78f
> and source manifest
> sha256:1a6c971bd786de1689f06f8c1c8a3399a216bf2bff4be6988294863440266d06
> to publish one source-line review comment to MikeMarcin/BPR-Test#1 at head
> 17990ba0c2fb50338141f35c0717f0f99639c7ba.

or:

> I reject this exact one-use live review publication.

After an approved run, return the command result and the full non-secret UI
observations requested in step 7. If blocked, return the exact obstruction.
Neither packet preparation nor a partial response is an approval or verdict.

# Designated one-shot live review publication operator packet

Status: **pending operator decision; not BPR-0142, BPR-0235, or RFC-BPR-0008 approval**

This packet requests one bounded decision: whether to authorize this exact
tests-disabled BetterPullReview build to publish one ordinary source-line review
comment to the already designated disposable pull request
`MikeMarcin/BPR-Test#1` at head
`17990ba0c2fb50338141f35c0717f0f99639c7ba`.

Approval authorizes at most the first async-host request whose application
session, workbench generation, designated target, freshly authenticated actor,
comparison, file, right-side semantic anchor, resolved path, and current head
all match. It does not authorize another repository, pull request, head,
executable, source build, comment, retry after an indeterminate POST, or any
non-review mutation. It does not close BPR-0142, BPR-0235, or RFC-BPR-0008.

## Why a person is required

The application can prove exact build/target identity and enforce one-use
admission, but it cannot decide that a public comment should be created or
judge the resulting GitHub UI. A person must approve the exact subject, enter
any credential through the Qt sensitive prompt, select the disposable source
line, publish once, and inspect that first comment in GitHub's web UI. Posting
a reply is outside this one-comment authority.

## Bound evidence

Run only these exact immutable artifacts:

| Artifact | SHA-256 | Bytes |
|---|---|---:|
| `build/bpr0256-release/designated-live/01b2c4bdd232acf86d5f6ef2cdb1f0b61fbc9b37a4069ad29c5c255dc15f11cd/capsule/BetterPullReview.app/Contents/MacOS/better_pull_review` | `sha256:a5027a312188fab6668d71469eb256946d1fc4608676133a5f2cdca7bbef373c` | 7724448 |
| `build/bpr0256-release/designated-live/01b2c4bdd232acf86d5f6ef2cdb1f0b61fbc9b37a4069ad29c5c255dc15f11cd/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor` | `sha256:e346b799a2f255dc7179075dd327800336a89084faa17d44a78c7eaff7b7cfbf` | 154720 |
| `build/bpr0256-release/designated-live/01b2c4bdd232acf86d5f6ef2cdb1f0b61fbc9b37a4069ad29c5c255dc15f11cd/capsule.inventory` | `sha256:f3ff8c296ca94e7025bb5ebd8d7693d514b491637e71ec4951574a12d78f93d9` | 13111 |
| `approval-subject.txt` | `sha256:3703270dfb05b37df79f208b1875d510793aa16d9a1154ed192b81c6dbf873bc` | 1058 |
| `source-manifest.txt` | `sha256:01b2c4bdd232acf86d5f6ef2cdb1f0b61fbc9b37a4069ad29c5c255dc15f11cd` | 3147 |

The source-manifest digest is the exact
`BPR_ATTENDED_REVIEW_SOURCE_SHA256` compiled into the executable. Its producer
cache records `BUILD_TESTING=OFF`, `BPR_BUILD_QT=ON`, `Release`, and source root
`/Users/jaeger/Documents/GitHub/BetterPullReview`. Rebuilding the capsule,
changing any source-manifest entry, or changing the supervisor or inventory
invalidates this packet. Before launching the GUI, the supervisor verifies the
complete signed capsule, performs the unauthenticated public target observation,
creates the app suspended, verifies the child's dynamic signing identity, and
only then resumes it.

The exact approval statement embedded in `approval-subject.txt` is:

> I authorize this exact BetterPullReview build to publish one source-line
> review comment to MikeMarcin/BPR-Test#1 at head
> 17990ba0c2fb50338141f35c0717f0f99639c7ba.

The subject binds repository database ID `1328277631`, repository node ID
`R_kgDOTyvofw`, pull-request database ID `4246446933`, pull-request node ID
`PR_kwDOTyvof879G6NV`, base-repository ID `1328277631`, open state, base
`main` at `e2a44aa8fef704de7373463222b1872e796918c6`, and head branch
`codex/disposable-live-review-20260810` at the head above.

## Decision and approval materialization

First verify the five artifact hashes and sizes above. If the decision is not
an unqualified approval of that exact statement and evidence, stop without
creating an approval file.

If approved, create a fresh private directory and materialize exactly the
subject bytes plus this one final line:

```text
subject_sha256=3703270dfb05b37df79f208b1875d510793aa16d9a1154ed192b81c6dbf873bc
```

The resulting approval must end in one newline, be a new regular file owned by
the operator with mode `0600`, have exactly one hard link, and have an immediate
parent owned by the operator with mode `0700`. Use an absolute resolved path
with no symlink component. Do not put a credential, nonce, comment body, shell
expansion, or additional text in the file. The application consumes and
unlinks that exact inode before any grant exists; it cannot be reused.

## Static manifest inspection

Before executing anything, configure the opt-in manifest with the materialized
approval path:

```sh
cd /Users/jaeger/Documents/GitHub/BetterPullReview
cmake -S . -B build/bpr0256-live-manifest -G Ninja \
  -DBUILD_TESTING=ON \
  -DBPR_BUILD_QT=ON \
  -DBPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST=ON \
  -DBPR_DESIGNATED_LIVE_MUTATION_APPROVAL=/absolute/resolved/private/approval.txt \
  -DBPR_DESIGNATED_LIVE_MUTATION_SUPERVISOR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release/designated-live/01b2c4bdd232acf86d5f6ef2cdb1f0b61fbc9b37a4069ad29c5c255dc15f11cd/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor \
  -DBPR_DESIGNATED_LIVE_MUTATION_INVENTORY=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release/designated-live/01b2c4bdd232acf86d5f6ef2cdb1f0b61fbc9b37a4069ad29c5c255dc15f11cd/capsule.inventory \
  -DBPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-release \
  -DCPM_SOURCE_CACHE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache
ctest --test-dir build/bpr0256-live-manifest --show-only=json-v1
```

Confirm the manifest contains exactly one test named
`live_review_mutation_designated_target`, its only label is
`bpr_live_mutation`, its timeout is 1800 seconds, and its command is the exact
bound supervisor followed only by `--attended-launch`, `--inventory`, the bound
inventory, `--approval`, and the approval path. Abort if the manifest names the
GUI executable directly, the regression runner, a fixture or tests-enabled app,
another live test or label, a verdict override, or any credential environment.

## Attended procedure

1. Close other BetterPullReview processes. Confirm the designated PR is still
   disposable and intended for this check.
2. Start only the fixed lane:

   ```sh
   ctest --test-dir build/bpr0256-live-manifest \
     --output-on-failure \
     -R '^live_review_mutation_designated_target$'
   ```

3. If the application refuses startup, stop. Do not recreate the consumed
   approval or retry under this packet.
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
7. Open `https://github.com/MikeMarcin/BPR-Test/pull/1` in the GitHub web UI.
   Record the full comment text, author shown, source location, and whether it
   has any BetterPullReview marker, badge, or footer. Do not reply, edit,
   resolve, or delete under this packet. BPR-0142's reply-return evidence
   remains pending for a separately authorized attended checkpoint.
8. Close BetterPullReview normally and retain the complete non-secret result
   record. Redact no structural failure, but never return a credential or
   secret-bearing log.

## Abort conditions

Abort without publishing if any artifact hash/size, manifest property, target
identity, PR state/head, source line, comment body, or credential surface
differs. Also abort on redirect, public-observation failure, approval refusal or
substitution, unexpected network/credential prompt, a missing exact Qt prompt,
an already-consumed approval, more than one enabled Publish action, target/head
drift, or an indeterminate POST result. Do not materialize a second approval
under this packet.

Target-native Linux and Windows evidence is explicitly deferred. This packet
is prepared for the bound macOS build only and makes no cross-platform claim.

## Operator response

Before any run, return one of these exact decisions:

> I approve the exact one-use approval subject
> sha256:3703270dfb05b37df79f208b1875d510793aa16d9a1154ed192b81c6dbf873bc
> for BetterPullReview binary
> sha256:a5027a312188fab6668d71469eb256946d1fc4608676133a5f2cdca7bbef373c
> and signed-capsule supervisor
> sha256:e346b799a2f255dc7179075dd327800336a89084faa17d44a78c7eaff7b7cfbf
> and capsule inventory
> sha256:f3ff8c296ca94e7025bb5ebd8d7693d514b491637e71ec4951574a12d78f93d9
> and source manifest
> sha256:01b2c4bdd232acf86d5f6ef2cdb1f0b61fbc9b37a4069ad29c5c255dc15f11cd
> to publish one source-line review comment to MikeMarcin/BPR-Test#1 at head
> 17990ba0c2fb50338141f35c0717f0f99639c7ba.

or:

> I reject this exact one-use live review publication.

After an approved run, return the command result and the full non-secret UI
observations requested in step 7. If blocked, return the exact obstruction.
Neither packet preparation nor a partial response is an approval or verdict.

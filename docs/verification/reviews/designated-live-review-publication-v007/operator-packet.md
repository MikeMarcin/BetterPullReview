# Designated one-shot live review publication operator packet

Status: **PENDING EXACT OPERATOR DECISION**; inactive until BPR-0256 records
clean critical review of these exact bytes. This is not approval of another
build, image, retry, repository, comment or mutation.

This v007 packet binds the device-login application built from committed source
`2a043335f649375d56f64590df8d0af8d181a4c0`. The owner approved and consumed v006 on
2026-08-24, then cancelled its manual-token prompt without entering a credential
or publishing. That approval is durably spent and cannot be retried. The
[answered v006 packet](../designated-live-review-publication-v006/operator-packet.md)
and [response](../designated-live-review-publication-v006/operator-response.md)
remain immutable history. v007 uses a fresh approval ID in the same namespace.

## Decision requested

Authorize one attended macOS run of the exact stored image below: sign in using
the production GitHub browser/device flow, publish one ordinary source-line
review comment to the designated disposable `MikeMarcin/BPR-Test#1`, inspect
that comment in GitHub's web UI, then explicitly Sign out and close the app.
The operator supplies browser authorization and confirms the acting account.

The comment body is exactly:

> BetterPullReview attended round-trip verification.

The authority admits at most one exact async-host publication attempt. It does
not authorize another comment, reply, review submission, edit, deletion,
resolution, retry after an indeterminate POST, or another target/head. This
packet does not close BPR-0142, BPR-0235, BPR-0256 or RFC-BPR-0008; their actual
execution and observation evidence remains owed.

## Exact target

- Host `github.com`, server contract `github.com-2022-11-28`.
- Repository `MikeMarcin/BPR-Test`, database ID `1328277631`, node `R_kgDOTyvofw`.
- Pull request `1`, database ID `4246446933`, node `PR_kwDOTyvof879G6NV`.
- Base repository `1328277631`, open state, base `main` at
  `e2a44aa8fef704de7373463222b1872e796918c6`.
- Head branch `codex/disposable-live-review-20260810` at
  `17990ba0c2fb50338141f35c0717f0f99639c7ba`.

The canonical subject below carries the authoritative exact tuple. The prior
public observation is provenance only. The supervisor must freshly observe and
match every compiled target field before GUI creation, and the application
must separately validate its bootstrap and authenticated acting identity.
Any drift refuses; it does not permit editing this packet in place.

## Bound evidence

| Artifact | Exact path | SHA-256 | Bytes |
|---|---|---|---:|
| Capsule application | `build/bpr0256-v007-release/designated-live/f2325e97fe25d6ca8e9d87568f522534027451dba8ab4a1912ae3baa26bfba45/capsule/BetterPullReview.app/Contents/MacOS/BetterPullReview` | `sha256:e8d2b7006f6da8aba8b006c4e933624cc8286a68136fba1f7d6f17e1efea8fcb` | 8042624 |
| Signed supervisor | `build/bpr0256-v007-release/designated-live/f2325e97fe25d6ca8e9d87568f522534027451dba8ab4a1912ae3baa26bfba45/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor` | `sha256:7bd25da312afba16c72dfe64c9432609ac164f4aa414422a753cd92ac7bb7847` | 239312 |
| Capsule inventory | `build/bpr0256-v007-release/designated-live/f2325e97fe25d6ca8e9d87568f522534027451dba8ab4a1912ae3baa26bfba45/capsule.inventory` | `sha256:8cc606342009652d9813d31d1860e65ec336c7dee352120f3c6fb00c9d78f7f2` | 11112 |
| Exact stored DMG | `build/bpr0256-v007-release/designated-live/f2325e97fe25d6ca8e9d87568f522534027451dba8ab4a1912ae3baa26bfba45/capsule.dmg` | `sha256:e53a6dbbe44318f059e7132ed71ac3fd7f76e896840fcb35f26af4c1ec1f3e76` | 29746780 |
| DMG digest file | `build/bpr0256-v007-release/designated-live/f2325e97fe25d6ca8e9d87568f522534027451dba8ab4a1912ae3baa26bfba45/capsule.dmg.sha256` | `sha256:b997b45eca46657cce26780f0001f6a57793c4af19214549dbb1f3589d9202ca` | 65 |
| Stage-zero launcher | `scripts/designated_live_macos_capsule.py` | `sha256:b969b90a26b87d862e04c5c2064636ffd875ef3361ff392cfa5f4f8df998da05` | 60716 |
| Canonical approval subject | `docs/verification/reviews/designated-live-review-publication-v007/approval-subject.txt` | `sha256:015ad8a7636483f4258ab7f8ac22af841ba48bdeb924f324b77abf01631e6921` | 1496 |
| 37-entry source manifest | `docs/verification/reviews/designated-live-review-publication-v007/source-manifest.txt` | `sha256:f2325e97fe25d6ca8e9d87568f522534027451dba8ab4a1912ae3baa26bfba45` | 4301 |

The producer is `/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v007-release`. Its cache selects Release,
`BUILD_TESTING=OFF`, Qt enabled, the designated macOS capsule enabled, and
offline dependencies. Both binaries bind source SHA
`f2325e97fe25d6ca8e9d87568f522534027451dba8ab4a1912ae3baa26bfba45`. The checked-in source manifest is byte-identical
to the producer's 37-entry manifest and every entry matches committed source.
The signed inventory binds the shipped executable and resources. No
bit-for-bit reproducible-build claim is made: this decision names one exact
stored application, supervisor, inventory and image.

Changing a bound source, artifact, launcher, subject or packet byte reopens the
decision. The generated CTest plan is rechecked before launch; changing it
invalidates the inspected command. The image digest is passed as a literal,
not read at launch from a mutable digest file.

## Runtime topology and accepted limits

The sole opt-in command uses `/usr/bin/python3 -I -S -c` with the captured-byte
preloader, then the exact launcher path and literal launcher digest. It opens
the launcher without following links, captures bounded stable bytes, verifies
the digest and executes that captured program. Stage zero retains and verifies
the exact image descriptor, mounts the image read-only, and runs the verified
supervisor and application from that mount. A retained root/inventory identity
spans verification, hashing and launch. The private mount parent is:

`/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v007-live-mount-parent`

Stage one owns its process group with a 1,500-second bound. Normal exit,
failure, signals and timeout go through post-child revalidation, bounded
TERM/KILL, reap and ordinary detach. CTest has a 1,800-second timeout.

The operator accepts these four limits for this one disposable course:

1. The mount is read-only, but its owner can still write the backing DMG.
   Revalidation detects persistent drift, not every transient modification.
2. `hdiutil attach` takes a pathname, not the retained image descriptor; attach
   is not descriptor-atomic.
3. The same user's generated CTest plan and cache are trusted. Repository-local
   checks cannot prevent that user from rewriting the execution plan.
4. Synchronous `hdiutil` attach/detach have no independent subprocess timeout.
   The post-attach stage-one bound does not prove detach if DiskImages stalls.

## Replay and approval identity

- Replay schema `bpr.designated_live_review_replay.v1`, version 1.
- Namespace `/Users/jaeger/Library/Application Support/BetterPullReview/BetterPullReview/designated-live-review-replay-state-v1`.
- Namespace ID `473412d4ffb9031cb291694cf7bd89b504a020f26e2a5b36c7e64fa90225b87c`.
- New approval ID `ab4a37dd919851edb63992fb5ebc311d5def51349733e4d05b77be9c6166f0d3`.
- Canonical subject SHA-256 `015ad8a7636483f4258ab7f8ac22af841ba48bdeb924f324b77abf01631e6921`.

The production read-only loader validated the existing namespace. Its directory
is mode `0700`; it contains the identity record and the spent v006 approval ID
`aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`.
The new approval ID is unspent. Never initialize or reset this namespace under
this packet. The new subject was rendered by the canonical production
`designated_review_approval_subject` formatter; no approval was materialized.

Schema 3 binds application, source, supervisor, inventory, replay identity,
exact target and the one-comment effect. The response below also binds the
outer launcher, image, mount parent and four limits. Credentials are separate
from approval: no token, refresh material, device/user code or credential goes
into the approval, repository, CTest configuration, argv, environment or logs.

## Unattended verification

The exact final capsule was built from committed source and its production-
shaped preflight ran from the read-only mounted image, then detached normally.
This probe performs no attended launch, credential operation, public observation
or mutation. The initial supervisor build needed a capture-name repair for
current Apple Clang and explicit unsigned mask operands for strict tidy; final
source, supervisor build and full-TU tidy pass. These preserve launch behavior.

The focused safe checks passed:

- Six authority/capsule/default-isolation/public-observer selectors: 6/6 in
  74.40 seconds, using fixtures and no live service.
- Final supervisor identity and production composition contracts: 2/2 in
  41.89 seconds after the compiler/tidy repair and source-inventory repin.
- Complete changed supervisor TU strict tidy, formatting, direct architecture
  and core boundaries, documentation structure and diff hygiene.
- Existing documentation validator self-tests: 54/54, unchanged inputs.
- Default manifest: 277 tests and zero live entries or labels.
- Static-only opt-in manifest: 278 tests, exactly one
  `live_review_mutation_designated_target`, one `bpr_live_mutation` label,
  timeout 1,800 seconds, 24 Python/loader/GitHub environment removals, and no
  pass/skip/disable/exit-verdict override. The official configured-isolation
  validator passed with backend `macos-sandbox-exec`.

Static inspection used a mode-`0600` deliberately invalid placeholder under a
mode-`0700` private directory. Both were removed after inspection. The live test
was not run. A fresh exact critical review of these artifacts and packet must
be recorded on BPR-0256 before the decision below becomes active. That review
record lives on the card so these reviewed packet bytes need not change.

## After an exact approval

First revalidate every bound artifact and source entry, confirm the new approval
ID remains unspent, and confirm the mount parent is empty and owner-private.
Materialize a fresh regular one-link mode-`0600` file in a new owner-private
mode-`0700` resolved directory: exactly `approval-subject.txt` bytes followed by
this final line and one newline:

```text
subject_sha256=015ad8a7636483f4258ab7f8ac22af841ba48bdeb924f324b77abf01631e6921
```

Reconfigure the static graph with that resolved approval path and these fixed
values, then inspect `ctest --show-only=json-v1` again without executing it:

```text
BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST=ON
BPR_DESIGNATED_LIVE_MUTATION_APPROVAL=<new resolved private approval path>
BPR_DESIGNATED_LIVE_MUTATION_IMAGE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v007-release/designated-live/f2325e97fe25d6ca8e9d87568f522534027451dba8ab4a1912ae3baa26bfba45/capsule.dmg
BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256=e53a6dbbe44318f059e7132ed71ac3fd7f76e896840fcb35f26af4c1ec1f3e76
BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v007-live-mount-parent
BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v007-release
```

The command must be the captured preloader and launcher, `stage-zero`, exact
image, literal image SHA, exact mount/source and only
`--attended-launch --approval <exact private path>` after those bindings.
Any different executable, extra live lane, credential environment or verdict
override aborts. No approval exists until the exact owner response is validated.

## Attended procedure

Begin only after clean exact critical review, the approval above and final
static inspection. Reserve fifteen uninterrupted minutes: the execution
authority expires fifteen minutes after approval consumption; the outer timeout
does not extend it. Close other BetterPullReview instances before starting.

1. Start only the fixed registered lane, once:

   ```sh
   ctest --test-dir build/bpr0256-v007-live-manifest --output-on-failure -R '^live_review_mutation_designated_target$'
   ```

2. If launch, target observation, identity validation, mounting or bootstrap
   refuses, stop. Do not recreate approval or retry this packet.
3. Select GitHub.com and use the device sign-in panel's **Get a code** and
   browser controls. The operator authorizes the displayed code directly in
   the system browser. Do not use the rejected manual-token Connect prompt.
   Do not copy codes or credentials to chat or capture them in evidence.
4. Confirm the authenticated account shown in BetterPullReview is the account
   the operator intended and has permission to comment. Record that non-secret
   login for comparison with the GitHub author. An unexpected account aborts.
5. Open only the exact designated PR above. Confirm open state, base and head.
   Select one right-side source line in the disposable text fixture, record its
   displayed path/line, and enter exactly the body stated at the top.
6. Press Publish once. An indeterminate result ends this attempt without retry.
7. Open [the PR in GitHub](https://github.com/MikeMarcin/BPR-Test/pull/1).
   Record the full comment text, author, source path/line, comment URL and
   whether any BetterPullReview marker, badge or footer appears. Inspect only;
   reply-return evidence belongs to a separately authorized checkpoint.
8. Use the visible **Sign out** control in BetterPullReview, wait for cleanup
   and signed-out state, then close normally. The approved device policy retains
   accepted grants on ordinary quit; quitting alone is not this course's
   credential cleanup. If Sign out reports failure, record that obstruction
   without claiming the slots were erased.
9. Record the CTest result, normal application exit and detach outcome with the
   non-secret UI observations. Do not return secret-bearing logs.

Abort on any artifact/manifest mismatch, unexpected prompt or account,
redirect/observation refusal, target/head/body/line drift, duplicate Publish
opportunity, consumed approval, image/runtime drift, indeterminate POST, or
failed/forced detach. Never change the image, plan, namespace or approval to
turn refusal into a retry. Native Linux/Windows execution remains outside this
macOS packet.

## Operator response

After BPR-0256 records clean final review of these exact bytes, return the
following approval verbatim, or the rejection below:

> I approve the exact one-use approval subject
> sha256:015ad8a7636483f4258ab7f8ac22af841ba48bdeb924f324b77abf01631e6921
> for BetterPullReview binary
> sha256:e8d2b7006f6da8aba8b006c4e933624cc8286a68136fba1f7d6f17e1efea8fcb
> and signed-capsule supervisor
> sha256:7bd25da312afba16c72dfe64c9432609ac164f4aa414422a753cd92ac7bb7847
> and capsule inventory
> sha256:8cc606342009652d9813d31d1860e65ec336c7dee352120f3c6fb00c9d78f7f2
> and source manifest
> sha256:f2325e97fe25d6ca8e9d87568f522534027451dba8ab4a1912ae3baa26bfba45
> and stage-zero launcher
> sha256:b969b90a26b87d862e04c5c2064636ffd875ef3361ff392cfa5f4f8df998da05
> and exact stored read-only image
> sha256:e53a6dbbe44318f059e7132ed71ac3fd7f76e896840fcb35f26af4c1ec1f3e76
> mounted below
> /Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v007-live-mount-parent
> under the backing-DMG, pathname-attach, no-independent-hdiutil-timeout, and
> trusted-CTest-plan limits stated in this packet, using browser/device sign-in
> and explicit Sign out cleanup, to publish one source-line review comment to
> MikeMarcin/BPR-Test#1 at head
> 17990ba0c2fb50338141f35c0717f0f99639c7ba.

Or:

> I reject this exact one-use live review publication.

A partial response, the prior v006 approval, or the separate device-retention
policy verdict does not approve this publication. After execution, return the
actual command result and requested non-secret GitHub/UI observations.

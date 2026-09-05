# Designated one-shot live review publication operator packet

Status: **PENDING EXACT OPERATOR DECISION**; inactive until BPR-0256 records
clean critical review of these exact bytes. This is not approval of another
build, image, retry, repository, comment or mutation.

This v008 packet binds the repaired browser/device-login application built from
committed source `a63e92e62bb6680ad5d811f77eb0b94c7724e7d6`. BPR-0559 fixed
capability discovery with nine serial bounded introspection queries; BPR-0605
then observed successful live sign-in as MikeMarcin in an ordinary Release
build. The [v005 diagnostic result](../device-session-bootstrap-diagnostic-v005/attended-result.md)
records that success and the separate post-quit inspection relaunch deviation.
This fresh capsule has not been launched for sign-in or publication.

The owner approved and consumed v007, but sign-in refused before any PR or
publication. The [answered v007 packet](../designated-live-review-publication-v007/operator-packet.md)
and [result](../designated-live-review-publication-v007/attended-result.md)
remain immutable history. Both v006 and v007 approvals are spent. v008 uses a
fresh approval ID in the same namespace.

## Decision requested

Authorize one attended macOS run of the exact stored image below: sign in using
the production GitHub browser/device flow, publish one ordinary source-line
review comment to the designated disposable `MikeMarcin/BPR-Test#1`, inspect
that comment in GitHub's web UI, then explicitly Sign out and close the app.
The operator supplies browser authorization as MikeMarcin. The agent reads
this run's sanitized local diagnostic records to detect terminal sign-in
completion automatically and verifies the displayed acting account.

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
| Capsule application | `build/bpr0256-v008-release/designated-live/7256057388474bc59e3cad76611cf6223684c05fd7b006cfea96539e9fd08b94/capsule/BetterPullReview.app/Contents/MacOS/BetterPullReview` | `sha256:3224a79b1f71b9fd9e989c1334c41b4e911fbf6ff45991a99077e782885ebf92` | 8361696 |
| Signed supervisor | `build/bpr0256-v008-release/designated-live/7256057388474bc59e3cad76611cf6223684c05fd7b006cfea96539e9fd08b94/capsule/BetterPullReview.app/Contents/Helpers/better_pull_review_designated_live_supervisor` | `sha256:73691252e2e3e484d3426c20b75f4830416581fa63d9d415148d3a1d7f13a640` | 239312 |
| Capsule inventory | `build/bpr0256-v008-release/designated-live/7256057388474bc59e3cad76611cf6223684c05fd7b006cfea96539e9fd08b94/capsule.inventory` | `sha256:d3245beba40a031b3db34cba983e3136b9e36d8c1e2b2a7d6b69925ce00b72f9` | 11112 |
| Exact stored DMG | `build/bpr0256-v008-release/designated-live/7256057388474bc59e3cad76611cf6223684c05fd7b006cfea96539e9fd08b94/capsule.dmg` | `sha256:43e6014419a56ba557ba068eab9b7347166c2cec43f73e2dfc4ef783d494d21f` | 30349827 |
| DMG digest file | `build/bpr0256-v008-release/designated-live/7256057388474bc59e3cad76611cf6223684c05fd7b006cfea96539e9fd08b94/capsule.dmg.sha256` | `sha256:fbd0d0d6e19e526ade919bfbddcd72914406f8f9530eb5831e90d593a92942d7` | 65 |
| Stage-zero launcher | `scripts/designated_live_macos_capsule.py` | `sha256:b969b90a26b87d862e04c5c2064636ffd875ef3361ff392cfa5f4f8df998da05` | 60716 |
| Canonical approval subject | `docs/verification/reviews/designated-live-review-publication-v008/approval-subject.txt` | `sha256:13325d1d31608ec424c6551238bd74241385d2c4b73b20a31026afad233ec981` | 1496 |
| 37-entry source manifest | `docs/verification/reviews/designated-live-review-publication-v008/source-manifest.txt` | `sha256:7256057388474bc59e3cad76611cf6223684c05fd7b006cfea96539e9fd08b94` | 4301 |

The producer is `/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v008-release`. Its cache selects Release,
`BUILD_TESTING=OFF`, Qt enabled, the designated macOS capsule enabled, and
offline dependencies. Both binaries bind source SHA
`7256057388474bc59e3cad76611cf6223684c05fd7b006cfea96539e9fd08b94`. The selected source entries also match `eff28443`,
whose additional changes only restore skill goal guidance. The checked-in source manifest is byte-identical
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

`/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v008-live-mount-parent`

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
- New approval ID `dc6ce6ab54a0d584524a9f76dc04bfe1638ae4c084262ebc3143f8453fa4f98f`.
- Canonical subject SHA-256 `13325d1d31608ec424c6551238bd74241385d2c4b73b20a31026afad233ec981`.

The production read-only loader validated the existing namespace. Its directory
is mode `0700`; it contains the identity record and two spent approvals:
v006 `aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`
and v007 `ab4a37dd919851edb63992fb5ebc311d5def51349733e4d05b77be9c6166f0d3`.
The new approval ID is unspent. Never initialize or reset this namespace under
this packet. The new subject was rendered by the canonical production
`designated_review_approval_subject` formatter; no approval was materialized.

Schema 3 binds application, source, supervisor, inventory, replay identity,
exact target and the one-comment effect. The response below also binds the
outer launcher, image, mount parent and four limits. Credentials are separate
from approval: no token, refresh material, device/user code or credential goes
into the approval, repository, CTest configuration, argv, environment or logs.

## Unattended verification

The exact final capsule was built from committed source with offline cached
dependencies. Its production-shaped preflight ran from the read-only mounted
image and detached normally. This probe performs no attended launch, credential
operation, public observation or mutation. All 37 selected source entries match
the current and committed tree; both binaries contain the new source identity.

The focused safe checks passed:

- Six authority/capsule/default-isolation/public-observer selectors: 6/6 in
  99.19 seconds, using fixtures and no live service.
- BPR-0559's unchanged-source capability/native-transport/logging tests,
  production composition contracts, scoped tidy/format and architecture checks
  remain reusable; the v008 preparation changes no production source.
- Documentation structure and diff hygiene pass; the existing documentation
  validator self-tests pass. No full matrix or native Windows/Linux run.
- Default manifest: 280 tests and zero live entries or labels.
- Static-only opt-in manifest: 281 tests, exactly one
  `live_review_mutation_designated_target`, one `bpr_live_mutation` label,
  timeout 1,800 seconds, 24 Python/loader/GitHub environment removals, and no
  pass/skip/disable/exit-verdict override. The official configured-isolation
  validator passed with backend `macos-sandbox-exec`.

Static inspection uses a mode-`0600` deliberately invalid placeholder under a
mode-`0700` private directory. It and the disposable static graph will be removed
after the exact review consumes the results; BPR-0256 records that cleanup.
The live test was not run. A fresh exact critical review of these artifacts and packet must
be recorded on BPR-0256 before the decision below becomes active. That review
record lives on the card so these reviewed packet bytes need not change.

## After an exact approval

First revalidate every bound artifact and source entry, confirm the new approval
ID remains unspent, and confirm the mount parent is empty and owner-private.
Materialize a fresh regular one-link mode-`0600` file in a new owner-private
mode-`0700` resolved directory: exactly `approval-subject.txt` bytes followed by
this final line and one newline:

```text
subject_sha256=13325d1d31608ec424c6551238bd74241385d2c4b73b20a31026afad233ec981
```

Reconfigure the static graph with that resolved approval path and these fixed
values, then inspect `ctest --show-only=json-v1` again without executing it:

```text
BPR_ENABLE_DESIGNATED_LIVE_MUTATION_TEST=ON
BPR_DESIGNATED_LIVE_MUTATION_APPROVAL=<new resolved private approval path>
BPR_DESIGNATED_LIVE_MUTATION_IMAGE=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v008-release/designated-live/7256057388474bc59e3cad76611cf6223684c05fd7b006cfea96539e9fd08b94/capsule.dmg
BPR_DESIGNATED_LIVE_MUTATION_IMAGE_SHA256=43e6014419a56ba557ba068eab9b7347166c2cec43f73e2dfc4ef783d494d21f
BPR_DESIGNATED_LIVE_MUTATION_MOUNT_PARENT=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v008-live-mount-parent
BPR_DESIGNATED_LIVE_MUTATION_BUILD_DIR=/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v008-release
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

1. Record the sanitized local log byte baseline, then start only the fixed
   registered lane, once:

   ```sh
   ctest --test-dir build/bpr0256-v008-live-manifest --output-on-failure -R '^live_review_mutation_designated_target$'
   ```

2. If launch, target observation, identity validation, mounting or bootstrap
   refuses, stop. Do not recreate approval or retry this packet.
3. Inspect only this run's appended sanitized records. Never capture device
   codes, tokens or raw responses. Select GitHub.com and use the device sign-in
   panel's **Get a code** and browser controls. The operator authorizes the displayed code directly in
   the system browser. Do not use the rejected manual-token Connect prompt.
   Do not copy codes or credentials to chat or capture them in evidence.
4. Monitor the appended sanitized log for terminal success or failure, without
   requiring the operator to announce completion. Confirm the authenticated
   account displayed in BetterPullReview is MikeMarcin and can comment. Record
   that non-secret login for comparison with the GitHub author. An unexpected
   account aborts.
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
9. After closing, verify application exit and detach through the existing CTest
   process, sanitized log and process/attachment checks. Do not call an app
   getter or inspect an app handle after quit: that can relaunch it. Record the
   command result and non-secret observations; retain the full owner-local log
   outside Git.

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
> sha256:13325d1d31608ec424c6551238bd74241385d2c4b73b20a31026afad233ec981
> for BetterPullReview binary
> sha256:3224a79b1f71b9fd9e989c1334c41b4e911fbf6ff45991a99077e782885ebf92
> and signed-capsule supervisor
> sha256:73691252e2e3e484d3426c20b75f4830416581fa63d9d415148d3a1d7f13a640
> and capsule inventory
> sha256:d3245beba40a031b3db34cba983e3136b9e36d8c1e2b2a7d6b69925ce00b72f9
> and source manifest
> sha256:7256057388474bc59e3cad76611cf6223684c05fd7b006cfea96539e9fd08b94
> and stage-zero launcher
> sha256:b969b90a26b87d862e04c5c2064636ffd875ef3361ff392cfa5f4f8df998da05
> and exact stored read-only image
> sha256:43e6014419a56ba557ba068eab9b7347166c2cec43f73e2dfc4ef783d494d21f
> mounted below
> /Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0256-v008-live-mount-parent
> under the backing-DMG, pathname-attach, no-independent-hdiutil-timeout, and
> trusted-CTest-plan limits stated in this packet, using browser/device sign-in
> as MikeMarcin, reading this run's sanitized local log, and explicit Sign out
> cleanup, to publish one source-line review comment to
> MikeMarcin/BPR-Test#1 at head
> 17990ba0c2fb50338141f35c0717f0f99639c7ba.

Or:

> I reject this exact one-use live review publication.

A partial response, a consumed v006/v007 or sign-in-only approval, or the
separate device-retention policy verdict does not approve this publication.
After execution, return the actual command result and requested non-secret GitHub/UI observations.

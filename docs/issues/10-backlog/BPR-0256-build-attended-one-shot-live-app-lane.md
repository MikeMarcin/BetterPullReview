---
id: BPR-0256
title: Build the attended one-shot live application lane
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0255, BPR-0261, BPR-0262, BPR-0263, BPR-0318, BPR-0448, BPR-0449, BPR-0559, BPR-0560, BPR-0606, BPR-0607, BPR-0608]
---

# Build the attended one-shot live application lane

BPR-0235 established one default-off designated mutation exception around a
fixed harmless canary. BPR-0255 made the shipped product mutation path
asynchronous, exactly bound, and denied by default because production has no
execution-grant issuer. The remaining portable boundary is an attended
bootstrap that consumes one digest-bound operator approval and can authorize
one exact request in a tests-disabled shipped application.

## Required final state

- The existing canary runner/gate remains a hermetic regression surface. The
  sole opt-in `live_review_mutation_designated_target` command instead starts
  the fixed captured-byte stage-zero loader with one exact literal-digest image
  from a freshly built `BUILD_TESTING=OFF` shipped application. Stage zero
  mounts that image read-only and owns the signed supervisor/application
  process group through child exit and ordinary detach. The command never
  launches a plain build-tree supervisor, the tests-enabled fixture app, or the
  GUI directly.
- The shipped app accepts one production-only attended bootstrap authority. It
  validates an absolute approval artifact using retained native file/directory
  identity, exact owner-private permissions, bounded bytes, exact schema and
  digest, and atomic one-time consumption without reopening a mutable path.
- Before the app can mint an execution authority, it performs an unauthenticated
  redirect-free public observation and matches every compiled designated
  `MikeMarcin/BPR-Test#1` repository/PR/base/head field. Failure, substitution,
  replay, timeout, malformed response, or identity drift leaves no grant.
- Approval and credential authority remain separate. The approval contains no
  token and grants only permission for one exact publication attempt. The
  operator authorizes the production browser/device sign-in after launch;
  credential secrets never enter CTest configuration, argv, environment, the
  approval artifact, IPC, fixtures, diagnostics, or logs. The v008 course
  explicitly Signs out before quit to remove its retained device grant.
- The bootstrap carrier is retained privately until the async mutation host has
  validated the exact application session, workbench generation, designated
  target, authenticated acting identity, comparison, file, right-side semantic
  anchor, resolved path, and current-head commit. Only then may one atomic
  consume succeed. Invalid requests do not consume it; the first exact request
  does; every later request refuses before an effect.
- Application invalidation, target/head/actor drift, cancellation, timeout, and
  shutdown revoke unused authority. A transmitted indeterminate POST remains
  guarded by the existing retry identity and server reconciliation; the
  bootstrap never submits directly or bypasses the async host.
- Default configured manifests retain zero live exceptions. The explicit
  opt-in manifest has exactly one test, exactly one `bpr_live_mutation` label,
  the fixed shipped-app/bootstrap command, no CTest pass/skip/disable override,
  no credential environment, a bounded attended timeout, and fail-closed
  missing-path/configuration behavior.
- A digest-bound operator packet records the exact immutable approval subject,
  app/source/build identity, designated PR/head, intended one-comment scope,
  credential-handling instructions, abort conditions, and required GitHub web
  inspection. This card prepares the packet but does not execute or approve it.

## Non-goals

- Do not execute the live lane, access a credential, submit a comment, or claim
  GitHub/web-UI evidence.
- Do not replace or weaken the BPR-0235 canary/fork safety contracts; they remain
  regression evidence, not application-path evidence.
- Do not pass a nonce/capability over a forgeable child pipe, argv, environment,
  or persisted application setting.
- Do not claim target-native Linux/Windows evidence; those remain deferred until
  the end of portable work by product-owner instruction.
- Do not close BPR-0142, BPR-0235, or RFC-BPR-0008.

## Verification

- Hermetic bootstrap tests cover absent/malformed/unsafe/replayed/substituted
  approval, every tuple mismatch, bounded remote failures, atomic consumption,
  and zero grant on failure without network or credentials.
- Async-host tests prove invalid exact-request bindings do not consume the
  authority, the first valid request consumes it immediately before queueing,
  the second refuses, and cancellation/shutdown revoke it.
- Production syntax/source/symbol cuts prove there is one issuer, no fixture
  issuer, no direct provider/coordinator/network/credential bypass, and no
  ambient authorization path.
- Default and opt-in manifest tests prove zero versus exactly one live exception
  and the exact tests-disabled shipped-app command. Hostile PASS/WILL_FAIL/SKIP,
  alternate executable, fixture app, credential environment, and second-lane
  graphs fail.
- Existing designated canary/gate, async host, live isolation, architecture,
  documentation, scoped format/tidy, and `git diff --check` gates remain green.

## Unattended implementation checkpoint

The signed-capsule prelaunch supervisor, private in-application bootstrap,
narrow GitHub-adapter public observation, private one-use host carrier, exact
request admission, production-only CLI surface, and tests-disabled shipped-app
CTest registration are implemented. Hermetic tests
exercise unsafe/malformed/replayed/substituted approvals, digest and every
target/build mismatch, observer failures, concurrent consume, invalid-before-
valid host admission, exact one-use behavior, expiry, cancellation, and
shutdown without network or credentials. Hostile syntax/source checks reject
external authority construction and redefinition of the sealed issuer.

The prior BPR-0235 runner/gate/canary sources remain unchanged and are
regression-only. Configure-time and independent manifest checks reject that
runner, direct GUI and fixture/alternate apps, alternate supervisors or
inventories, tests-enabled/non-Release producer trees, missing approval, stale
artifacts, credential environment, verdict overrides, or a second live lane.

The digest-bound operator packet is pending attended review. The live test has
not been executed; no credential or network was accessed and no mutation was
submitted. Target-native Linux/Windows evidence remains deferred by product-
owner direction, and the Windows bootstrap remains fail-closed. Accordingly
this card stays in progress and makes no BPR-0142, BPR-0235, or RFC closeout
claim.

## Session handoff — 2026-08-11

The operator ended the implementation session and requested that all current
work be committed and merged without claiming this card complete. The saved
tree contains the signed macOS capsule/supervisor, private approval bootstrap,
one-shot async-host admission, public target observer, durable publication
recovery, static live-lane registration, hostile contracts, documentation, and
the pending operator packet. Future authentication/read investigations are
preserved as draft RFC-BPR-0016 and RFC-BPR-0017; they are not acceptance
claims for this card.

Exact unfinished work remains owned here:

- Re-run `architecture_boundaries` and `architecture_boundary_contract` after
  the saved verifier update from the old `inventory` expression to the current
  dereferenced `*inventory` expression. The interrupted 16-test replay passed
  the other twelve completed BPR-0256 tests, failed those two stale-expression
  gates before the correction, and was stopped while the two composition
  contracts were still pending.
- Rebuild the signed capsule and refresh `source-manifest.txt`,
  `approval-subject.txt`, and `operator-packet.md` after any subsequent source
  edit. The currently recorded packet binds source digest
  `d8ad05b9e356b0df1177c9b276feeea2a80978da20134682ce2ce02fa8ab2791`;
  it must be treated as stale if this commit changes a bound source byte.
- Reconfigure and inspect the static opt-in manifest against the final packet,
  then run scoped format/tidy, documentation validation, `git diff --check`,
  and the complete focused matrix from the final committed tree.
- Run the capability-critical independent closeout review. The required
  read-only reorientation lane could not be created in this session because
  the runtime repeatedly returned `agent thread limit reached`; this is a
  recorded serial-fallback trigger, not a waiver of the review gate.
- Do not execute the live lane until the regenerated operator packet receives
  its exact approval statement. BPR-0142 still owns the attended publication
  and GitHub web inspection; BPR-0235 still owns target-native/default-deny
  closeout. Terminal BPR-0241 through BPR-0245 remain in backlog under their
  existing dependency cards.

Post-session integration merged the BPR-0257 through BPR-0259 compact-review
navigation branch into `main`. That merge changes bound source
`src/app/src/github_application.cpp`, so the checked-in BPR-0256 source
manifest, approval subject, capsule identity, and operator packet are now
**intentionally stale and must not be used**. Regeneration and full replay are
the first required BPR-0256 actions in a future session.

## Unattended refresh — 2026-08-11

The current tests-disabled Release capsule rebuilt and verified on macOS with
source digest
`01b2c4bdd232acf86d5f6ef2cdb1f0b61fbc9b37a4069ad29c5c255dc15f11cd`.
The refreshed application, supervisor, and capsule inventory are respectively
bound as
`sha256:a5027a312188fab6668d71469eb256946d1fc4608676133a5f2cdca7bbef373c`,
`sha256:e346b799a2f255dc7179075dd327800336a89084faa17d44a78c7eaff7b7cfbf`,
and
`sha256:f3ff8c296ca94e7025bb5ebd8d7693d514b491637e71ec4951574a12d78f93d9`.
The exact refreshed approval subject is
`sha256:3703270dfb05b37df79f208b1875d510793aa16d9a1154ed192b81c6dbf873bc`;
the packet itself is
`sha256:10d04228de59c536df5398be22b755131f51cf1ffa467146c2ee807f2ba6ed4e`
at 9,569 bytes. No approval file was materialized.

The eight-test focused matrix passed 8/8 after repairing three stale
composition-integrity digests and changing one vault-specific hostile fixture
from globally reserved `CreateProcessW` to independently forbidden
`ShellExecuteW`. The complete architecture pair also passed, as did the direct
three-test precedence check for vault fallback and exact Windows process
ownership. The code-simplification pass found no safe further change.

The opt-in graph was configured only for static inspection with a private,
deliberately invalid placeholder that was removed immediately afterward. CTest
reported exactly one `live_review_mutation_designated_target`, exactly one
`bpr_live_mutation` label, a 1,800-second timeout, the refreshed supervisor and
inventory, only `--attended-launch`, `--inventory`, and `--approval` arguments,
and explicit credential-environment removal. The test was not executed; no
network request, credential access, browser action, or mutation occurred.

The repository-wide format gate then identified pre-existing formatting drift
in the preserved BPR-0240 Windows process slice and merged compact-navigation
source. That drift is remaining unattended work and blocks this card's final
format/tidy evidence until repaired. The refreshed packet remains pending an
exact operator decision and must be regenerated again if any bound source byte
changes.

## Strict-tidy repair checkpoint — 2026-08-11

After the formatting drift was isolated and repaired under BPR-0240 and
BPR-0260, the complete 227-translation-unit strict-tidy scan exposed five
additional violations in this card's designated-lane sources. The repair keeps
the same semantics while removing unsafe C-string assumptions: the retained
`std::string` now supplies the exact `posix_spawn` executable path, and the
fixed spent-directory name is a null-terminated character array. The one local
mutation helper was renamed to the repository's lower-camel convention.

Strict tidy passes directly for all three affected translation units. Their
adapter/application targets rebuild, and the exact
`application_designated_review_authority_bootstrap`,
`application_review_mutation`, `designated_live_public_observer_fixture`,
`github_mutation_contract`, and `live_review_mutation_provider` selector passes
5/5 in 15.15 seconds. These bound source changes intentionally invalidate the
preceding capsule, source manifest, approval subject, and packet. No approval
file exists and the live test has not run; a fresh final packet is required
after this checkpoint is committed.

## Final unattended packet refresh — 2026-08-12

The final committed bound source produced and verified a tests-disabled Release
capsule at source digest
`d625798086f4f441d22aeb4986ab8058e62a09d8d5ed6d2b4bf7337e025acef2`.
Its application, supervisor, and inventory are respectively
`sha256:7bacbba642fe140bad64cc2ae4d25c089e441d79f9cfb4bf4586e33b2630934a`,
`sha256:3917ed1c5e150b3f7c2e7aeb3886d13c003c997cc56f4b323d9c258e960a37e4`,
and
`sha256:a1efe2b300503b84a4085516f7b494c9361ef6dfff71a43dc2575945346cb150`.
The exact approval subject is
`sha256:7016b9f127ef79ee5bc8eb8245be1c4d58d177fc07440118c10d06eacb1c545f`;
the 9,569-byte operator packet is
`sha256:ddddcac933ae0bb5a5f3c04ccaa5ad18a6367b7c5052562df718a21ef4f7bae9`.

The exact eight-test hermetic matrix passed 8/8 in 89.80 seconds. A fresh static
inspection again found exactly one `live_review_mutation_designated_target`,
one `bpr_live_mutation` label, a 1,800-second timeout, only the exact supervisor,
inventory, and approval arguments, and explicit credential/loader environment
removal. Its deliberately invalid private placeholder was deleted afterward;
the test was not executed. The combined repository gate then passed formatting
for all 373 files and strict warnings-as-errors tidy for all 227 translation
units. Documentation validation reports `DOCUMENTATION_OK`, and
`git diff --check` passes.

No approval file, credential, network request, browser action, comment, or
mutation was created. The packet is now mechanically ready for independent
capability-critical review and then an exact operator decision. Any subsequent
bound-source, capsule, inventory, subject, or packet byte change reopens this
refresh and invalidates that decision.

## Capability-critical review — blocking result

The independent capability-critical review found three reproducible blocking
counterexamples and two related high-risk gaps. The checked-in packet is
therefore invalid and must not be approved or executed.

- The spent marker is scoped to the approval artifact's mutable parent, so the
  same approval identity can mint another grant under a second or recreated
  private parent. Approval opening also follows symlinked earlier ancestors.
  Backlog BPR-0261 owns replay authority and descriptor-walk path safety; its
  attempted local-filesystem repair failed independent review and now awaits
  the exact threat-model decision packet.
- The tests-disabled application accepts the attended approval argv directly
  without authenticating the verified supervisor or binding the inventory, so
  direct launch bypasses the claimed capsule-only path. Static capsule checking
  also releases nested identities before launch. Backlog BPR-0262 owns launch
  provenance and retained capsule identity.
- The indeterminate-recovery counterexample is repaired by done BPR-0263. It
  persists a bounded digest-bound pre-POST baseline, distinguishes prepared
  from possibly-transmitted attempts through a native transport witness,
  refuses pre-existing direct receipts, and performs zero-POST restart
  reconciliation only for possibly-transmitted attempts. Its independent
  capability-critical review accepted the repair after the final 8/8 matrix.

The reviewer independently confirmed the current packet hashes and manifest
shape and ran no live/network/credential action. Those mechanical checks do not
overcome the two remaining authority counterexamples. This card is parked in
Backlog until BPR-0261 and BPR-0262 are done, the packet is regenerated again,
the complete gates pass, and a fresh capability-critical review has no blocker.

## Dependencies met, remaining work is not mine — 2026-08-18

[[BPR-0261]] and [[BPR-0262]] are both `60-done/`, so the blocking
counterexamples this card recorded are answered: the launch-provenance and
retained-capsule-identity gaps are closed by [[BPR-0351]] through
[[BPR-0354]], and replay authority by BPR-0261.

What remains is not implementation:

1. **Regenerate the capsule, source manifest, approval subject, and packet.**
   Every bound source byte moved — `main.cpp`, `github_application.cpp`, the
   supervisor, and two new `src/app` files are now in the attended-review source
   surface — so the recorded digests are stale by construction. This cannot be
   done here: the tests-disabled capsule graph will not configure, because a
   fresh configure resolves a system Qt and fails the verified
   KSyntaxHighlighting build. [[BPR-0385]] owns that.
2. **A fresh capability-critical independent review** of the regenerated packet.
3. **An exact attended operator decision.** This card prepares the packet; it
   has never had authority to approve or execute it.

So the card moves to `40-parked/` behind the operator verdict rather than to
`20-ready/`: (1) is environment-blocked and tracked, and (3) is the terminal
requirement in any case. No live lane was executed, no credential accessed, no
mutation submitted.

## Packet regenerated — 2026-08-18

Step (1) is complete. [[BPR-0385]]'s fix held: the tests-disabled Release
graph configured and built fresh at `build/bpr0256-release`, and the first
capsule build since the graph was unblocked exposed and closed two real
defects on the way — done [[BPR-0388]] (the capsule surface still pinned the
pre-BPR-0328 executable name) and done [[BPR-0389]] (the verifier could not
parse the universal Qt's per-architecture Mach-O framing). With both fixed
the capsule builds and verifies: `designated-live-macos-capsule:pass`.

The regenerated evidence is
`docs/verification/reviews/designated-live-review-publication-v002/`:

- capsule application `sha256:26719adf…`, supervisor `sha256:d5490007…`,
  inventory `sha256:053f5e99…`, all under source digest `1a6c971b…`, whose
  31-entry manifest was reproduced byte-exactly during preparation;
- a schema-3 approval subject (`sha256:929a013d…`) rendered through the real
  `designated_review_approval_subject` renderer, binding the supervisor and
  inventory digests and the existing replay namespace, which was loaded
  read-only — it was not reset, and the packet forbids resetting it;
- the digest-bound read-only capsule image (`sha256:4603539d…`) built and
  `stage-zero`-verified as separate release evidence, with the packet
  explicitly not claiming the CTest lane runs from the mounted image.

The current eleven-selector hermetic matrix passed 10/10 at the repinned
app-source digest, recorded in the RFC-BPR-0008 ledger. Two gates remain
before the operator decision may be requested, both deliberately sequenced
behind the dev-binary hold protecting two other digest-bound attended
packets: `application_review_round_trip_composition_contract` runs the dev
application binary and waits for that hold to lift, and step (2)'s fresh
capability-critical independent review runs after the full matrix is green.
Step (3), the exact attended operator decision, remains the terminal
requirement. No live lane was executed, no credential accessed, no mutation
submitted, and no approval file exists.

## The held gate ran, so this card is not parked on a person — 2026-08-19

The previous section parked this card behind two gates and then filed it under
`awaiting-owner-verdict`. That was half right. Step (3) is genuinely attended,
but steps (2) and the round-trip contract are not, and a card whose *next*
action is unattended does not belong in `40-parked/` — the lane means "nothing
is in the way but a person or a live service", and something was.

The first of the two gates has now run:

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| the composed review round trip still holds at HEAD | `application_review_round_trip_composition_contract` | `ctest --test-dir build/dev -R '^application_review_round_trip_composition_contract$'` | it drives the real dev application binary through the composed round trip and fails if the composition regressed | **pass**, 26.61 s, exit 0 | this commit, at `0c4b3b7` |

It ran without lifting the dev-binary hold, which is the part worth recording.
The hold exists because three pending operator packets bind the exact bytes of
`build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review`,
and three earlier rebuilds each invalidated a pending packet before an operator
reached it. `ctest` does not build. Running a selector against an
already-built tree therefore consumes the binary without changing it, and that
was verified rather than assumed: the digest was
`sha256:7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93` at
51,085,872 bytes before the run and byte-identical after it, with
`git status --short` clean on both sides.

So the sequencing note in the previous section — that this contract "waits for
that hold to lift" — was stricter than the hold requires. What the hold forbids
is a **rebuild**, not a **run**.

What remains is step (2), the fresh capability-critical independent review of
the regenerated `designated-live-review-publication-v002` packet, and then step
(3). Step (2) is unattended and is the reason this card is in
`30-in-progress/` rather than `40-parked/`. The card returns to `40-parked/`
under `awaiting-owner-verdict` when step (2) has no blocking finding, which is
the point at which the operator decision really is the only thing left.

## Step (2) ran, and v002 is invalidated — 2026-08-19

The capability-critical review returned **BLOCKING**, and the finding is not
about the mechanism. It is that the packet does not describe this repository.

Every claim below was reproduced directly rather than accepted from the review.

### The packet's bound source surface does not match the tree

`source-manifest.txt` binds 31 files by digest. Recomputed at HEAD:

```
matched 25 / 31   mismatched 6
  src/app/CMakeLists.txt
  src/app/main.cpp
  src/app/include/bpr/app/github_application.hpp
  src/app/src/github_application.cpp
  src/adapters/github/src/github_mutation_contract.cpp
  src/adapters/sqlite/src/review_mutation_recovery_store.cpp
```

The other 25 — every supervisor, provenance, capsule-identity, bootstrap,
observer, provider, and transport file, and `scripts/designated_live_macos_capsule.py`
— are byte-identical. The drift is confined to the application composition and
two mutation-path files.

### It was stale the day it was written

This is the part that matters more than the count. Recomputing the same manifest
against the tree at `1842216`, the commit that *introduced this packet*:

```
AT THE PACKET'S OWN INTRODUCTION COMMIT: matched 28  mismatched 3
  src/app/main.cpp
  src/adapters/github/src/github_mutation_contract.cpp
  src/adapters/sqlite/src/review_mutation_recovery_store.cpp
```

So the packet never bound the tree it was committed into. The capsule was built
from `b04b43c`, and

```
$ git merge-base --is-ancestor b04b43c HEAD
b04b43c is NOT an ancestor of HEAD   (merge-base 605aa56)
```

That commit was never merged as written; its BPR-0351/0352/0354 content reached
`main` as different commits. "The 31-entry manifest was reproduced byte-exactly
during preparation" was true of the side-branch build tree and of nothing else.

### What that would have authorized

An approval minted against subject `sha256:929a013d…` would have authorized a
binary whose `main.cpp`, application composition, and the byte shapes of two
mutation-path files are not what any reviewer of this tree reads. No hermetic
test ever executed those exact capsule bytes: the 11-selector matrix drives
dev-tree binaries built from mainline sources, and the capsule came from the
side tree.

This card's own rule already covered it — *"Any subsequent bound-source, capsule,
inventory, subject, or packet byte change reopens this refresh and invalidates
that decision"* — and the rule was written as a forward guard against future
edits. It turned out to be needed backwards.

### The mechanism is sound, and that is a real result

The review re-derived all three of v001's blocking counterexamples against live
source rather than against the cards claiming them, and all three are genuinely
repaired: the spent marker lives in the subject-bound owner-private replay
namespace with every path component walked `O_NOFOLLOW` from the filesystem
root, `F_FULLFSYNC` on the marker and its directory, and a consume built from
`renameatx_np(RENAME_EXCL)` → inode identity → unlink → retained-fd
`st_nlink==0` recheck, so hardlinks, substitutions, and crash orderings all end
in no-grant with the id burned; attended argv is refused before Qt exists unless
the capsule's supervisor authenticates over the inherited Mach right with kernel
audit trailer and dynamic code identity in both directions; and BPR-0263's
recovery surfaces are green at this tree.

So this is regeneration work, not redesign, and the drift content itself —
syntax, appearance, epoch, bundle-banner work plus GCC-visibility reshapes — is
semantically outside the designated-live lane. [[BPR-0406]] carries it.

### Three things the regenerated packet must fix

Non-blocking, and all three would otherwise survive regeneration unchanged:

1. **An undisclosed 15-minute authority lifetime.**
   `designated_review_authority_bootstrap.cpp:46` sets it, and
   `review_mutation_application_host.cpp:82-86,122-125` auto-revokes on it. The
   packet's timeout story is 1,800 s and its abort list never mentions expiry, so
   a slow attended session consumes the approval at startup and then loses
   authority mid-session with retry forbidden. Fail-closed, but the operator
   should be told.
2. **The packet misorders the launch sequence.** It says the supervisor verifies
   the child's signing identity through the Mach handshake and revalidates
   capsule identities "and only then resumes it". In
   `designated_live_macos_supervisor.cpp:661-699` the pid-keyed signing check and
   capsule revalidation precede `SIGCONT`, and the audit-token handshake
   necessarily follows resume, because a suspended child cannot send. The
   enforced invariant is real; the sentence describing it is wrong.
3. **No test evidence exercises the exact bound capsule bytes.** Regeneration
   from the committed tree closes this, provided the matrix is re-run at the
   regenerated digest rather than reused from this one.

### The verification finding this card owes elsewhere

`docs/verification/RFC-BPR-0008.md:87` still records the round-trip contract and
the review as "pending behind a deliberate dev-binary hold for **two**
digest-bound attended packets". The round-trip passed at `0c4b3b7`, the review
has now run, and the hold protects **three** packets. Corrected in this commit.

The review also found that `scripts/verify_documentation.py` crashes under
`/usr/bin/python3` 3.9 on `zip(strict=True)` at line 552 and requires ≥3.10,
which nothing states. That is not this card's; it belongs to whoever owns the
gate's invocation contract.

### Status

The dev binary is unchanged — `sha256:7b9d6be9…a78fcd93`, verified after both of
the review's `ctest` runs — so the three attended packets bound to it are intact.
No live lane was executed, no credential accessed, no mutation submitted, and no
approval file exists.

## The live packet is v003 — 2026-08-20

`designated-live-review-publication-v002` is invalidated and now carries a
pointer to its successor. The live packet is
`docs/verification/reviews/designated-live-review-publication-v003/`,
regenerated by [[BPR-0406]] from the committed tree.

What it binds, all measured on this host rather than carried forward:

| artifact | sha256 | bytes |
|---|---|---:|
| capsule application | `4227ef6636a354e6…` | 7,665,936 |
| capsule supervisor | `1f350f4fe65fed52…` | 197,200 |
| capsule inventory | `7517091535797058…` | 11,112 |
| `approval-subject.txt` | `35200c89a63bcd9d…` | 1,496 |
| `source-manifest.txt` | `b2e2c2d8f005e62f…` | 3,615 |

The manifest digest equals the `BPR_ATTENDED_REVIEW_SOURCE_SHA256` compiled into
the capsule binary. That is the check v002 failed, and it is now checked rather
than asserted.

Step (3), the exact attended operator decision, is still the terminal
requirement and has not been requested. Step (2), a fresh capability-critical
review of v003, has not run — the one that invalidated v002 does not transfer.

## Step (2) is clean; only the operator decision remains — 2026-08-20

The fresh capability-critical review of v003 returned **no blocking finding**,
and [[BPR-0406]] is done. Both remaining gates this card recorded on 2026-08-18
are now discharged:

1. regenerate the capsule, manifest, subject, and packet — done, from the
   committed tree at `8ca12d9`, with the manifest verified against the tree and
   against the constant compiled into the capsule binaries;
2. a fresh capability-critical review — done, no blocker, with three
   non-blocking findings fixed at BPR-0406 closure.

Step (3) is the whole remainder: **one exact attended operator decision.** This
card has never had authority to approve or execute it, and still does not.

The live packet is
`docs/verification/reviews/designated-live-review-publication-v003/`. The replay
namespace holds zero spent markers, so the approval identity is unspent and the
lane has never run. No credential was accessed, no approval file exists, and no
mutation was submitted.

This card returns to `40-parked/` under `awaiting-owner-verdict`, which is now
exactly true — for the first time since it was written, nothing unattended
remains on it.

## v003 invalidated by the authorized BPR-0410 source change — 2026-08-21

BPR-0410 changed bound source `src/app/main.cpp` under the product owner's
explicit authorization to replace the protected development binary. That makes
v003 immutable history rather than a packet an operator may approve. Its status
line now says so, and none of its manifest, subject, capsule, inventory, or
operator-decision bytes were rebound to the new tree.

The next unattended action is a complete v004 regeneration from committed
source followed by a fresh capability-critical review, so this card returns to
`30-in-progress/`; it is not currently waiting on an owner. Only a clean
successor can return it to an exact operator decision. No approval file was
created, no credential was accessed, and the live lane was not run.

## v004 regenerated from committed BPR-0410 source — 2026-08-21

The complete v004 unattended regeneration is now recorded at
`docs/verification/reviews/designated-live-review-publication-v004/`. It was
built from committed tree `a83bfd0` in a fresh tests-disabled Release graph at
`build/bpr0256-release`, not from the development graph.

The exact 31-entry source manifest is 3,615 bytes with SHA-256
`c5023d4f81b02c6663afa813a2a3e1876ddfcce481537feb59ee24f974fa9f79`.
That digest is compiled into both capsule binaries and selects the artifact
directory. The bound artifacts are:

| artifact | SHA-256 | bytes |
|---|---|---:|
| capsule application | `2a1c65bf1249ba25333f949deac674ded204cd9ce0653d7fdbb165fcbb4a2216` | 7,682,464 |
| signed-capsule supervisor | `6ff0d7c7949dc446e8d4e63d04d2eda762d1d6155aab84fdbb6beaae97f35a6f` | 197,200 |
| capsule inventory | `ca4fbe45e2a6ed6198a90b1b114b4617c1008bc8739c3662c2f4e7f1665a25ee` | 11,112 |
| schema-3 approval subject | `7daa46aa18dfc0cd804c9060f1b790089e17a1db972d074b38e61a78769dcb39` | 1,496 |
| operator packet | `08cc6f277cb92c89760c94f263f8c4813e6e1eca1e5bd55482d03b991bddd4e2` | 16,628 |

The approval subject reuses the unchanged, unspent approval ID
`aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`.
Preparation read the existing owner-private replay namespace without changing
it: its schema, path, and ID still match, and it contains zero spent records.
No reset or initialization command ran.

The capsule builds and verifies with
`designated-live-macos-capsule:pass`. Its separate read-only image is
29,978,636 bytes with SHA-256
`cb45d813452913f142f6128c1ede5010c5c4361ae146612f9e61a997bd0d6769`;
`stage-zero` mounted that exact image read-only, verified the inventory and
source binding from inside the mount, detached, and passed.

The first exact eleven-selector replay passed 9/11 and caught one BPR-0410
closeout omission: the production-application source aggregate expected by
`application_review_round_trip_composition_contract.cmake` still named the
pre-BPR-0410 value. It found
`f6d1c03d7fe4f50abcdaea2e93fec93d341f4325882f992f7f3d629643c2595c`
instead. The verifier pin was corrected without changing any bound production
source. The two failed composition selectors then passed 2/2 in 122.28 seconds,
and the complete exact matrix passed 11/11 in 217.92 seconds. The protected
development executable remained byte-identical throughout at 51,303,392 bytes
and SHA-256
`333e88c9298f5a57182b7b3dda9994974485ffc77f07fb7496ac9ee43f40204a`.

The v004 packet was initially **pending fresh capability-critical review and
not operator-ready**. No approval file was materialized, no credential was
accessed, no live or public observation lane was executed, and no mutation was
submitted during that state.

## v004 capability-critical review is clean — 2026-08-21

The fresh independent review returned **no blocking finding**. It recomputed
the manifest and artifact identities from the current tree and directly reran
the relevant authority and adjacent effect surfaces. Its exact results were:

- 11/11 authority/composition selectors passed in 211.50 seconds;
- 6/6 provider/native-HTTPS/SQLite selectors passed in 12.97 seconds;
- 1/1 hermetic public-observer selector passed in 0.33 seconds;
- the default configured manifest contained 194 tests and zero live entries;
- documentation, `git diff --check`, capsule, image/stage-zero, replay-state,
  and protected dev-executable identity checks were clean.

The reviewed packet now says **operator-ready** and requests its one exact
decision. Its final bytes are 16,628 bytes with SHA-256
`08cc6f277cb92c89760c94f263f8c4813e6e1eca1e5bd55482d03b991bddd4e2`.
No approval file has been materialized and the lane has not been configured or
run. This card therefore returns to `40-parked/` under
`awaiting-owner-verdict`; the exact operator decision is its sole remaining
work.

The review also exposed a high-risk sequencing constraint. Any BPR-0334 edit
under `src/` makes the live-lane timestamp guard reject this v004 producer as
stale. The attended v004 checkpoint must be resolved before BPR-0334 changes
`src/`; otherwise v004 must not run and a complete successor regeneration,
fresh capability-critical review, and new exact operator decision are required.
The guard must never be weakened or bypassed.

## v004 approved, then aborted before GUI creation — 2026-08-21

The owner returned the exact approval alternative bound to subject
`7daa46aa18dfc0cd804c9060f1b790089e17a1db972d074b38e61a78769dcb39`.
The complete response and attended result are recorded in
`docs/verification/reviews/designated-live-review-publication-v004/operator-response.md`.

The private approval was materialized byte-exactly and the fresh static
manifest passed: 195 tests, exactly one `bpr_live_mutation` entry, the exact
supervisor/inventory/approval command, and no credential or verdict environment.
The attended lane then failed closed after 1.84 seconds with no supervisor
output and before a usable GUI or credential prompt appeared. The approval
remained unconsumed, the replay namespace retained zero spent records, no
credential was entered, and no mutation was submitted.

An immediate redirect-free, unauthenticated request to the same GitHub REST
pull-request endpoint returned HTTP 404. The supervisor does not expose which
early refusal branch fired, so this is recorded as a direct diagnostic fact
consistent with mandatory public-observation failure, not as an invented
stage-specific verdict. The packet forbids retry after startup refusal. The
unconsumed private approval was deleted, and v004 must not run again.

The owner decision is discharged, but the live publication criterion is not.
This card remains `40-parked/` under `awaiting-live-github`: a future attempt
needs a publicly observable designated target and a complete successor packet,
fresh capability-critical review, and new exact operator decision. BPR-0334 may
now proceed; its source edit will make the already-aborted v004 producer stale
as expected.

## v004's approved source has drifted — 2026-08-23

The frozen v004 approval no longer describes the tree. Checked entry by entry
against `source-manifest.txt`: **28 of 31 still match, 3 have moved.**

| File | v004 | Now | Moved by |
|---|---|---|---|
| `src/app/CMakeLists.txt` | `b758c2db…` | `edf82fc3…` | [[BPR-0364]] |
| `src/app/main.cpp` | `e49260fd…` | `7347389b…` | [[BPR-0435]], [[BPR-0440]] |
| `src/app/src/github_application.cpp` | `990d179d…` | `4666edeb…` | [[BPR-0436]], [[BPR-0437]]/[[BPR-0441]], [[BPR-0442]] |

Nothing is wrong with the packet, and its manifest is deliberately left alone:
it records what the owner actually reviewed, and `verify_documentation.py`
checks only *pending* packets for exactly that reason. The v004 verdict remains
valid for the bytes it names.

What has changed is that those bytes are no longer the ones a live run would
execute. This is the same situation [[BPR-0410]]'s change created for v003,
which is marked `INVALIDATED … a regenerated v004 is required`. By that
precedent this card now needs a **v005 regeneration** from current committed
source before the parked live attempt can proceed; running v004's approval
against today's tree would authorize a digest the owner never saw.

Recorded rather than acted on. Regenerating an attended capsule and asking for
a fresh verdict is this card's own work and needs the owner, which is why it is
parked.

Worth noting how long this went unseen. Every one of the six commits above was
a legitimate, necessary repair — the Windows lane could not build without most
of them — and none of them is wrong. The drift is a consequence nobody owed a
check for, because the gate that would catch it deliberately skips answered
packets. That is the right gate; this is the missing habit beside it. A repair
that touches a file an answered packet binds should say so on the parked card
at the time, not leave it to be discovered.

## The designated target is public — 2026-08-23

Recorded verbatim:

> I have made BPR-Test public

The direction was verified immediately with the same bounded diagnostic the
v004 abort recorded, one credential-free redirect-free GET of
`https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1`:

- HTTP 200 with `base.repo.private: false` — the observation gate's 404
  cause is gone;
- PR 1 is open, base `main` at
  `e2a44aa8fef704de7373463222b1872e796918c6`;
- head `codex/disposable-live-review-20260810` at
  `17990ba0c2fb50338141f35c0717f0f99639c7ba` — byte-identical to the head
  the v004 approval bound, so the target has not drifted since the owner's
  decision. The v005 subject re-binds the current head regardless.

So the live-service half of `awaiting-live-github` is answered, and what is
in front of this card now is a platform, which is a lane. The next actions
are unattended macOS work — the v005 regeneration from committed source
(three bound sources moved since v004; see the drift note above) and a
fresh capability-critical review — followed by the terminal new exact
operator decision. The card moves to `22-ready-mac/`. v004 remains aborted
and must not be retried; no approval file exists, no credential was
accessed, and no mutation was submitted today — the diagnostic was
read-only.

## v005 regeneration claimed — 2026-08-24

This macOS session claims only the unattended regeneration and review phase at
committed base `1396457`. It will rebuild the tests-disabled Release capsule
from the current 31-file source surface, create a new v005 source manifest,
approval subject, and operator packet, replay the capsule/image, focused,
documentation, and hygiene gates, and obtain a fresh capability-critical
review. The card returns to `40-parked/` only if those exact v005 bytes are
clean and the operator decision is the sole remainder.

The live lane remains out of scope. This phase does not materialize an
approval, access a credential, invoke the attended supervisor, issue a GitHub
request, or submit a mutation. The answered and aborted v004 artifacts remain
immutable history and must not be retried.

## v005 regenerated from current committed source — 2026-08-24

The complete unattended v005 regeneration is now recorded at
`docs/verification/reviews/designated-live-review-publication-v005/`. It was
built from committed tree `bb8212a` in a fresh tests-disabled Release graph at
`build/bpr0256-release`, not from the protected development graph.

The exact 31-entry source manifest is 3,615 bytes with SHA-256
`0608914ff88f395a07b5de937e18a9191c6cd0a6f6fef3ea9be38db9d3941114`.
That digest is compiled into both capsule binaries and selects the artifact
directory. The bound artifacts are:

| artifact | SHA-256 | bytes |
|---|---|---:|
| capsule application | `b071b432f4682803ec0c63ed8977cfa6d469f2cb8a5d0ccf40dabcc0bb1fa18e` | 7,718,720 |
| signed-capsule supervisor | `1b6778a292377ead1d12a11a71cd316f7e63a1bf19d5583800cb796619a21e1d` | 197,200 |
| capsule inventory | `09afbd31d5893e699a1c861e2db8544bc9a21539bda72b3186b197379b25129e` | 11,112 |
| schema-3 approval subject | `6f158a0801c22219b77a3dfe44dac390de6d77fc520ab419565678ab72847bd3` | 1,496 |
| invalidated operator packet | `066ead4a1dfbbb668a8e3ca9adf639b2d24d2f3a9d8b83e6f6b7364320f498ae` | 17,809 |

The approval subject reuses the unchanged approval ID
`aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`.
Preparation inspected the owner-private replay namespace read-only: its
identity still matches, it contains zero spent records, and no reset or
initialization command ran.

The capsule build returned `designated-live-macos-capsule:pass`. Its separate
read-only image is 29,983,469 bytes with SHA-256
`a6f55ee4cd1a1d39a0c238af931fc14c5df453a2b423a073298c8f85e71d3be9`;
`stage-zero` mounted that exact image read-only, verified the inventory and
source binding from inside the mount, detached, and passed. The exact
authority/composition matrix passed 11/11 in 244.79 seconds. The protected
development executable remained byte-identical before and after at 52,067,088
bytes with SHA-256
`cac2d05d5c951e58d0d5144ca8bb9eebd60428db46a8c4bbf2c20fb084d406ff`;
no development configure or build command ran.

The packet remains **pending fresh capability-critical review; do not run or
approve**. No response or approval file exists, no credential was accessed, no
attended supervisor or GitHub request ran, and no mutation was submitted. A
clean independent review is the only unattended work left before these exact
bytes may become an operator decision.

## v005 invalidated by capability-critical review — 2026-08-24

The fresh independent review returned three blocking findings. v005 cannot be
made operator-ready by changing prose; its exact binaries and command topology
are rejected history.

1. The static verifier exits before the capsule root is opened and captured.
   Whole-root A-to-B substitution in that interval makes B the new baseline, so
   the final revalidation cannot detect it.
2. The verifier and the later application-bound inventory digest open the
   inventory pathname separately. They can therefore consume different inodes.
3. `stage-zero` verifies and detaches the read-only image without running stage
   one, while the registered live lane still launches the writable
   plain-directory capsule.

All prescribed gates nevertheless passed: 11/11 authority/composition, 6/6
provider/native-HTTPS/SQLite, 1/1 hermetic public observer, 198 default tests
with zero live entries/labels, capsule and stored-image stage-zero, 107
documentation self-tests, documentation, and diff hygiene. The reviewer also
recomputed every v005 artifact identity, the 31-file manifest, compiled source
digest, Release/tests-disabled selection, and zero-spent replay state. Those
results demonstrate that the existing tests are false-green for the injected
interleavings; they do not soften the findings.

[[BPR-0448]] owns one retained capsule root and inventory across verification,
hashing, capture, and launch. [[BPR-0449]] owns execution of the actual live
lane from the digest-bound read-only image for the full child lifetime. After
both land, this card requires a complete v006 rebuild, fresh identities, fresh
capability-critical review, and a new exact operator decision. No v005 response
or approval exists, and no credential, live launch, GitHub request, or mutation
occurred.

## v006 regeneration claimed — 2026-08-24

[[BPR-0448]] and [[BPR-0449]] are now `60-done/`, and both repaired ranges have
clean capability-critical review. This macOS session claims the complete
unattended v006 regeneration from committed source after the BPR-0449 closeout
commit. It will rebuild the tests-disabled Release capsule and exact stored
DMG, regenerate the source manifest, approval subject, and operator packet,
configure and inspect the sole opt-in manifest, replay the complete safe
matrix, and obtain fresh capability-critical review of those exact bytes.

The v006 packet must bind the BPR-0449 topology and its limits: one
producer-bound captured launcher, one literal image digest, one owner-private
mount parent, bounded process-group cleanup, post-child revalidation, and
ordinary detach. It must not claim that separate DMG builds are byte-identical,
that the owner-writable backing image is immutable while mounted, that
`hdiutil` attach is descriptor-atomic, or that repository-local code protects a
generated CTest plan from same-user rewriting.

This claim does not materialize an approval, request an operator verdict,
invoke `--attended-launch`, access a credential, perform public observation,
issue a network request, or submit a mutation. v005 remains invalidated
history. Any v006 packet remains non-authoritative until its own fresh critical
review is clean and the owner returns the exact decision it requests.

## v006 candidate regenerated from repaired topology — 2026-08-24

The complete pending candidate is recorded at
`docs/verification/reviews/designated-live-review-publication-v006/`. A fresh
offline, tests-disabled Release graph at `build/bpr0256-v006-release` produced
the capsule and ran the production-shaped probe from its mounted read-only
image. No development configure or build command ran during production; the
protected development executable remained byte-identical before and after at
52,067,088 bytes and SHA-256
`cac2d05d5c951e58d0d5144ca8bb9eebd60428db46a8c4bbf2c20fb084d406ff`.

The exact candidate identities are:

| artifact | SHA-256 | bytes |
|---|---|---:|
| 37-entry source manifest | `4f5a9d57e0383113f5f368eb3f1923936f06148987062563e276a5b25faf36fc` | 4,301 |
| capsule application | `a0ff772d65a3ae55a65aee664dd6883f415d7783396c7c5f266dda28aeac37bb` | 7,718,720 |
| signed-capsule supervisor | `c46f8e69c5265c2b57afe3e7a6243fd89c9f86e9076d8604c47e7525e6f3df5c` | 240,496 |
| capsule inventory | `deae04e7bf4f24cfc4ea34bcdc578bf37a83bb763a7e37fdbc5206b6c41ca986` | 11,112 |
| exact stored DMG | `2601e22323168ca82ea173789fb92eafb3dc3698d159acb5107643eabdbf72fb` | 30,013,110 |
| stage-zero launcher | `b969b90a26b87d862e04c5c2064636ffd875ef3361ff392cfa5f4f8df998da05` | 60,716 |
| schema-3 approval subject | `984a1ab0ce7971a369052be1be60ef3d9c23b7161cb2e29ab368e5e9cc4dc1bd` | 1,496 |

The checked-in manifest is byte-identical to the producer manifest and binds
all 37 current source entries. The stored image digest file contains the exact
DMG SHA above. The image mounted for the probe, the application and supervisor
matched their compiled source identity and inventory, the probe exited
successfully, and ordinary detach left no v006 attachment.

The complete safe evidence replay passed: 12/12 authority, composition,
capsule, provenance, identity, isolation, and architecture selectors; 6/6
provider, native-HTTPS, SQLite recovery, restart, and hermetic-observer
selectors; 44/44 capsule/stage-zero hostile tests; 37/37 isolation hostile
tests; 126/126 architecture hostile tests; the direct architecture gate; and
the 399-file formatting gate. The default graph has 199 tests and zero live
entries or labels. A static-only opt-in graph has 200 tests, exactly one live
entry and label, the fixed captured-byte launcher, literal image digest,
owner-private mount parent, 1,800-second timeout, and no credential or verdict
environment. Its deliberately invalid private placeholder was deleted after
`ctest --show-only=json-v1`; the live test was not executed.

The repository-wide warnings-as-errors tidy replay reproduced the six unique
pre-existing diagnostics in untouched files recorded in the packet; it found
no v006 change-set diagnostic. The documentation validator reports
`DOCUMENTATION_OK`, all 107 documentation self-tests pass, and
`git diff --check` passes. The candidate packet remains **pending
capability-critical review; do not run or approve**. That review must accept
the exact checked-in candidate and the disclosed owner-writable backing-DMG,
pathname-based attach, and trusted generated-CTest-plan limits before the card
can return to an operator-decision lane. No approval was materialized, no
credential, public observation, or network was accessed, and no attended or
remote mutation command ran.

## v006 pending-candidate critical review clean — 2026-08-24

Fresh capability-critical review accepted exact pending candidate `ae63649`
and range `77ade3e..ae63649` as clean. The reviewer independently validated all
37 source-manifest and 95 capsule-inventory entries, every checked-in and
stored-image identity, the schema-3 approval subject, the unspent replay
namespace, and the protected development executable. A separate 138-step
offline tests-disabled Release build and mounted probe passed, as did 44/44
capsule tests, 37/37 isolation tests, the focused 15/15 CTest matrix, both
default and static-only opt-in manifest inspections, documentation, and diff
hygiene.

The fresh build retained the source, launcher, and supervisor identities but
produced different application, derived inventory, and DMG bytes. The review
localized that drift to Mach-O UUID/signature bytes and reordered semantically
identical resources and accepted the packet's exact-stored-artifact boundary;
v006 makes no reproducible-build claim. The review also retained one additional
high-risk non-blocker: synchronous `hdiutil` attach/detach calls have no
independent subprocess timeout, so a genuinely stalled DiskImages process is
outside the tested 1,500-second post-attach stage-one cleanup bound.

The operator-decision revision now discloses that fourth limit and records the
first review. Those prose changes create new packet bytes. This card therefore
remains `30-in-progress/`, accepts no operator response yet, and requires one
fresh capability-critical review of the exact final packet before it can park
on an owner decision. No approval, credential, attended launch, public
observation, network request, GitHub interaction, or mutation occurred.

## Exact operator-decision packet accepted — 2026-08-24

A second fresh capability-critical review accepted exact HEAD `9bca2b3`, range
`77ade3e..9bca2b3`, and the final 22,490-byte operator packet with SHA-256
`9ecf44272a19ad6993589690254b0f3b778da27b51090bf9a1fabfd238d7ef7a`.
The reviewer recomputed the 37-entry source manifest, 95-entry stored capsule
inventory, subject and packet identities, every producer artifact, the unspent
replay namespace, and the zero-attachment state. It confirmed that
`ae63649..9bca2b3` changes documentation only and that the singular operator
approval binds the subject, application, supervisor, inventory, source,
launcher, image, mount parent, target/head, and all four disclosed limits.

No packet byte changes after that review. These exact bytes are now the current
operator decision at
`docs/verification/reviews/designated-live-review-publication-v006/operator-packet.md`.
The card moves to `40-parked/` under `awaiting-owner-verdict`; the exact
unqualified approval or rejection alternative in that packet is its sole
remaining current action. Approval is not inferred from discussion, a partial
response, or any earlier version.

The lane remains unexecuted. No approval file, credential, attended launch,
public observation, network request, GitHub interaction, remote mutation, or
spent record exists, and no image remains attached. If approved exactly, the
private approval, final static manifest inspection, attended run, and GitHub UI
observation remain future attended work. A rejection is evidence and reopens
repair work; it is not softened into an approval.

## v006 approved, consumed, and cancelled at manual token entry — 2026-08-24

The owner returned the packet's exact singular approval. Direct preflight
matched the 22,490-byte packet, 1,496-byte subject, every stored artifact hash
and size, all 37 source entries, the owner-private unspent replay namespace,
the empty mode-`0700` mount parent, the detached image, and the clean worktree.
The materialized approval was 1,576 bytes with SHA-256
`1bd771dbbacdbbf7482fcdac7e061f370ae17cb46323eada97b0b7596f11c862`,
mode `0600`, one hard link, and an owner-private mode-`0700` parent.

The fresh static manifest retained the reviewed shape: 200 tests, one live
entry and label, a 1,800-second timeout, the fixed captured launcher and exact
image/source/mount/approval arguments, 24 environment removals, and no verdict
override. The exact lane then ran once. It consumed the approval, durably spent
approval ID `aa6dd849a0bc31e36e8585d1b5b8fc0d065f653ee92218c9f770729766c610d9`,
mounted the exact image read-only, and reached the shipped Qt application.

The first usable interaction was the existing generic password-echo dialog:
`Authenticate GitHub host` / `Enter a GitHub credential for this connection.`
The owner expected a login, OAuth/device-authorization, or explicit
authentication-method flow and did not understand this as a request to mint
and paste a personal access token. Once that current product behavior was made
explicit, the owner rejected it and cancelled without entering a credential.
The application closed normally, the image detached ordinarily, and CTest
reported a safe attended cancellation after 483.51 seconds. No line or body was
selected, Publish was never pressed, no POST was submitted, and no GitHub UI
comment exists. The durable spend makes v006 non-retryable. The complete result
is recorded in
`docs/verification/reviews/designated-live-review-publication-v006/operator-response.md`.

This is not a BPR-0256 mechanism defect: the packet and current RFC-BPR-0002
product path deliberately selected manual token enrollment through the Qt
sensitive prompt. It is a product-sequencing finding. The owner reaffirmed that
the requested login flow was supposed to be built before this attended course.
That flow is already allocated to deferred RFC-BPR-0016. BPR-0276 must finish
its owner-attended real-service capture; BPR-0277 through BPR-0282 then
implement and verify device authorization; and BPR-0318 records the
distributable identity required for production selection. BPR-0256 therefore
returns to `10-backlog/` with BPR-0318 as its transitive login-flow prerequisite.
It must not request another live-publication approval until that dependency is
Done and a successor packet binds the resulting authentication path.

## Lane — 2026-09-01

[[BPR-0318]] is done, so the login-flow prerequisite this card returned to
`10-backlog/` for is met: the macOS Release artifact selects the recorded
production client and the device flow is the shipped authentication path.
The next action is unattended macOS work — a v007 regeneration of the capsule,
source manifest, approval subject, and packet from committed source that binds
the device-authorization login path, followed by a fresh capability-critical
review — so the card moves to `22-ready-mac/`. It is deliberately sequenced
after the portable slices that can still change bound `src/app` sources, since
every one of them would invalidate a regenerated packet. The terminal
requirement is unchanged: one exact attended operator decision, then the live
lane against the now-public `MikeMarcin/BPR-Test#1`.

## Lane — 2026-09-02

The [[BPR-0521]] question is decided — old-revision continuations stay
unanchored, nothing further rebuilds the binary for it — so that hold is
lifted. The sequencing rule above still applies: RFC-BPR-0017's
implementation is in progress and its composition card ([[BPR-0305]]) will
change bound `src/app` sources, so the v007 regeneration is scheduled after
that lands rather than binding a digest it would move.

## v007 regeneration claim — 2026-09-04

All known source-changing portable work is committed at `d9006237`, including
ordinary Public preview entry and the owner-approved persistent device-session
policy. BPR-0309's read-only final review is running; the v007 producer may build
these frozen bytes while that review finishes. Any required source correction
invalidates the candidate and requires rebuilding before an operator decision.

Prepare a fresh tests-disabled Release capsule and stored image, a new approval
ID in the existing replay namespace, the canonical approval subject, and the
successor exact operator packet. The packet must use production browser/device
sign-in and explicit Sign out cleanup, not the rejected manual-token flow or
an assumption that quitting erases an accepted device grant. The v006 approval
remains spent and non-retryable; never reset its namespace. Inspect only the
static opt-in graph with an invalid private placeholder, run focused authority
checks and the production-shaped mounted probe, then obtain fresh critical
review of the exact final artifacts and packet. This preparation creates no
approval and authorizes no live launch, credential operation or mutation.

Task-created producer root: `build/bpr0256-v007-release`, retained only while
building/reviewing the capsule and while the exact attended packet consumes
its stored artifacts. Static graph and helper/log roots are disposable after
inspection. The final packet will bind an owner-private empty mount parent.

### macOS compiler and scoped-tidy repair

The first current capsule build failed on Apple Clang's
`-Wshadow-uncaptured-local`: the supervisor's move capture reused its outer
local name. The local is now `originalSpawn`; the retained callable and launch
ordering are unchanged. Scoped strict tidy also exposed three signed-bitmask
diagnostics in that production-only translation unit. Explicit unsigned
operands preserve the same owner-write and POLLIN predicates. The inspected
twelve-source application inventory was repinned after these exact changes.

The Release supervisor builds, strict tidy for its complete translation unit
passes, formatting and direct architecture/core boundaries pass, and final
`macos_supervisor_identity_continuity` plus
`application_review_round_trip_composition_contract` pass 2/2 in 41.89 seconds.
The earlier six-selector authority/capsule/default-isolation/public-observer
course passed 6/6 in 74.40 seconds. It exercised fixtures only. The first
intermediate image's mounted production probe passed, but those image bytes
are obsolete after the tidy repair and will be removed; the final producer
must rebuild and probe the committed repaired source before identities or an
operator packet are finalized. No approval or live authority was created.

## Final v007 candidate prepared — 2026-09-04

The [v007 operator packet](../../verification/reviews/designated-live-review-publication-v007/operator-packet.md)
is 16,002 bytes with SHA-256
`565043fb548ee9896a1b6242b1a745ea0dad60ad70d4eaf08391e11aa2a65539`. It binds final committed
source `2a043335f649375d56f64590df8d0af8d181a4c0`, the 37-entry manifest
`f2325e97fe25d6ca8e9d87568f522534027451dba8ab4a1912ae3baa26bfba45`, and the canonical approval subject
`015ad8a7636483f4258ab7f8ac22af841ba48bdeb924f324b77abf01631e6921`. The application, supervisor, inventory and exact
stored image are respectively:

- `e8d2b7006f6da8aba8b006c4e933624cc8286a68136fba1f7d6f17e1efea8fcb` (8,042,624 bytes);
- `7bd25da312afba16c72dfe64c9432609ac164f4aa414422a753cd92ac7bb7847` (239,312 bytes);
- `8cc606342009652d9813d31d1860e65ec336c7dee352120f3c6fb00c9d78f7f2` (11,112 bytes);
- `e53a6dbbe44318f059e7132ed71ac3fd7f76e896840fcb35f26af4c1ec1f3e76` (29,746,780 bytes).

The final capsule build and mounted production-shaped preflight pass; ordinary
detach leaves zero v007 image attachments. Both obsolete intermediate source-
artifact roots were removed. The final producer root and empty mode-0700 mount
parent remain active consumers for exact critical review and the attended
packet. The read-only production formatter/load helper lives temporarily at
`/tmp/bpr0256-v007-subject.cpp` and `/tmp/bpr0256-v007-subject`; its artifact
metadata and inspection logs are `/tmp/bpr0256-v007-*` and will be removed after
review consumes their results. No source checkout was created as evidence.

The configured default graph contains 277 tests with zero live exceptions.
Static opt-in inspection contains 278 tests, exactly one live entry/label,
the exact captured-byte preloader/launcher/image/source/mount/approval command,
a 1,800-second timeout, 24 environment removals and no verdict override. The
official configured-isolation validator passes with 278 configured tests,
254 source registrations and `macos-sandbox-exec`. Its deliberately invalid
private placeholder and parent were removed; the static graph remains only
for review and is never executed. A real approval is absent.

The existing namespace is unchanged and still contains the spent v006 ID.
The new ID `ab4a37dd919851edb63992fb5ebc311d5def51349733e4d05b77be9c6166f0d3` is unspent. The packet uses production
browser/device sign-in, verifies the operator's intended actor, and explicitly
Signs out before quit because the separately approved device policy retains
accepted grants on ordinary shutdown. This is pending exact critical review;
no owner response may yet activate it. No live observation, credential,
attended launch or mutation occurred during regeneration.

## Exact v007 review and attended handoff — 2026-09-04

Closeout review runtime: native agent (gpt-6-astra, xhigh).
The opposite model family was unavailable through this driver. The bounded
critical review accepted candidate `ee159dba`, production source `2a043335`,
and packet SHA-256
`565043fb548ee9896a1b6242b1a745ea0dad60ad70d4eaf08391e11aa2a65539`.
No packet bytes changed after review.

- Blocking findings: none. No existing acceptance regression was found.
- Non-blocking findings: none. The retained manual-token sentence in this
  card's required final state was reconciled to the current device flow.
- Verification failures: none unresolved. The initial compiler shadow and
  three signed-mask tidy failures were repaired; final build, mounted capsule
  preflight, complete-TU tidy and 2/2 recheck pass. Earlier 6/6 focused results
  remain valid.
- Independent evidence: all eight artifact hashes, all 37 committed source
  entries, both embedded source identities and all 95 inventory entries
  match. Fresh CTest show-only enumeration confirms default 277/zero live and
  static 278/exactly one live with matching command/properties. The new replay
  ID is unspent; the placeholder is absent; the mount parent is empty, 0700.
- Material limitations: this review performed only reads, hashing and static
  enumeration. The packet's four disclosed runtime limits still apply. No
  live observation, credential operation, approval, mutation, attended cleanup
  or new Windows/Linux result is claimed.

The exact packet is now active for the owner's decision. BPR-0256 parks as
`awaiting-owner-verdict`; its dependencies are done and its unattended work is
complete. No owner response or private approval artifact exists for v007.
Actual browser/device login, the single publication, web inspection, explicit
Sign out, normal exit and detach remain attended evidence. The v006 response
and the device-retention policy decision cannot authorize this new publication.

After the reviewer consumed the results, the static-only graph
`build/bpr0256-v007-live-manifest` and all nineteen `/tmp/bpr0256-v007-*`
inspection logs, metadata, manifest and formatter helper files were removed.
The final `build/bpr0256-v007-release` producer/capsule/image and empty
`build/bpr0256-v007-live-mount-parent` remain because the exact attended packet
still consumes them. Recreate and inspect the static graph with the real
private approval only after the exact owner response. No task-created
worktree remains; `git worktree list` contains only the original main checkout.
The Mac and portable ready lanes are empty. BPR-0295 remains ineligible until
RFC-BPR-0013 closes; other remaining cards depend on attended/live evidence or
another platform. No full matrix or new target-native platform run is claimed.

## Exact owner approval received — 2026-09-04

The owner returned the [v007 approval](../../verification/reviews/designated-live-review-publication-v007/operator-response.md). Removing only Markdown quote prefixes and blank-line presentation yields the exact packet approval text, with all wording and identities unchanged. The packet digest, eight artifact hash/size pairs, all 37 current and committed source entries, unspent replay identity, and empty private mount parent revalidate. This authorizes only the packet's one attended publication attempt with browser/device sign-in and explicit Sign out. The card resumes for materialization, final static command inspection and the authorized attended run; no execution result is claimed yet.

## v007 consumed; application sign-in refused — 2026-09-04

The [attended result](../../verification/reviews/designated-live-review-publication-v007/attended-result.md) records the exact one-use run. GitHub browser authorization completed for the operator-reported MikeMarcin account, but the application refused data before session establishment. No PR or comment was opened/submitted. Explicit Sign out cleared the diagnostic, normal shutdown passed in 248.79 seconds, and the image detached. The approval is spent and cannot be retried. BPR-0559 investigates the supported-response failure and BPR-0560 fixes the owner's inability to copy the diagnostic. This card returns to backlog behind those repairs; no successor approval is requested before their work is concrete.

Consumed-resource cleanup: after the run result was recorded and zero attachments verified, the v007 producer/image tree, static CTest graph, empty mount parent, empty private approval parent, and two temporary run-state/configure files were removed. No approval or replay marker was reset. The answered packet, response and result remain immutable. Existing dev and application-Release build roots are reused for the repairs.

## v008 regeneration claim — 2026-09-05

The owner resumed implementation of the RFCs. RFC-BPR-0008 is the first
unfinished RFC; all BPR-0256 dependencies are now Done. BPR-0559's nine-part
capability repair is committed in `b14a69d0`, and BPR-0605 observed live sign-in
as MikeMarcin, all four supported capabilities and explicit Sign out. Its
separate post-quit UI-inspection relaunch was disclosed and cleaned up; it
does not extend any live authority.

Resume unattended preparation of a v008 candidate from the current committed
source, retaining the existing exact designated target and one-comment scope.
Build and inspect the signed capsule, stored read-only image, source inventory
and static opt-in plan; use fixture checks for changed dependencies and review
the exact final artifacts. No approval materialization, live launch, credentials
or publication is authorized by this claim. Prior packets and responses remain
immutable and consumed. The owner explicitly defers schema-method evaluation
and advanced logging; neither is part of this slice.

Planned temporary producer: `build/bpr0256-v008-release`; static inspection graph:
`build/bpr0256-v008-live-manifest`; private mount parent:
`build/bpr0256-v008-live-mount-parent`. Retain only the final candidate while its
attended decision is pending; remove intermediate artifacts and consumed
inspection logs. Do not use an app UI getter or handle after quit; monitor the
new sanitized log for sign-in completion and verify final process absence.

## v008 unattended candidate — 2026-09-05

The [v008 packet](../../verification/reviews/designated-live-review-publication-v008/operator-packet.md)
binds a new signed capsule and exact stored DMG from committed source
`a63e92e6` (production source unchanged at `eff28443`). Its canonical subject
was rendered through the producer Application/Core libraries and the production
read-only replay loader. The namespace identity is unchanged, both prior spent
markers remain intact, and the new approval ID is unspent. No approval file was
materialized and no live command was executed.

Verification on this candidate:

- Offline Release/tests-disabled configure and `designated_live_macos_capsule`
  build passed, including production-shaped mounted preflight and ordinary detach.
- All 37 selected manifest entries match current and committed source; both
  binaries embed source digest `7256057388474bc59e3cad76611cf6223684c05fd7b006cfea96539e9fd08b94`.
  The packet binds the exact application, supervisor, inventory, image, digest
  file, launcher and canonical subject hashes/lengths. This is the existing
  selected manifest contract, not a transitive-source or reproducible-build claim.
- `ctest --test-dir build/dev --output-on-failure -R
  '^(designated_live_mutation_gate|designated_live_macos_capsule_contract|live_mutation_isolation|live_mutation_isolation_contract|application_designated_review_authority_bootstrap|designated_live_public_observer_fixture)$'`:
  6/6 passed in 99.19 seconds, with fixtures and no live service.
- Show-only CTest enumeration: default 280 tests, zero live entries/labels;
  static opt-in 281 tests, exactly one live entry/label, fixed captured-byte
  launcher command, literal image/source digests, 1,800-second timeout, 24
  environment removals and no verdict overrides.
- The official `verify_test_network_isolation.py` static check uses the exact
  configured CMake/Python paths, macOS deny profile and runner, plus the packet's
  image/digest/mount/producer and deliberately invalid private placeholder.
  An initial invocation omitted required backend arguments; a second used
  different symlink spellings for the configured tools. These were invocation
  errors, corrected without changing source, graph or enforcement. The final
  command passed: `TEST_NETWORK_ISOLATION_OK:configured=281:registrations=257:backend=macos-sandbox-exec`.
- Documentation structure, its 62 self-tests and `git diff --check` pass.
- BPR-0559's unchanged-source production composition, architecture, scoped
  tidy/format and focused repair checks remain valid. No full matrix is run.

A read-only native critical review will inspect these exact artifacts and
packet for authority/identity mismatch before the packet becomes active. The
current static graph, private invalid placeholder and `/tmp/bpr0256-v008-*`
inspection files are temporary review inputs; remove them after that consumer
finishes. Retain the final producer/image and empty private mount parent for
the attended checkpoint only. There is no new credential or publication authority.

## v008 exact critical review and pending decision — 2026-09-05

Closeout review runtime: native collaboration (gpt-6-astra, xhigh).

The read-only critical reviewer accepted the exact 17,237-byte packet with
SHA-256 `2bc3e8699cf3674b25e1c5b0e933e3e59605d0abecc130dfafde99cfec77cdfd`.
No packet bytes changed after review. Blocking findings: none. Non-blocking
findings: none. No verification failure remains unresolved.

Independent inspection matched all eight bound artifact hashes/lengths, all 37
selected source entries against the producer/current/`a63e92e6`/`eff28443`,
and all 95 inventory entries. Fresh show-only enumeration matched default
280/zero live and opt-in 281/exactly one live, including the captured launcher,
literal digests, 1,800-second timeout and 24 environment removals. The replay
namespace identity and both spent markers are unchanged; the fresh v008 ID is
unspent. The placeholder was invalid and the mount parent empty, mode 0700.

The four disclosed runtime limits and selected-manifest contract remain the
bounds of this review. Opposite-family review was unavailable. The reviewer
performed no launch, build/test execution, live query, credential operation,
approval materialization or publication. Review is not operator approval.

After that review consumed the inputs, removed the disposable
`build/bpr0256-v008-live-manifest`, its invalid private placeholder and parent,
and all eleven `/tmp/bpr0256-v008-*` inspection/formatter files. No process held
the static graph. No BetterPullReview process or v008 image attachment remains.
Retain `build/bpr0256-v008-release` and the empty mode-0700
`build/bpr0256-v008-live-mount-parent` for this pending attended consumer only.
No extra worktree exists. The owner's existing sanitized log is untouched.

The exact packet is active for the owner's decision. BPR-0256 parks with
`awaiting-owner-verdict`; the portable and Mac ready lanes are empty. The next
step is the exact v008 response, followed by artifact/replay revalidation,
private approval materialization and fresh static inspection before its sole
attended run. No previous response authorizes it. Publication, web inspection,
Sign out, normal exit and detach evidence remain owed; no RFC closeout is claimed.

## Exact v008 owner approval received — 2026-09-05

The owner returned the exact [v008 response](../../verification/reviews/designated-live-review-publication-v008/operator-response.md). Removing only Markdown quote prefixes and the blank-line presentation matches every approval word and identity. The frozen packet, all eight artifact hash/length pairs, all 37 current and committed source entries, unchanged replay namespace with two spent historical markers, fresh unspent v008 ID and empty private mount parent revalidated. No app process is running. Resume only this one approved course: materialize the private approval, inspect the exact configured command, launch once, detect sign-in automatically from appended sanitized records, publish at most the one bound comment, inspect the web result, explicitly Sign out and quit. This record grants no retry or broader mutation and claims no execution result yet.

## Approved v008 run launched once

After final exact artifact/approval validation and passing configured isolation (281 tests, one exact live lane), the fixed CTest command started once at 2026-09-05T10:16:55.694371+00:00. The app consumed and unlinked the 1,576-byte private approval, SHA-256 `560e7882cd70c01616f9aea5dd32dc743435858d6d3c3d73139e243c513fa189`, and opened from the approved image. The owner-local sanitized log baseline is 6877 bytes. Its process is running; terminal sign-in is monitored automatically. Continue only this existing process, then the one approved publication and explicit Sign out/quit. No publication result is claimed yet.

## v008 consumed; agent UI-targeting failure — 2026-09-05

The [frozen v008 result](../../verification/reviews/designated-live-review-publication-v008/attended-result.md) records an aborted attempt: name-based UI selection launched/controlled an ordinary build alongside the approved mounted app. Device-flow interaction and an in-session device-panel screenshot occurred in that wrong instance; neither is compliant approved-build evidence. No PR was opened or comment submitted. Explicit Sign out and quit completed for both instances; CTest exited 0 in 892.24 seconds and the image detached. This is cleanup success, not publication or one-launch success. The approval is spent and cannot be replayed. Operating instructions now require full mounted-path selection, immediate PID/executable checks before and after binding, prompt device-flow initiation and filtered non-secret observations. No product source defect was established. BPR-0256 will prepare a fresh subject/packet using unchanged artifact evidence and this corrected procedure before any new approval request.

Consumed v008 resources removed: the static graph, empty private approval and mount parents, and three temporary configure/isolation/run-state files. The exact producer/image at `build/bpr0256-v008-release` is retained only because immediate v009 subject/packet regeneration will consume those unchanged bytes. The successor uses a new approval ID and `build/bpr0256-v009-live-mount-parent`; no rebuild or live retry is authorized by preparation.

## v009 successor with direct owner authorization — 2026-09-05

The [v009 packet](../../verification/reviews/designated-live-review-publication-v009/operator-packet.md) reuses the unchanged v008 producer/image with a fresh canonical subject, corrected full-path/PID UI procedure and the same one-comment scope. The owner instructed, “I'm going to approve it so why even bother asking me, just make it and say I accepted please”. This direct instruction authorizes the bounded successor and overrides the local verbatim-response workflow; it is not represented as a pasted digest statement or a review of bytes not yet finalized. Finish exact critical review and static checks before materialization/launch. No prior spent approval is reused and no new run has started.

### v009 preparation evidence

At `824360af`, all eight artifact hashes/lengths and all 37 selected current/committed source entries revalidate; the retained capsule and prior passing fixture/preflight evidence are unchanged. The producer-linked canonical formatter read-only loaded the same namespace with three spent markers, rendered fresh subject `240c5949d950e4718bd59dd3d508be046c41d9c579e3243164a00dce8f6101b9`, and left all namespace contents unchanged. Fresh show-only graph: 281 tests, exactly one live lane/label, fixed command, 1,800-second timeout, 24 environment removals and no verdict overrides. The official configured-isolation validator passed (281 configured, 257 registrations, macos-sandbox-exec); documentation structure and diff hygiene pass. No repeated source tests or rebuild was needed. Temporary v009 formatter/metadata/static files serve the exact reviewer, then the already-authorized live run; remove each after its consumer.

### v009 exact delta review complete

Closeout review runtime: native collaboration (gpt-6-astra, xhigh).

The reviewer accepted packet SHA-256 `1f057d41abb1332eccfc85af4372ff25f9a7fa0dca61afec86de867af3a2fa64`, 18,673 bytes, with no blocking or non-blocking findings. The subject changes only the approval ID; the namespace has the three expected spent IDs and the fresh ID is absent. Current show-only enumeration matches the inspected 281-test/one-live graph, unchanged image/launcher/source and new mount/placeholder paths, timeout, environment removals and no overrides. Full mounted-path/sole-PID binding, abort-before-sign-in on a second instance and filtered observations address the prior procedure failure. The actual owner words are preserved and no digest statement is invented. Unchanged artifact acceptance reuses the v008 review; opposite-family review was unavailable. No live or credential operation occurred in review. The direct owner instruction now proceeds to materialization, final command inspection and this single course.

## v009 consumed; selected-file refusal — 2026-09-05

The [v009 result](../../verification/reviews/designated-live-review-publication-v009/attended-result.md) records the single approved mounted run: sign-in as MikeMarcin and all nine capability checks succeeded, and the correct designated PR opened. Selecting its sole file failed before source loading with a changed-file refusal and unavailable exact comparison history. No body was entered or Publish action taken. Explicit Sign out, quit and detach completed; CTest exit 0 in 321.64 seconds proves cleanup only. Device-field emissions are disclosed in the result; clean capture compliance is not claimed. The new approval is spent. Next: diagnose and repair the file/history path with fixtures before preparing another candidate. All live publication acceptance rows remain incomplete.

Consumed v009 resources removed after process/attachment/handle checks: the v009 static graph, retained v008 producer and image, empty v009 mount and approval parents, and three temporary configure/isolation/run-state files. No extra worktree was created. The owner log and replay namespace remain untouched. Existing dev, core-only and ordinary app-release trees remain available for offline repair work.

## Read-only diagnostic checkpoint prepared — 2026-09-05

BPR-0606 repaired the missing stage/error diagnostics. BPR-0607 now owns one
prepared ordinary Release sign-in/PR-read diagnostic to identify the history
failure before regenerating another publication capsule. The next publication
candidate depends on that observation and any proven repair. No changed binary
was launched and no new response or acceptance is invented. Prior v009 remains
consumed; the deferred advanced logging/schema work is untouched.

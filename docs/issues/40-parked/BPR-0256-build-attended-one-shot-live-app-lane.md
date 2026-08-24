---
id: BPR-0256
title: Build the attended one-shot live application lane
status: Parked
rfc: RFC-BPR-0008
depends_on: [BPR-0255, BPR-0261, BPR-0262, BPR-0263, BPR-0448, BPR-0449]
parked_on: awaiting-owner-verdict
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
  token and grants only permission for one exact publication attempt. Any
  write-scoped credential is entered through the existing sensitive Qt prompt
  after launch; it never enters CTest configuration, argv, environment, the
  approval artifact, IPC, fixtures, diagnostics, or logs.
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

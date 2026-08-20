---
id: BPR-0256
title: Build the attended one-shot live application lane
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0255, BPR-0261, BPR-0262, BPR-0263]
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
  sole opt-in `live_review_mutation_designated_target` command instead names an
  absolute signed-capsule supervisor and inventory around a freshly built
  `BUILD_TESTING=OFF` shipped application and its attended bootstrap. It never
  launches the tests-enabled fixture app or the GUI directly.
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

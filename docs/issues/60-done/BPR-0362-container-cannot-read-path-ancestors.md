---
schema: "implement-rfc.card.v1"
id: "BPR-0362"
title: "The container cannot read the ancestors every path check walks"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: []
---

# The container cannot read the ancestors every path check walks

A contained process can create a directory under its own temporary root, write
into it, and list it, and still cannot resolve or validate that path — because
both operations walk upward, and the container can read nothing above its own
`AC` directory.

This is the correct property for the boundary. A contained process should not be
able to enumerate `C:\Users`. It is nonetheless the single cause behind six of
the sixteen remaining Windows failures, in two different code paths, and it has
to be answered rather than worked around one call site at a time.

## Measured, with the outside case as the control

Opening a directory handle exactly the way the product does — `CreateFileW` with
`dwDesiredAccess` 0 and `FILE_FLAG_BACKUP_SEMANTICS`:

| path | inside the container | outside |
|---|---|---|
| `\\?\C:\` | **denied, win32=5** | opened |
| `\\?\C:\Users` | **denied, win32=5** | opened |
| `\\?\C:\Users\mikem` | **denied, win32=5** | opened |
| `\\?\C:\code` | **denied, win32=5** | opened |
| `\\?\C:\code\git\BetterPullReview` | opened | opened |
| `…\Packages\bprautomatedtest\AC\Temp` | opened | opened |

The two that open inside are exactly the two the launcher grants: the repository
it configured against, and the container's own temporary directory. Everything
above them is closed, including the drive root.

The Python-level symptom is the same fact seen through `realpath`:

```
created  = …\bprautomatedtest\AC\Temp\probeecc6d4d28435a4cc
write    = ok      listdir = ['f.txt']      os.stat = ok
resolve(strict=False)  -> OK
resolve(strict=True)   -> DENIED PermissionError winerror=5
```

`os.stat` on the leaf succeeds and `realpath` on it does not, because `realpath`
resolves every component. Walking up confirms it: `AC` and `AC\Temp` stat fine,
`…\Packages\bprautomatedtest` and everything above it does not.

## The two paths that hit it

### The product retains the whole ancestor chain by design

`validate_ancestor_chain` in `src/adapters/sqlite/src/catalog_database.cpp:2030`
starts at `\\?\C:\` — the first seven characters of the native path — and opens,
identifies, and retains **every** ancestor down to the target, comparing each
handle against its path. That is a deliberate anti-substitution defence, and
[[BPR-0053]] names it as a required Windows property: "a live
ancestor-substitution attempt while the catalog is open; require the retained
directory chain and private SQLite VFS to prevent any redirected create, open,
delete, or publication effect".

Inside an AppContainer that loop cannot get past its first iteration. So
`open_state_directory` (line 3862) cannot succeed for any path, and with it the
epoch catalog, the review ledger, the queue enrollment store, and the appearance
settings store.

Four tests fail here:

| test | symptom |
|---|---|
| `sqlite_review_ledger_storage` | `FAILED: the bounded state directory opens` |
| `sqlite_review_mutation_recovery_storage` | `FAILED: owner-private recovery store opens` |
| `sqlite_epoch_catalog_storage_contract` | hangs; `Timeout` at 30s |
| `sqlite_epoch_catalog_behavior_contract` | hangs; `Timeout` at 180s |

The two timeouts are not explained by this alone and must not be assumed to be.
A direct run under the launcher produced no output at all for seven minutes,
which is longer than either CTest timeout allows, so something is retrying or
blocking rather than simply failing. That needs its own diagnosis.

### Python strict resolution

`scripts/capture_rfc0016_device_contracts.py:2076` — `_load_attestation_review`
calls `REPOSITORY_ROOT.resolve(strict=True)`. Two tests fail on it:
`rfc0016_device_capture_security` and `rfc0016_device_live_runner_security`. Both
emit a long run of `PermissionError: [WinError 5]` naming the temporary fixture
root, which reads like [[BPR-0350]]'s temporary-directory defect and is not it.
[[BPR-0350]]'s fix is what let them get far enough to reach this.

## The decision this card carries

`strict=True` and the retained ancestor chain are not decoration. Both exist to
prove that a path was not substituted between check and use. Neither can be
dropped to a weaker form without giving up the property, so this is a real
choice and not a repair:

1. **Grant the container traverse access up the chain.** The launcher already
   grants the repository and the build directory with `icacls`. It could grant
   the ancestors too. This makes both paths work unmodified and widens the
   boundary [[BPR-0347]] built — the contained process would gain the ability to
   walk to the drive root.
2. **Anchor the chain at a readable root instead of the drive.** Walk upward only
   to a declared boundary the container can see, and state that substitution
   above that boundary is out of scope because the container cannot observe it
   anyway. Narrows the claim honestly; requires changing product code and the
   [[BPR-0053]] criterion that describes it.
3. **Do not contain the tests that need the chain.** Register the storage
   contracts outside the AppContainer prefix. This is the smallest code change
   and the largest hole: it exempts exactly the tests whose subject is
   filesystem authority from the default-deny boundary [[BPR-0235]] requires
   every test to carry.

Option 3 should not be chosen quietly. Options 1 and 2 differ in what they give
up — one gives up sandbox narrowness, the other gives up a security claim's
reach — and that is a product-owner call, not an implementation detail.

## Required final state

- One of the three resolutions above is chosen explicitly and recorded, with what
  it gives up stated.
- The six named tests pass, or the narrower truth is written down where the
  claim lives rather than the check being weakened silently.
- macOS and Linux behaviour is unchanged.
- A test fails without the change.
- If the two epoch-catalog timeouts turn out to have a separate cause, they leave
  this card and get their own.

## Verification

```sh
ctest --preset core-only --output-on-failure \
  -R '^(sqlite_epoch_catalog_storage_contract|sqlite_epoch_catalog_behavior_contract|sqlite_review_mutation_recovery_storage|sqlite_review_ledger_storage|rfc0016_device_capture_security|rfc0016_device_live_runner_security)$'
```

Run the same selectors outside the launcher as the negative control, so a pass
inside is evidence about the boundary rather than about the machine.

## Operator decision packet — 2026-08-18 — SUPERSEDED

**Status: superseded by "Operator decision packet v2 — 2026-08-19" at the end of
this card. Do not answer this version.** It offers option 2, which this card
later measured and struck; it presents option 4 as the one to examine first,
which a later section falsified as an incomplete answer; and it states a stake of
six tests that is now seven. It is kept unedited below because the reasoning it
records is what the later measurements were run against.

Parked here rather than decided. Every unattended piece of evidence this card
can produce has been produced; what remains is a choice about how much sandbox
narrowness to trade for how much security-claim reach, and that is the product
owner's.

**The bounded decision requested.** Pick one of the three resolutions above for
the retained ancestor chain. The Python `resolve(strict=True)` half follows from
the same choice and does not need a separate answer.

**What to look at.**

- This card's measured table: the drive root is `win32=5` inside the container
  and opens outside it.
- `src/adapters/sqlite/src/catalog_database.cpp:2155` — `validate_ancestor_chain`,
  the loop that starts at `\\?\C:\`.
- [[BPR-0053]]'s epoch-catalog criterion, which is the security claim option 2
  would narrow: "a live ancestor-substitution attempt while the catalog is open;
  require the retained directory chain and private SQLite VFS to prevent any
  redirected create, open, delete, or publication effect".
- `cmake/WindowsTestIsolationSetup.cmake`, which is where option 1's `icacls`
  grants would go.

**What the verdict binds to.** The three options as written in this card, at
commit `c55f2d7`. Nothing else is being approved: no live GitHub call, no
credential, no release authority, no remote mutation.

**Consequences, stated plainly.**

| option | gives up | keeps |
|---|---|---|
| 1 — grant traverse up the chain | the contained process can walk to the drive root; the boundary [[BPR-0347]] built gets wider | the substitution defence intact and unmodified; no product change |
| 2 — anchor the chain at a readable root | the substitution claim no longer reaches above that root, and [[BPR-0053]]'s criterion has to be rewritten to say so | the sandbox exactly as narrow as it is now |
| 3 — do not contain the storage tests | the default-deny boundary [[BPR-0235]] requires of *every* test, for exactly the tests whose subject is filesystem authority | both the sandbox and the claim, for every other test |

**Explicit non-scope.** This card does not change what the product does on macOS
or Linux, does not touch [[BPR-0360]]'s descriptor defect, and does not decide
whether the two epoch-catalog timeouts have a separate cause.

Six tests wait on this: the four SQLite storage contracts and the two RFC-0016
security tests. Nothing else in the Windows program is blocked by it —
[[BPR-0363]], [[BPR-0361]], [[BPR-0365]], and [[BPR-0366]] were all completed
while it stood.

## Correction — the SQLite tests are not this card

[[BPR-0368]] falsified the causation claim above. This card said the four SQLite
storage tests fail because the container cannot read path ancestors. They fail
**uncontained** too, where `C:\` opens normally, so the container is not why.

What was actually stopping them was a doubled separator in
`validate_ancestor_chain`'s own string building, which made the retained chain
compare unequal on every Windows host, contained or not. [[BPR-0368]] fixed that,
and with it `open_state_directory` returns successfully and every descriptor
check passes.

What this card still owns is real and unchanged: the container genuinely cannot
read above its own `AC` directory, the measured table stands, and
`Path.resolve(strict=True)` still cannot work inside it. But the honest test
count is smaller than claimed. `rfc0016_device_capture_security` and
`rfc0016_device_live_runner_security` also fail uncontained, so they are not
proven to be this card either.

The decision packet stands, because the ancestor-read denial is still true and
still has to be answered before anything containment-dependent can be relied on.
What must not stand is the claim that six tests are waiting on it.

## The claim, restored and made exact — 2026-08-18

The correction above was itself too broad, and the honest position is narrower
than either version.

When it was written, the four SQLite storage tests failed **uncontained** as
well, so the container could not be shown to be their cause. Three separate
product and test defects were responsible for that —
[[BPR-0368]], [[BPR-0369]], and [[BPR-0370]] — and all three are now fixed.
With them fixed:

| test | uncontained | contained |
|---|---|---|
| `sqlite_epoch_catalog_behavior_contract` | **passes** | fails |
| `sqlite_review_ledger_storage` | **passes** | fails |
| `sqlite_review_mutation_recovery_storage` | **passes** | fails |
| `sqlite_epoch_catalog_storage_contract` | fails later, at a no-delete-share assertion | fails |
| `test_network_isolation_structure_contract` | **passes** | fails |

Contained, they fail at exactly the step this card measures — the ancestors
cannot be read, so `open_state_directory` cannot walk them — with the same
messages as before: `the bounded state directory opens`,
`owner-private recovery store opens`.

So this card's mechanism was right, its test list was premature, and it is now
evidenced. **Four tests are waiting on this decision, not six and not zero.**
The two epoch-catalog tests also stopped hanging: they now fail fast contained
rather than timing out, because the failure they hit is immediate.

The decision packet is unchanged and still needs an answer.

## Option 2 was attempted and is not sufficient — 2026-08-18

The three resolutions above were written before anyone had tried one. Option 2
was implemented and measured, and it does **not** work. The card's decision
space was wrong, and this is what replaces it.

The implementation anchored the chain at the highest ancestor the process can
open: unchanged outside a container, where the drive root is always openable,
and relaxed only while contained. It behaved exactly as designed —

```
PROBE chain contained=1
PROBE chain deny err=1 retained=0 skip=1
  (seven times: C:\, Users, mikem, AppData, Local, Packages, bprautomatedtest)
```

— and then failed anyway, one step later:

```
PROBE match failed err=1
  wanted = \\?\C:\Users\mikem\AppData\Local\Packages\bprautomatedtest\AC
  got    = <final_path_of failed>
```

`CreateFileW` **succeeds** on that directory. `GetFinalPathNameByHandleW` on the
resulting handle **fails**. So inside an AppContainer a process can open a
directory it has been granted and still not resolve that handle's own name,
because resolving it walks the parent chain internally and that is denied.

`path_matches_handle` runs for every retained ancestor and for the leaf, so
anchoring the chain lower cannot help: each retained handle still has to name
itself, and none of them can.

## What that leaves

Two of the original three survive, and a fourth appears that is better than
either.

1. **Grant the container ACLs up the chain** — unchanged, and still means
   modifying `C:\`, `C:\Users`, and the invoking user's profile directory. It
   would fix both the opens and the name resolution.
3. **Do not contain these tests** — unchanged, and still the largest hole.
4. **Verify identity rather than name.** `identity_of` succeeded on the very
   handle whose name could not be resolved: `GetFileInformationByHandle` returns
   the volume serial and file index inside a container. The chain already
   collects that identity for every ancestor, and already compares identities
   elsewhere. Verifying the retained chain by file identity instead of by
   `GetFinalPathNameByHandleW` would work contained *and* uncontained, and file
   identity is a stronger statement than a name string — a rename cannot forge
   it.

Option 4 is the one to examine first. It is still a change to how the
substitution defence is expressed, so it remains the product owner's call, but
unlike option 2 it is not known to be insufficient, and unlike options 1 and 3
it gives up neither the sandbox nor the policy.

**Option 2 should be struck.** It is measured, and it does not work.

The implementation was reverted rather than left in the tree, along with a
second attempt at [[BPR-0360]]'s descriptor fix that rode along with it: with the
chain still failing, that fix remains unobservable and unproven.

## The test list, now evidenced rather than guessed — 2026-08-18

The original list was written before the other causes underneath these tests
were fixed, and was then withdrawn as unproven. With [[BPR-0368]], [[BPR-0369]],
[[BPR-0370]], [[BPR-0374]], [[BPR-0375]] and [[BPR-0377]] all landed, what is
left is measurable, and this card owns more than it first claimed rather than
less.

Every one of these passes outside the launcher and fails inside it:

| test | where it stops contained |
|---|---|
| `sqlite_epoch_catalog_behavior_contract` | `validate_ancestor_chain` |
| `sqlite_review_ledger_storage` | `validate_ancestor_chain` |
| `sqlite_review_mutation_recovery_storage` | `validate_ancestor_chain` |
| `sqlite_epoch_catalog_storage_contract` | `validate_ancestor_chain` |
| `rfc0016_device_capture_security` | `resolve(strict=True)`, `capture_rfc0016_device_contracts.py:2076` |
| `test_network_isolation_structure_contract` | `resolve(strict=True)`, `verify_test_network_isolation.py:371` |

`test_network_isolation_structure_contract` was the last one still filed as
undiagnosed. Its symptom read as something else entirely —

```
live supervisor, inventory, or producer build is unavailable:
[WinError 5] Access is denied: '…\AC\Temp\…\BetterPullReview.app\Contents
\Helpers\better_pull_review_designated_live_supervisor'
```

— and looked like a permissions problem with a fixture the test had just
created. The fixture is fine. `_live_supervisor_identity` calls
`supervisor.resolve(strict=True)` on it, and that is this card.

A false trail worth recording, so nobody follows it again: the fixture is
created and then `chmod(0o700)`'d, which looked like the same DACL defect
[[BPR-0350]] fixed. It is not. Measured inside the container, a file remains
readable after `chmod` at `0o700`, `0o600`, and `0o755` alike — CPython's
Windows `chmod` toggles the read-only attribute and writes no descriptor, unlike
its `mkdir`.

## Three code paths, one cause

That makes three independent places, in two languages, that cannot work
contained:

1. `validate_ancestor_chain` — opens every ancestor, and cannot.
2. `capture_rfc0016_device_contracts.py` — `resolve(strict=True)`.
3. `verify_test_network_isolation.py` — `resolve(strict=True)`, three calls.

Fixing them one at a time is not a repair, it is a pattern of concessions. That
is the argument for answering this card as a policy rather than per site, and it
strengthens option 4: file identity is available contained, and it is the one
answer that serves all three.

## Git cannot run contained either, which narrows the options — 2026-08-18

A fourth code path reaches this card, and it is not ours to change.

```
subprocess.CalledProcessError: Command '['git', 'init', '-q']' returned 128
fatal: unable to get current working directory: Permission denied
```

Measured inside the launcher, in a directory the process had just created and
can write to. `git` resolves its working directory at start-up, that resolution
walks the ancestor chain, and the chain is unreadable. Nothing about the
repository, the fixture, or the temporary root is wrong.

`operator_response_validation_contract` fails on this, and [[BPR-0379]] fixed
everything else that test was failing on.

### What it means for the options

**Option 4 cannot fix this.** Verifying by file identity instead of by name is
a change to *our* code, and `git` is a third-party binary that will keep calling
`getcwd()` however we express our own checks. Any test that shells out to `git`
inside the container fails regardless.

That leaves the two options this card started with, and sharpens the choice:

1. **Grant the container ACLs up the chain** — the only option that fixes every
   path at once, including third-party binaries. Its cost is unchanged and
   real: it means modifying the ACLs of `C:\`, `C:\Users`, and the invoking
   user's profile directory.
3. **Do not contain the tests that need this** — now a larger set than it
   looked, because it includes anything invoking `git`.

Option 4 remains worth doing for the product's own chain, on its own merits —
`validate_ancestor_chain` should probably verify identity rather than name
regardless of containers. But it is no longer a complete answer to this card,
and it should not be presented as one.

### The honest summary

Six tests wait on this decision. Three separate mechanisms are involved — our
product's retained chain, `resolve(strict=True)` in two scripts, and `getcwd()`
in `git` — and only option 1 addresses all three.

## Operator decision packet v2 — 2026-08-19

The 2026-08-18 packet is superseded. It was written before option 2 was
attempted, before `git init` was measured inside the container, and before the
test list was evidenced, and every one of those changed the answer. An owner
reading it would have been offered a dead option and pointed at the wrong one
first. This version replaces it; nothing below repeats a claim the card has since
falsified.

**Status: pending operator decision.** Not RFC closeout, not a live GitHub call,
not a credential, not release or remote-mutation authority.

### The bounded decision requested

Pick **option 1** or **option 3** for how the Windows AppContainer answers the
ancestor-read denial. Option 2 is struck. Option 4 is a separate yes/no that does
not settle this card on its own — see below.

### Why the choice is down to two

The container cannot read any path ancestor above its own `AC` directory, and
three independent mechanisms hit that, in two languages plus a third-party
binary:

| # | mechanism | can we change it? |
|---|---|---|
| 1 | `validate_ancestor_chain` opens every ancestor | yes, it is our C++ |
| 2 | `resolve(strict=True)` in `capture_rfc0016_device_contracts.py` and `verify_test_network_isolation.py` | yes, it is our Python |
| 3 | `git` resolves its working directory at start-up via `getcwd()` | **no** |

Mechanism 3 is what collapsed the option set. Any test that shells out to `git`
inside the container fails no matter how we express our own checks, so no
change to our code can be a complete answer.

### The options, as they now stand

| option | what it does | gives up | keeps |
|---|---|---|---|
| **1 — grant the container ACLs up the chain** | `icacls` grants in `cmake/WindowsTestIsolationSetup.cmake` extend to `C:\`, `C:\Users`, and the invoking user's profile directory | sandbox narrowness: the contained process gains traverse to the drive root, widening the boundary [[BPR-0347]] built | every mechanism fixed at once, including `git`; no product change; [[BPR-0053]]'s substitution criterion intact |
| ~~2 — anchor the chain at a readable root~~ | **struck.** Implemented and measured: the chain relaxed correctly and then failed one step later, because `GetFinalPathNameByHandleW` cannot resolve a handle's own name inside a container even when `CreateFileW` succeeded on it. Reverted rather than left in the tree | — | — |
| **3 — do not contain the tests that need this** | register the affected tests outside the AppContainer prefix | the default-deny boundary [[BPR-0235]] requires of *every* test, for exactly the tests whose subject is filesystem authority — and now for anything invoking `git`, which makes the exempt set larger than it looked | the sandbox and the security claim for every other test |

### Option 4 is a separate question, and it is not a substitute

**Verify the retained chain by file identity instead of by name.**
`GetFileInformationByHandle` returns the volume serial and file index inside a
container — measured working on the very handle whose name could not be
resolved — and a file identity is a stronger statement than a name string,
because a rename cannot forge it.

It is probably right on its own merits, container or no container. But it only
touches mechanism 1. It cannot help mechanism 3, so answering yes to option 4
does **not** answer this card. Treat it as a second, independent yes/no.

### What the verdict binds to

The option set as written in **this v2 section**, at commit `cbf6f9f`. The
superseded 2026-08-18 packet and its `c55f2d7` binding are void.

### What is at stake

**Seven tests**, each of which passes outside the launcher and fails inside it:

| test | where it stops contained | mechanism |
|---|---|---|
| `sqlite_epoch_catalog_behavior_contract` | `validate_ancestor_chain` | 1 |
| `sqlite_review_ledger_storage` | `validate_ancestor_chain` | 1 |
| `sqlite_review_mutation_recovery_storage` | `validate_ancestor_chain` | 1 |
| `sqlite_epoch_catalog_storage_contract` | `validate_ancestor_chain` | 1 |
| `rfc0016_device_capture_security` | `resolve(strict=True)` at `capture_rfc0016_device_contracts.py:2076` | 2 |
| `test_network_isolation_structure_contract` | `resolve(strict=True)` at `verify_test_network_isolation.py:371` | 2 |
| `operator_response_validation_contract` | `git init` → 128, `unable to get current working directory` | 3 |

The seventh arrived by re-filing rather than by measurement: [[BPR-0347]] and
[[BPR-0358]] both carried it as separately undiagnosed until the taxonomy
re-check showed `d348be9` had already recorded it as this exact cause.

### What to look at

- This card's measured table: the drive root is `win32=5` inside the container
  and opens outside it.
- `src/adapters/sqlite/src/catalog_database.cpp:2155` — `validate_ancestor_chain`.
- `cmake/WindowsTestIsolationSetup.cmake` — where option 1's `icacls` grants go.
- [[BPR-0053]]'s epoch-catalog substitution criterion, which is the claim under
  discussion.
- The option-2 probe output recorded in this card, which is the evidence that
  the middle path is closed.

### Explicit non-scope

No change to macOS or Linux behaviour. Does not touch [[BPR-0360]]'s descriptor
defect. Does not decide option 4. Does not close [[BPR-0347]], [[BPR-0358]], or
any RFC.

### What is not blocked by this

Nothing else in the Windows program. [[BPR-0363]], [[BPR-0361]], [[BPR-0365]],
[[BPR-0366]], [[BPR-0368]] through [[BPR-0375]], [[BPR-0377]] and [[BPR-0379]]
were all completed while this stood.

## Operator direction — defer to a Windows session — 2026-08-20

Recorded verbatim:

> answer windows questions on windows host, delay this

The v2 packet is not withdrawn and its option set is not reopened. What changed
is **who** should answer it and **when**: this is a question about an
AppContainer boundary on Windows, and the direction is that it be answered from
the Windows host rather than from a macOS session presenting a summary of it.

That is a better disposition than the one this card had, and worth stating why.
Options 1 and 3 differ in what they cost *in practice on that machine* — option 1
means modifying the ACLs of `C:\`, `C:\Users`, and the invoking user's profile
directory, and whether that is acceptable is much easier to judge sitting at the
host it applies to. Option 4's merit is also easier to see next to a running
suite.

`parked_on` stays `awaiting-product-decision`, which remains exactly true. The
card is not blocked on a Windows host in the `50-blocked/` sense — the *decision*
does not require one — but the owner has asked for it to be taken there, so it
waits.

The seven tests this decision is worth are unchanged, and nothing else in the
Windows program is blocked by it.

## Answered — option 1 — 2026-08-22

### Provenance, stated exactly

**The product owner did not weigh these options.** On 2026-08-22 they delegated
this decision — "spawn a fable subagent to answer the decision for me" — and
then, told that the result would be an agent recommendation rather than their
verdict, directed: "Write the honest provenance but move forward, dont wait for
further action from me."

So this is an **owner-delegated agent decision**, recorded unreviewed. It was
produced by an independent capability-critical analysis on the deep-max route,
which read this card and verified its claims against live source rather than
accepting its summary. The reasoning is recorded below so a future session can
weigh it rather than inherit it. It carries the authority the owner delegated
and no more: it does not become the owner's own judgement by being written down,
and the owner may overturn it on sight.

The integrator reproduced every checkable claim before recording it. The Windows
host measurements in this card are not re-verifiable from macOS and are accepted
as recorded.

### Decision: option 1

Extend the `icacls` grants so the container can open the ancestor chains —
**non-inheritable, this-folder-only, minimal mask, on every strict ancestor of
every granted scope**.

### Why

**Network containment and filesystem narrowness are separable, and only Windows
couples them.** That is the whole decision.

- [[BPR-0235]]'s required final state claims exactly one property:
  "Automated tests run with **network** denied by default at the process
  boundary." Nothing claims the container cannot read `C:\`.
- The Linux backend is a network namespace
  (`cmake/BprTestIsolation.cmake:281`, `linux-network-namespace`) and leaves the
  filesystem alone. The macOS backend leaves ancestor resolution working — the
  isolation runner itself calls `resolve(strict=True)` three times at
  `scripts/run_network_isolated_test.py:168-180` and the suite passes.
- So the ancestor denial is an **accidental side effect** of the only
  unprivileged network boundary Windows offers, not a property any card asserts.

Option 1 therefore spends an unclaimed accident. Option 3 spends the claimed
invariant — and not cheaply: `scripts/verify_test_network_isolation.py` exists to
verify that *every* configured test crosses the boundary, so option 3 requires
building a third registration class and teaching that verifier a
platform-conditional exempt list. New contract code whose only purpose is
punching holes in the invariant it verifies. It would also leave [[BPR-0235]]'s
Windows criterion permanently undischargeable as written.

Filesystem DACL grants confer no network capability: the container keeps zero
capability SIDs either way.

### Option 4: no

Verifying the retained chain by file identity **instead of** by name weakens the
defence. At first open there is no reference identity, so
`GetFileInformationByHandle` returns an equally valid identity for the wrong
directory — unforgeable but unanchored. The name check is the only thing binding
each retained handle to the *configured* path string, and it has already earned
its place: [[BPR-0368]]'s doubled-separator defect was caught by that check
failing loudly, where an identity comparison would have silently retained a
mis-built chain. This card's claim that identity "is a stronger statement than a
name string" inverts the property — identity is stronger only for
re-verification against an already-trusted reference, which this loop does not
have.

The genuinely stronger design is opening each component *relative to the
retained parent handle*, never resolving names at all. That is a redesign of the
open discipline, not a comparison swap, and after option 2's history it belongs
on its own card with target-native adversary measurement.

### What this costs, including a cost this card never stated

The stated cost: contained test processes gain names-and-attributes visibility of
`C:\`, `C:\Users`, the profile chain, and the repository's ancestors. The ACE
names only the `bprautomatedtest` SID, so no other principal gains anything.

**The unstated cost, and the strongest argument against this choice:** `C:\` and
`C:\Users` are not user-owned, so those grants need a **one-time elevated step**
— the exact cost class [[BPR-0347]] deliberately removed when it deleted its
elevated-prompt refusal. It does not change the answer, because that removal was
argued on "authority the suite never exercises", and this authority is
load-bearing for seven tests and for the boundary's universality. It is
once-per-machine, auditable, and reversible with `icacls /remove`. The
alternative permanently falsifies a standing security claim *and* grants those
seven tests full network and filesystem authority — strictly more total
authority than option 1 grants.

### Corrections to this card, verified against live source

| card said | live tree | checked |
|---|---|---|
| `validate_ancestor_chain` at `catalog_database.cpp:2155` | **2030**; 2155 is inside `reliable_local_filesystem` | corrected above |
| `open_state_directory` at 3862 | **3758** | — |
| `_live_supervisor_identity` calls the failing resolve | the live function is **`live_supervisor_failures`** (`verify_test_network_isolation.py:360`); the line numbers and symptom string do match | — |
| option 1 grants `C:\`, `C:\Users`, and the profile directory — three | **understated by this card's own evidence.** Its option-2 probe lists seven denied profile-chain ancestors, and its measured table shows `C:\code` denied, so the repository chain needs grants too or `REPOSITORY_ROOT.resolve(strict=True)` still fails | the grant set is "every strict ancestor of every granted scope and of the AC root" |
| the elevation asymmetry | **omitted from every packet version**, and material to the cost judgement | stated above |

`rfc0016_device_live_runner_security` appears at this card's line 90 as failing on
this cause and then vanishes from both the evidenced list and the v2 stake table
with no disposition. Either an intervening card fixed it or it is an unlisted
eighth mechanism-2 test. The Windows session should measure it; it does not
change the choice.

### Required implementation, and why it is not split

The decision is made; the work is not. Implementing it means measuring the
minimal mask on the host, writing the grants into
`cmake/WindowsTestIsolationSetup.cmake`, and proving the seven tests pass
contained. **Never `(OI)(CI)` on any ancestor** — an inheritable grant on `C:\`
or the profile root would hand the container the drive or `Documents` and
`.ssh`. The configure must fail closed with a message naming the exact elevated
command.

None of that is portable. A CMake edit written from macOS could not be measured,
and this card has already recorded what that produces: [[BPR-0360]] wrote an
implementation and reverted it rather than land it unverifiable. So the card
moves whole to `21-ready-win/` rather than splitting, which is the condition the
board's own splitting rule carries.

### What would reverse this

- A Windows session grants the complete chains non-inheritably and measures that
  `GetFinalPathNameByHandleW`, `resolve(strict=True)`, and `git init` **still**
  fail — meaning the denial is lowbox-intrinsic rather than ACL-caused. Option 1
  then joins option 2 as measured-dead and option 3 is taken explicitly, with its
  exempt list pinned by its own contract and [[BPR-0235]]'s Windows criterion
  rewritten honestly.
- The owner refuses the one-time elevated step — a legitimate call this card
  never surfaced, and the same fallback applies.
- Any claim or test appears that binds AppContainer *filesystem* narrowness as a
  required property. None exists today.

## Measured on the Windows host — 2026-08-22

Run from the `windows-lane` worktree at `5ce0bc1`, with the launcher built
from the checked-in source and the host online. Two probe scripts were kept
under `build/core-only/probe/`, because the first attempt to run one from
outside the repository and build tree failed inside the container with
`[Errno 13]` on the script itself — the grant set is exactly the two scopes
`cmake/WindowsTestIsolationSetup.cmake` names, and nothing else.

### The ancestor opens are the file system's; the name failure is not

Opening each ancestor with `CreateFileW` (`FILE_FLAG_BACKUP_SEMANTICS`), then
asking the handle that opened for its final name under each `VOLUME_NAME_*`
flag:

| path | outside | inside the container |
|---|---|---|
| `C:\`, `C:\code`, `C:\code\git` | opened | **denied, win32=5** |
| `C:\code\git\bpr-windows` (granted) | opened; every name form ok | opened; **`DOS` err 5, `GUID` err 5, `NT` ok, `NONE` ok** |
| `…\Packages\bprautomatedtest` and every ancestor above it | opened | **denied, win32=5** |
| `…\bprautomatedtest\AC`, `AC\Temp`, a directory created under it | opened; every name form ok | opened; **`DOS` err 5, `GUID` err 5, `NT` ok, `NONE` ok** |

`NT` ok is the finding. The kernel returned the complete normalized name —
`\Device\HarddiskVolume4\code\git\bpr-windows`, every denied ancestor included
— for a handle inside the container. So the option-2 section above was wrong
about the mechanism: resolving a handle's name does **not** walk the parent
chain and fail there. What fails is the translation of `\Device\HarddiskVolume4`
into `C:`, which is what the `DOS` and `GUID` forms add and the `NT` form does
not.

Every primitive that translation depends on is denied inside, and succeeds
outside:

| call | outside | inside |
|---|---|---|
| `QueryDosDeviceW("C:")` | `\Device\HarddiskVolume4` | err 5 |
| `GetVolumeNameForVolumeMountPointW("C:\")` | `\\?\Volume{…}\` | err 5 |
| open `\\.\MountPointManager` | opened | err 5 |
| open `\\.\C:` (the volume device) | opened | err 5 |
| `GetDriveTypeW("C:\")` | 3, `DRIVE_FIXED` | 1, `DRIVE_NO_ROOT_DIR` |
| `GetVolumeInformationByHandleW` on the repository handle | NTFS, serial read | NTFS, same serial |
| `resolve(strict=True)` on the repository and on the created directory | ok | `PermissionError`, winerror 5 |
| `git init -q` in the created directory | 0 | 128, `unable to get current working directory: Permission denied` |

### Why no directory grant can reach it

Those are not directories. Read outside the container with `READ_CONTROL`,
the objects' own DACLs, in SDDL:

```
\GLOBAL??\C:                O:BAD:(A;;CCRC;;;WD)(A;;CCSDRCWDWO;;;SY)(A;;CCSDRCWDWO;;;BA)(A;;CCRC;;;RC)
\GLOBAL??\MountPointManager same
\Device                     O:BAD:(A;;CCDCRC;;;WD)(A;;CCDCLCSWSDRCWDWO;;;SY)(A;;CCDCLCSWSDRCWDWO;;;BA)(A;;CCDCRC;;;RC)
\\.\MountPointManager       O:BAD:(A;;FX;;;WD)(A;;FA;;;SY)(A;;FA;;;BA)(A;;FX;;;RC)
\\.\C:                      same
```

Everyone, SYSTEM, Administrators, and `RESTRICTED`. No `ALL APPLICATION
PACKAGES`, no package SID. A lowbox token passes its second access check only
through one of those two, so every one of these objects refuses it by
construction. They are created by the object manager and the mount manager at
boot; `icacls` has no path to them, and neither does the elevated step the
decision record priced.

The product asks for exactly the form that fails. `final_path_of`
(`src/adapters/sqlite/src/catalog_database.cpp:1564-1584`) calls
`GetFinalPathNameByHandleW` with `FILE_NAME_NORMALIZED | VOLUME_NAME_DOS`, and
`path_matches_handle` runs it for every retained ancestor and the leaf. CPython's
`realpath` and Git's `getcwd` ask for the `DOS` form too. So with every
directory in every chain granted — `C:\` and `C:\Users` included — mechanism
1's name check, mechanism 2, and mechanism 3 still fail at the same call.

### Option 1 is measured-dead

The reversal criterion written into the decision record is met in substance
and stated exactly where it is not met to the letter. `C:\` and `C:\Users` were
**not** granted, because that needs elevation and the result cannot depend on
it: the handles whose `DOS` name fails are handles whose open *succeeded*, the
`NT` name of those same handles resolves completely, and the primitives that
fail are symbolic links and device objects rather than directories. The
directory half of option 1 would have fixed only the ancestor *opens* — the
`C:\code`, `C:\code\git`, and profile-chain denials above are ordinary file
ACLs — and that half buys nothing while the name check behind it cannot pass.

One more attribution closes with this. The line `Failed to find real location
of <image>` that [[BPR-0347]] records on every test, and guessed was the
launcher seeing the ancestor denial, is CPython: `python314.dll` carries the
string and `windows_network_sandbox.cpp` does not. It is `getpath` running
`realpath` on its own executable inside the container and meeting this exact
failure. It is harmless and it is the same defect.

### What follows

The decision record's fallback applies as written: **option 3 is taken
explicitly**, with its exempt list pinned by its own contract and
[[BPR-0235]]'s Windows criterion rewritten honestly. The exempt set is pinned to
what a fresh contained run of this worktree measures, not to the stake table
above; the implementation and its evidence follow below.

Two consequences for neighbouring cards are recorded rather than left to be
rediscovered:

- [[BPR-0360]] no longer becomes observable through the four SQLite tests,
  because under option 3 they run uncontained and the descriptor defect only
  exists inside a container. That card needs its own contained probe of the
  product's descriptor builder and validator.
- [[BPR-0053]]'s ancestor-substitution criterion is unaffected: the retained
  chain is exercised uncontained on this host exactly as it is on macOS and
  Linux. What the Windows suite cannot do is exercise it *inside* an
  AppContainer, and that is now a statement about the AppContainer rather than
  about the product.

## Option 3, taken — 2026-08-22

### The fresh run first

The card's stake table was not trusted; a fresh contained run of the whole
`core-only` graph in this worktree was taken before anything was pinned. At
`5ce0bc1`, with [[BPR-0421]] making the graph compile, it reads **76 of 88**,
twelve failing. Every one of the twelve was then run again *outside* the
launcher, exactly as registered, as the negative control. That sorted them
into four groups, and only the first is this card's:

| group | tests | evidence |
|---|---|---|
| **container-intrinsic** — passes uncontained, fails contained at a named lowbox denial | the four SQLite storage contracts, `rfc0016_device_capture_security`, `test_network_isolation_structure_contract`, `operator_response_validation_contract`, `coding_standard_format_gate_contract`, `dependency_modernization_contract` | the measurements in the previous section, plus two more mechanisms below |
| **portable defects the run exposed** — fails uncontained for a reason that has nothing to do with Windows | `contained_temporary_usage_contract` ([[BPR-0422]]), the banner half of `coding_standard_format_gate_contract` ([[BPR-0423]]) | fixed on this branch, verified uncontained |
| **Windows defects the run exposed** — fails uncontained on this host only | the retention assertion in `sqlite_epoch_catalog_storage_contract` ([[BPR-0425]]), five POSIX-shaped assertions in `dependency_modernization_contract` ([[BPR-0426]]) | carded, not folded in here |
| **not the boundary at all** | `documentation_structure` (this session's own README mid-edit; passes on rerun), `application_github_capability_cache` (Smart App Control refuses the image, contained or not: [[BPR-0424]]) | measured directly |

Two of the nine container-intrinsic tests fail for mechanisms the v2 packet
did not know about, and they are lowbox-intrinsic in exactly the way the first
three are:

- **`cmd.exe` will not run a batch file inside the container.** A one-line
  `.bat` that works outside returns `Access is denied.` inside. The format
  gate's stand-in `clang-format` is a `.bat` on Windows, so the gate sees no
  tool at all; CPython's own `Failed to find real location` warning also
  lands on the stderr the gate compares.
- **`ninja` cannot create its named pipe inside the container** —
  `ninja: fatal: CreateNamedPipe: Access is denied` — so every CMake fixture
  configure in `dependency_modernization_contract` dies inside the compiler
  check, before the rejection the test asserts can be reached. A lowbox may
  create named objects only in its own `AppContainerNamedObjects` namespace.

Neither is a directory, and neither is reachable by a grant, which is the
same reason option 1 died.

The two epoch-catalog tests do not *fail* contained; they hang until CTest
kills them, while their two siblings on the same `open_state_directory` path
fail fast. That is a separate fact about the product inside a container and
is [[BPR-0427]]'s, not this card's.

`rfc0016_device_live_runner_security` — the unlisted eighth the decision
record asked this session to measure — **passes contained**. It is not in the
set.

### What was built

`bpr_add_network_isolated_test` gained one option, `WINDOWS_UNCONTAINED`.
Under the `windows-appcontainer` backend, and only there, a test carrying it
is registered without the launcher prefix and labelled
`bpr_windows_uncontained` instead of `bpr_network_isolated`; its environment
scrubs are unchanged. On macOS and Linux the keyword is inert, so the whole
suite stays contained where containment does not cost name resolution. The
raw `add_test` stays inside `cmake/BprTestIsolation.cmake`, so the
single-owner source contract is untouched.

`scripts/verify_test_network_isolation.py` pins the set, by name and by
measured cause:

```
WINDOWS_UNCONTAINED_TESTS = {
    sqlite_epoch_catalog_storage_contract      dos-volume-name
    sqlite_epoch_catalog_behavior_contract     dos-volume-name
    sqlite_review_mutation_recovery_storage    dos-volume-name
    sqlite_review_ledger_storage               dos-volume-name
    rfc0016_device_capture_security            dos-volume-name
    test_network_isolation_structure_contract  dos-volume-name
    operator_response_validation_contract      dos-volume-name
    coding_standard_format_gate_contract       batch-stub
    dependency_modernization_contract          ninja-named-pipe
}
```

and fails the manifest gate in every direction it could drift: a member found
contained ("retire it from the pinned set"), a member without its label or
still carrying `bpr_network_isolated`, any non-member carrying the label, any
missing prefix on a non-member exactly as before, the label on any other
backend at all, and a source count of `WINDOWS_UNCONTAINED` keywords inside
`bpr_add_network_isolated_test(...)` calls that differs from the set's size.
Six unit cases in `tests/scripts/verify_test_network_isolation_tests.py`
cover each direction; the count case was itself corrected once, when the
first live gate run counted the wrapper's own option string and comment and
reported eleven.

### What it gives up, and what it does not

Nine tests on Windows run with no network boundary. They are the four SQLite
storage contracts, three Python contracts, one gate contract and one
dependency contract; none performs network I/O, and the manifest gate
continues to prove that *every other* configured test crosses the boundary.
[[BPR-0235]]'s Windows criterion is rewritten on that card in those words.

It does not give up the sandbox for anything else, does not touch macOS or
Linux, and does not change a product line: `validate_ancestor_chain`,
`resolve(strict=True)`, and Git are exactly as they were. The documentation
in `docs/development/build-and-test.md` and
`docs/architecture/review-round-trip.md` no longer says Windows refuses every
test, which had been stale since [[BPR-0347]].

### Verification

| claim | command | proves it | result |
|---|---|---|---|
| the nine run uncontained and nothing else does | `ctest --show-only=json-v1`, labels read back | nine tests carry `bpr_windows_uncontained` and their first argv is the test program, not the launcher; 88 registered | pass |
| each of the nine passes as registered, or fails for a carded non-container cause | `ctest -R` over the nine | seven pass; `sqlite_epoch_catalog_storage_contract` fails at [[BPR-0425]]'s assertion and `dependency_modernization_contract` at [[BPR-0426]]'s five, both reproduced uncontained before this change | as stated |
| the manifest gate accepts exactly this set | `ctest -R '^test_network_isolation_manifest$'` (contained) | `TEST_NETWORK_ISOLATION_OK` | pass, `configured=88:registrations=…:backend=windows-appcontainer` |
| the verifier refuses every drift direction | `py -3 tests/scripts/verify_test_network_isolation_tests.py` | 31 cases including the six new ones | OK |
| the two portable fixes hold | `py -3 tests/scripts/contained_temporary_usage_tests.py`, `py -3 tests/scripts/coding_standard_format_gate_tests.py`, `py -3 tests/scripts/record_rfc0017_anonymous_contracts_tests.py` | 6/6, 9/9 (1 skip), 36/36 | OK |
| nothing else regressed | `ctest --preset core-only` after the change | **86 of 88**, up from 76; the two failures are [[BPR-0425]] and [[BPR-0426]], both uncontained-registered and both reproduced uncontained before this change; `documentation_structure` and `application_github_capability_cache` pass | pass |

The full contained matrix after the change, at this worktree, is 86 of 88 in
435 s. Before it, 76 of 88. Ten tests moved, and each of the ten is either
one of the nine now registered uncontained or a test whose earlier failure
this session caused and corrected in its own tree.

### Phase

Decision made and implemented; card closes. Nothing here is reversible by
undoing a grant, because no grant was made: the machine's ACLs are exactly
as they were when the session opened.

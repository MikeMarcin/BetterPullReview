---
id: BPR-0362
title: The container cannot read the ancestors every path check walks
status: Parked
rfc: RFC-BPR-0008
depends_on: []
parked_on: awaiting-product-decision
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

`validate_ancestor_chain` in `src/adapters/sqlite/src/catalog_database.cpp:2155`
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

## Operator decision packet — 2026-08-18

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

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

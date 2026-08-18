---
id: BPR-0362
title: Strict path resolution cannot cross the container's unreadable ancestors
status: Ready
rfc: RFC-BPR-0008
depends_on: []
---

# Strict path resolution cannot cross the container's unreadable ancestors

A contained process can create a temporary directory, write into it, and list
it, and still cannot call `Path.resolve(strict=True)` on it. `realpath` resolves
every component of the path, and the container cannot read the components above
its own `AC` directory — including `C:\`.

Measured inside the launcher, on a directory this process had just created and
written to:

```
created  = …\Packages\bprautomatedtest\AC\Temp\probeecc6d4d28435a4cc
write    = ok      listdir = ['f.txt']
resolve(strict=True)   -> DENIED PermissionError winerror=5
resolve(strict=False)  -> OK
os.stat                -> OK
```

Walking the ancestors shows exactly where it stops:

| path | `os.stat` | `realpath(strict=True)` |
|---|---|---|
| `…\bprautomatedtest\AC\Temp` | ok | denied (5) |
| `…\bprautomatedtest\AC` | ok | denied (5) |
| `…\Packages\bprautomatedtest` | denied (5) | denied (5) |
| `C:\Users\mikem\AppData\Local` | denied (5) | denied (5) |
| `C:\Users` | denied (5) | denied (5) |
| `C:\` | denied (5) | denied (5) |

The container can stat its own `AC` and `AC\Temp` and nothing above them. So the
denial is not about the leaf at all, and no permission change on the temporary
directory can fix it. This is a property of the boundary, and it is the correct
property — a contained process should not be able to enumerate `C:\Users`.

## Why it matters

`strict=True` is not decoration. It is used to assert that a path exists and to
collapse symlinks and junctions before a path is trusted — which is a security
check in several places here. Code that needs that guarantee cannot simply drop
to `strict=False`, because `strict=False` returns a path that may not exist and
does not prove the ancestors were not substituted.

So each site has to be read and answered on its own terms, not swept with a
global replace. The two questions are: what was `strict=True` proving, and can
that thing be proven some other way inside the container.

## Where it is failing now

`scripts/capture_rfc0016_device_contracts.py:2076` — `_load_attestation_review`
calls `REPOSITORY_ROOT.resolve(strict=True)`. Two tests fail on it:

- `rfc0016_device_capture_security`
- `rfc0016_device_live_runner_security`

Both produce a long run of `PermissionError: [WinError 5]` naming the temporary
fixture root, which reads like the [[BPR-0350]] temporary-directory defect and is
not it. [[BPR-0350]]'s fix is what let these tests get far enough to hit this.

## Required final state

- Every `resolve(strict=True)` reached by a contained test either succeeds inside
  the container or is replaced by a check that proves the same property there.
- Where the same property genuinely cannot be proven inside the container, the
  narrower truth is recorded rather than the check being weakened silently.
- `rfc0016_device_capture_security` and `rfc0016_device_live_runner_security`
  pass, and macOS and Linux behaviour is unchanged.
- A test fails without the fix.

## Not in scope

Whether the launcher should grant the container traverse access up the ancestor
chain. That would make `realpath` work and would also widen the boundary
[[BPR-0347]] built, so it is a deliberate decision about the sandbox rather than
a repair, and it belongs with the launcher rather than here.

## Verification

```sh
ctest --preset core-only --output-on-failure \
  -R '^(rfc0016_device_capture_security|rfc0016_device_live_runner_security)$'
```

Run the same selectors outside the launcher as the negative control.

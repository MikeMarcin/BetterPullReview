---
schema: "implement-rfc.card.v1"
id: "BPR-0342"
title: "Stop the Linux test isolation presenting every test as root"
status: "Done"
rfcs: ["RFC-BPR-0008"]
depends_on: []
completed: "2026-08-17"
---

# Stop the Linux test isolation presenting every test as root

The Linux automated-test boundary calls
`unshare(CLONE_NEWUSER | CLONE_NEWNET)` and maps the invoking user to uid 0:

```
$ better_pull_review_network_sandbox /bin/cat /proc/self/uid_map
         0       1000          1
```

Every test on Linux therefore runs as root inside the namespace. That is not
what the boundary was for, and it silently changes the answer to any question
about POSIX permissions, because root is exempt from the checks the product
relies on.

`gh_cli_transport` is the first casualty. It builds a file whose execute bits
belong only to group and other, and requires the transport to reject it,
because `access(X_OK)` must fail for the owner. Measured on the same file at
the same moment:

```
outside sandbox: uid=1000  access(X_OK)=False
inside  sandbox: uid=0     access(X_OK)=True
```

Root succeeds when *any* execute bit is set, so the transport accepts an
executable it must refuse, and the test fails. The test is right and the
product is right; the harness changed the question. Running the same binary
outside the sandbox passes.

## Why this matters beyond one test

Nothing announces the privilege change, so any present or future test that
asserts a permission, an ownership check, or a refusal to use an
attacker-writable path will quietly stop testing what it claims. The
credential-vault, cache-permission, and approval-artifact ownership rules are
all of that kind. A boundary meant to deny the network should not also grant
privilege.

## Result

The launcher now writes `<id> <id> 1` to `uid_map` and `gid_map` instead of
`0 <id> 1`, and the two `setresuid`/`setresgid` calls to 0 are gone. The
topology is unchanged: the user namespace is still created, and it is still
what lets an unprivileged caller create the network namespace.

Capabilities come from creating the namespace, not from the mapped id, so
loopback still comes up. Removing the `setresuid` call matters for the same
reason in reverse: a transition away from 0 is exactly what would have dropped
those capabilities before the `SIOCSIFFLAGS` call.

```
uid=1000(mmarcin) gid=1000(mmarcin)
/proc/self/uid_map:  1000  1000  1
/proc/net/dev:       lo
```

`access(X_OK)` on the mode-0611 fixture now returns False inside the sandbox
as it does outside, and `gh_cli_transport` passes inside with no change to the
test. All eight other isolation tests, the live-mutation pair, and the
24-test verifier suite still pass.

`test_network_isolation_identity` is the regression guard. It reads
`/proc/self/uid_map` and `/proc/self/gid_map` from inside and requires every
range to name the same id on both sides. The map is the proof, and it is
readable from inside, so nothing has to be passed in.

The guard was checked against a deliberately reverted launcher rather than
only against the repaired one:

```
NETWORK_ISOLATION_IDENTITY_REMAPPED:uid_map:0 1000 1   exit=1
```

Equal ends hold whoever runs the suite, a genuine root included, so the guard
does not quietly double as an assertion about who that is. It is registered on
Linux only, because `uid_map` is what it reads and `sandbox-exec` never
changed the identity in the first place.

Recording the effective uid in the isolation verifier was listed above and was
not done. The verifier inspects the configured manifest, which is static text;
it cannot observe a runtime identity, and the probe proves the property
directly at the moment it matters.

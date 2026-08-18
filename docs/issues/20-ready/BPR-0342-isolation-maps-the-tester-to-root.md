---
id: BPR-0342
title: Stop the Linux test isolation presenting every test as root
status: Ready
rfc: RFC-BPR-0008
depends_on: []
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

## Required final state

- The Linux isolation launcher maps the invoking user to their own uid rather
  than to 0, keeping the network namespace and dropping the privilege change.
  A user namespace is still needed to create a network namespace unprivileged,
  so the uid map is what changes, not the topology.
- `gh_cli_transport` passes inside the sandbox with no change to the test.
- A test proves the mapped uid is not 0, so a future launcher change cannot
  reintroduce this without failing.
- The isolation verifier records the effective uid alongside the backend name.

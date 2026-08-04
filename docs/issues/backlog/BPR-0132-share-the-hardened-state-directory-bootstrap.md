---
id: BPR-0132
title: Share the hardened state-directory bootstrap between stores
status: Backlog
rfc: RFC-BPR-0007
depends_on: [BPR-0128]
---

# Share the hardened state-directory bootstrap between stores

The epoch catalog creates `state/v1` through a platform-specific bootstrap that
sets an explicit owner-only ACL on Windows, verifies ownership and mode on
POSIX, refuses symlinked or replaced directories, and cleans up orphaned
bootstrap artefacts.

The review ledger store added in BPR-0128 does not use it. It calls
`create_directories` and narrows the mode with `chmod`, because it must not
depend on the catalog having been composed first — an adapter that only works
when another adapter ran before it is a latent ordering bug.

The result is two directory-creation policies for one directory, and the weaker
one wins whenever the ledger is composed first. That is the same class of
problem the connection-policy extraction fixed, and it deserves the same
treatment.

## Required direct evidence

- One bootstrap creates `state/v1`, and both stores use it; no second
  `create_directories` for that directory remains.
- The hardening is asserted after the ledger composes it first, not only after
  the catalog does — the ordering that currently produces the weaker directory.
- A symlinked or wrong-mode directory is refused by whichever store opens
  first.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

---
id: BPR-0175
title: "Host: process launch, environment allowlist, and the credential boundary"
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Host: process launch, environment allowlist, and the credential boundary

The host resolves the executable, prepares the environment, launches the
process, and carries the context over its input channel.

## The two rules this card exists to hold

**No credential crosses.** The child's environment is an allowlist, not an
inheritance: a variable is present because the profile named it, never because
this process happened to have it.

**Nothing sensitive travels in argv.** Command lines are world-readable — `ps`
on POSIX, the process list on Windows — so a supplement in argv would be
visible to every other user of a shared machine.

## Required direct evidence

- A fixture process records its own argv and environment, and the test reads
  both in full rather than searching them for one known secret.
- The parent's own credential-bearing variables are shown to be present in the
  parent and absent in the child.
- The supplement arrives over the input channel, and appears nowhere in argv.
- Cancellation terminates the child and publishes nothing.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

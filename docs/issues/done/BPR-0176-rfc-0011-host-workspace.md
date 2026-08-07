---
id: BPR-0176
title: "Host: workspace preparation, pinned and dirty"
status: Done
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Host: workspace preparation, pinned and dirty

Two modes. A disposable workspace pinned to the review head, isolated from the
active worktree; and an existing checkout, chosen deliberately, where the
reviewer's uncommitted work is visible to the agent.

## What this slice delivered

`application::prepare_agent_workspace`, and the `checkout_cleanliness` and
`workspace_refusal` vocabularies.

### A profile cannot opt itself into the reviewer's uncommitted work

The policy says the mode is *available*; a separate argument says it is wanted
**now**. A profile whose `Workspace` is `existing_checkout` is refused without
that choice, so the dangerous mode cannot be reached by configuration alone —
only by someone deciding it for this invocation.

### The pin comes from the session, never from the checkout

A disposable workspace is pinned to `Binding.HeadOid`. It is materialised from
an immutable revision, which is what makes it unaffected by anything the
reviewer does to the active worktree afterwards. The test prepares the same
workspace twice with the worktree dirtied in between and asserts the pin did
not move.

### Unknown is not clean

`checkout_cleanliness` has three states because "not asked" is not "clean". An
agent reasoning about a reviewer's uncommitted work should be *known* to be
doing so, and a host that could not determine the state must say so rather
than pick the reassuring answer. The test drives all three and asserts each
travels unchanged.

## Non-vacuity

| Break | Failure |
|---|---|
| Reach existing-checkout mode without a choice | `a profile configured for the reviewer's checkout cannot opt itself into their uncommitted work` |
| Report `unknown` as `clean` | `the observed state travels unchanged, including unknown` |

The first break's first attempt did not compile — `-Wunused-parameter` on the
argument I had just stopped reading — and `ctest` reported the previous
binary's pass. Caught by checking the error count rather than the exit status,
which is the fourth time this session that has been the thing that caught it.

## What this card does NOT deliver

- **No filesystem work.** This decides *what* workspace applies and where; the
  Git adapter materialises one. `Path` is supplied rather than created.
- **No cleanup.** A disposable workspace's lifetime is the host's.
- **No gesture.** Nothing yet asks a reviewer to choose existing-checkout mode;
  BPR-0177 owns the explicit command.

## Required direct evidence

- A disposable workspace matches the session's head exactly and is unaffected
  by changes made to the active worktree afterwards.
- Existing-checkout mode is never the default and never reached without an
  explicit choice.
- The dirty state of an existing checkout is reported rather than assumed
  clean — an agent reasoning about uncommitted work should be known to be
  doing so.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.

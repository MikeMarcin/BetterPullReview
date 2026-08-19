---
id: BPR-0384
title: Carry the retained git text past the catalog boundary
status: Done
rfc: RFC-BPR-0003
depends_on: []
completed: 2026-08-18
---

# Carry the retained git text past the catalog boundary

[[BPR-0348]] made a failing git subprocess retain its standard error and carry
it to the operator. It arrives on the agent-workspace path only.

Ten sites in `src/application/src/epoch_application_host.cpp` call
`cache_failure(… .Failure)` and drop the diagnostic, because `core::epoch_failure`
is a core enum and the functional core may not grow a `std::string`. So an epoch
or maintenance failure still reaches an operator as a bare code, which is the
condition BPR-0348 exists to remove — just on a different path.

## Why it was left

BPR-0348's motivating case is BPR-0341, whose failure ran through the agent
workspace, and that path now carries text end to end. Widening the change to
the core's own failure vocabulary is a different question with a different
owner, and folding it in would have meant editing `src/core/` inside a slice
scoped to adapters and the application host.

## What has to be decided first

The core cannot hold owning text — that is the architecture, not an oversight.
So this is not a matter of adding a field. The options are at least:

- a bounded non-allocating carrier in the core, as `git_process_error` already
  uses at the git port boundary;
- keeping the core enum and carrying the text beside it in the host, so the
  core stays text-free and the host does the joining;
- accepting that the epoch path reports codes, and making those codes finer
  instead.

Pick deliberately. The third is a real answer, not a concession.

## Required final state

- An epoch or maintenance failure whose first cause was a git subprocess
  reaches the operator with what git said, or the decision not to carry it is
  recorded here with its reason.
- Whatever is chosen does not put an owning container in `src/core/`.
- A test proves the chosen behaviour, and fails if the text stops again.

## Result — 2026-08-18

The decision is the third option, and it is a decision rather than a shortfall:
**the epoch path reports codes, and the code now says which layer failed.**

`git_cache_failure::process_failure` — the code a failing Git subprocess
produces, via [[BPR-0348]]'s chain — was mapped to
`core::epoch_failure::unavailable`, which it shared with a cancellation, an
invalid path, an unavailable lock, and a spool failure. Five different things
to go and look at, one code. That was the real loss on this path, and it does
not need text to fix.

It now maps to a distinct `core::epoch_failure::process_failure`, rendered by
the comparison control as "A Git command failed while reading the review
history."

### Why not carry the text

The card left three options open and asked for a deliberate choice.

Carrying the message would mean threading an owning payload through
`epoch_catalog_load_failed_event`, `epoch_revision_prepare_failed_event` and
`epoch_catalog_commit_failed_event` — core records that hold compact typed
state and are compared by value. The card's own constraint says not to put an
owning container in `src/core/`, and there is no host-side diagnostic surface
to put it on instead; inventing one that only tests consume would be an
evidence wrapper, not a repair.

There is also a smaller reason worth stating. What Git writes is Git's wording,
not this product's, and the place it belongs is the log and the failure the
adapter returns — where BPR-0348 already put it. Quoting a subprocess to a
reviewer in a comparison control is a different decision from making a
diagnostic honest.

So the text stays in the adapter that produced it, and the code says which
layer to look at. An operator who sees "A Git command failed" knows to read the
adapter's retained standard error; one who saw "unavailable" did not know
whether to look at Git, a lock, a path, or their own cancellation.

### Evidence

`a_failed_git_subprocess_names_itself_rather_than_unavailable` injects
`git_cache_failure::process_failure` on the resolve path and requires the
presentation diagnostic to be `process_failure` and **not** `unavailable` —
the code it used to collapse into. Its second half injects
`lock_unavailable` and requires the opposite, so the new code means what it
says rather than being what this path always reports.

Mutation-checked by collapsing the mapping back: the test fails with "a failing
Git subprocess reaches presentation as a process failure rather than as a bare
unavailability".

Seven selectors pass, including the core epoch policy, the epoch host, both
epoch contracts, maintenance integration, the Qt comparison scale, and the
architecture boundaries. Format and `DOCUMENTATION_OK` pass.

### One thing this slice had to fix that it did not cause

Adding the enumerator broke the build with an unhandled-switch error in the Qt
comparison control — which is the compiler doing exactly what a `-Werror`
switch is for, and the reason the new code reaches a reviewer with words rather
than silently falling through to a default.

The original front matter depended on this line's BPR-0348 slice; the
reconciliation kept the other line's BPR-0348, which parked on a policy
decision this card never needed. The dependency is dropped as historical:
this card's work completed and was verified without it.

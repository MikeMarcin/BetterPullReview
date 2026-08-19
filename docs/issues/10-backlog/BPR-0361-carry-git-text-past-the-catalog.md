---
id: BPR-0361
title: Carry the retained git text past the catalog boundary
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0348]
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

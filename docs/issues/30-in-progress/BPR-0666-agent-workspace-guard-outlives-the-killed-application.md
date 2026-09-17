---
schema: "implement-rfc.card.v1"
id: "BPR-0666"
title: "Agent workspace guard outlives the killed application"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
---

# Agent workspace guard outlives the killed application

`agent_parent_death` fails intermittently on macOS: two of three runs at
9ec5eba3 failed with "finished supervisor retained its workspace guard",
the third passed. The course kills the application, waits for the agent and
its descendant to stop, and then requires the workspace guard to be
available. The guard is still held when the check runs.

Either the supervisor releases the guard after the processes it supervises
have exited, in which case the product holds a lock nobody owns for an
unbounded moment, or the release is prompt and the check simply has no wait
where every other condition in the course has one. The course polls for the
processes to stop, so the distinction is visible: instrument the release
and see whether it lands within the same tick.

The failure predates RFC-BPR-0028 and belongs to no card in that pack; it
surfaced in the full matrix run at the pack's closeout head.

## Scope

- Decide which of the two it is before changing anything, by observing the
  release against the process exits.
- If the guard is released late, release it where the supervisor learns the
  processes are gone; if the check races a prompt release, give it the same
  bounded wait the surrounding conditions have and say what it waits for.
- Owned paths: the Unix agent supervisor and its workspace guard,
  `tests/scripts/agent_parent_death_tests.py`.

## Also in this file

`src/adapters/git/src/installed_git_process.cpp` carries eleven strict-tidy
errors that predate this card: one signed operand in a bitwise expression at
the guard's own `fstat` check, three more of the same, and the rest naming
style on parameters and a type alias. Nothing runs strict tidy over that
translation unit today, so they accumulated. Clear them with the guard fix,
since both are in the same file.

## Acceptance

- `agent_parent_death` passes twenty consecutive runs on macOS, and the
  reason it was failing is named in the card's notes.

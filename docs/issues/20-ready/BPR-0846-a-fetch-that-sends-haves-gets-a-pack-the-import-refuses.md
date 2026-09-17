---
schema: "implement-rfc.card.v1"
id: "BPR-0846"
title: "A fetch that sends haves gets a pack the cache import refuses"
status: "Ready"
rfcs: ["RFC-BPR-0029"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-linux task-20260917-lnx-epoch-and-startup new defect 1 (BPR-0839 investigation)","found_at":"4762d716b2048fc74eadc85d3425528696daa625","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0846: A fetch that sends haves gets a pack the cache import refuses

## Suspected defect (not yet observed against a live server)

The Git cache imports a fetched pack with
`git index-pack --stdin --strict --check-self-contained-and-connected`
(`src/adapters/git/src/installed_git_process.cpp` ~3855-3866). Its process
runner accepts exit 1 only when a request sets `AllowExitOne` (~2376-2384),
and the import does not. `--check-self-contained-and-connected` exits 1 when
the pack is not self-contained. With Git 2.53, `bpr-linux` confirmed in a
local experiment that the pack is written and the exit is 1.

A server that honours a `have` omits the objects reachable from it. The pack
is then complete for the repository but not self-contained. According to
`bpr-linux`, once an epoch exists the live path's fetch for a new head
already sends haves. If so, an ordinary pull request update against a server
that trims with haves fails with `process_failure`, and the new head never
loads. No fixture honours a have: the public-path fixture transport matches
only `want` lines and serves recorded packs. That would explain why no
course sees this.

Evidence so far is code reading and a local `index-pack` experiment
([[BPR-0839]], "Why offering haves is not a local change either").

## Scope

- **Reproduce offline first.** A local `git upload-pack`, or a `file://`
  or `git daemon` remote, honours haves. Drive the real live-path fetch and
  import for a second head whose ancestors are resident, and record the
  outcome. No live GitHub or GHES access.
- If it reproduces, fix the import rule so a complete but not self-contained
  pack is accepted only when the result is verifiably connected (for example,
  accept exit 1 and rely on the existing connectivity verification). Keep the
  refusal of genuinely broken or unconnected packs. This is the Git cache
  import contract: give it a critical review.
- Add a fixture or contract course that honours haves, so the path stays
  covered.

## Acceptance

- The live-path update of a pull request whose earlier head is resident loads
  over a transport that honours haves, in a composed course on macOS, and on
  Linux and Windows through the stations.
- A pack with a missing object, or one that is not connected to resident
  history, is still refused, and a course shows it.
- If it does not reproduce, this card records why, with the experiment.

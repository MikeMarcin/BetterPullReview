---
id: BPR-0465
title: The cleanup-retry course cannot stage its substitute on Windows
status: In Progress
rfc: RFC-BPR-0011
depends_on: []
---

# The cleanup-retry course cannot stage its substitute on Windows

`application_agent_invocation_cleanup_retry_contract` fails at
`cleanup-retry-substitute` on the Windows host. The course
(`tests/end_to_end/agent_invocation_composition_public_path.cpp`, the
`cleanup-retry` branch) proves that workspace cleanup refuses a foreign
directory: with the canary paused pre-release, it renames the live
workspace aside, creates an empty substitute at the same path, releases
the canary, and requires `WorkspaceCleanupFailures == 1` with
`WorkspaceCleanups == 0` and a successful agent terminal.

The staging step is the Windows problem, not the assertion. The
workspace is the running canary's current directory —
`windows_agent_process.cpp` hands it to `CreateProcessW` as
`lpCurrentDirectory`, which is the product's own agents-run-in-their-
workspace behavior — and Windows holds a cwd's directory handle without
delete sharing, so `std::filesystem::rename` on it fails where POSIX
permits it. The premise "swap the directory out from under a live
process" is not expressible this way on this platform.

## Required final state

- A Windows staging that reaches the same assertion — a foreign
  directory at the workspace path when cleanup runs, cleanup refusing it
  exactly once, the agent terminal unaffected — or a recorded reason the
  claim is untestable here, with the course skipping self-describingly
  rather than failing.
- Candidate directions to evaluate, not prescribe: substituting between
  canary exit and cleanup through a canary-assisted handshake; renaming
  an ancestor rather than the cwd itself; staging through whatever
  identity the cleanup actually verifies. Whichever is chosen must not
  weaken what the POSIX course proves.

## Non-goals

- Not the cancellation or workspace-diagnostic courses; their Windows
  repairs (`.exe`-suffixed canary copies, spawn-and-wait in the git
  diagnostic wrapper) landed with [[BPR-0240]]'s diagnosis.
- No product cleanup-semantics change.

## The staging chosen, and why — 2026-08-27

A scout established two facts the candidates turned on. First, the canary
polls for its release marker through the **captured path string** it
emitted as `cwd\t`, not through its OS current directory — that is the
same path-versus-identity divergence the POSIX course exploits. Second,
the refusal layer the POSIX course actually exercises is the
`.git/bpr-owned-workspace` ownership-marker check in
`installed_git_process.cpp`'s `cleanup_agent_workspace`; the deeper
volume/file-ID `exact_directory_identity` comparison is only reached
after the marker passes, so the existing course never invokes it on
either platform — its empty substitute short-circuits at the absent
marker.

That opens a staging none of the card's three candidates named exactly:
**the canary vacates its own cwd**. In `cleanup-retry-wait` mode, Windows
only, the canary steps to the workspace's parent as it enters the release
poll; the poll still reads the captured absolute path, so it observes the
substitute the course creates at the same literal path. The course's
swap-aside rename becomes a bounded retry that self-synchronizes on the
OS itself: it fails with a sharing violation exactly until the cwd handle
is released, then succeeds. Everything the POSIX course proves is
preserved — a genuinely foreign directory object at the workspace path
when cleanup runs, refused exactly once at the same marker layer, the
canary alive and paused as the ordering mechanism, the substitute
preserved, and the restored original cleaned at shutdown.

The honest deviation, recorded rather than hidden: on Windows the swap
happens while the live agent has stepped off its cwd, because the
platform holds a cwd's directory handle without delete sharing — the
card's own founding fact. That is also the production-faithful shape:
real cleanup always runs after the child has exited and released the
handle.

Implemented in `agent_canary_main.cpp` (the Windows-gated vacate) and
`agent_invocation_composition_public_path.cpp` (the rename retry, inert
on POSIX where the first attempt succeeds). The focused green waits on
the relinked canary image's reputation verdict; a retry loop is armed,
and the sibling canary courses rerun with it.

---
id: BPR-0465
title: The cleanup-retry course cannot stage its substitute on Windows
status: Ready
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

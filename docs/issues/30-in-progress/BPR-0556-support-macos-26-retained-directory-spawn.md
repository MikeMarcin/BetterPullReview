---
id: BPR-0556
title: Support macOS 26 retained-directory spawn without dropping older targets
status: In Progress
rfc: RFC-BPR-0008
depends_on: []
---

# Support macOS 26 retained-directory spawn without dropping older targets

The current macOS 26 SDK deprecates `posix_spawn_file_actions_addfchdir_np`.
The warnings-as-errors build fails at both retained-root spawn sites, blocking
BPR-0256's capsule refresh and the BPR-0316 platform course.

## Required final state

- Use the standardized descriptor-based spawn action on macOS 26 and retain
  the existing descriptor-based API on older SDKs and deployment targets.
- Both verifier and application still spawn relative to the retained root
  descriptor; no pathname reopen or process-global directory change is added.
- The native continuity test, architecture boundary checks, scoped formatting,
  documentation gate, and development build pass.

## Non-goals

No live launch, approval consumption, credential access, or remote mutation.

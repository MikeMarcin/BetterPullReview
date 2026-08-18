---
id: BPR-0348
title: Keep the git subprocess error text that failures currently discard
status: Ready
rfc: RFC-BPR-0003
depends_on: []
---

# Keep the git subprocess error text that failures currently discard

Diagnosing BPR-0341 needed an instrumented build, and it should not have.

The installed-Git process adapter drains a subprocess's standard error into a
byte count and passes `nullptr` as the sink, so the text is read and thrown
away. On a nonzero exit the adapter returns `git_process_failure::nonzero_exit`
and nothing else. Git had said exactly what was wrong:

```
error: unable to write file /tmp/.../pack-f041bc5e....pack:
       Invalid cross-device link
```

None of that reached any log, any evidence record, or any test output.

The loss compounds on the way up. `nonzero_exit` becomes
`git_cache_failure`, which becomes `agent_workspace_failure::unavailable`,
which `workspace_outcome` turns into `agent_run_outcome::failed_to_start`. The
operator is told the agent failed to start. The agent was never reached: the
failure was three layers earlier, in a pack write.

## Required final state

- A failing git subprocess retains its standard error, bounded by the limit
  already applied to the stream, and carries it in the typed failure.
- The text reaches evidence and test output on failure. It must not be
  retained on success, where it is currently a `malformed_output` signal, and
  that check keeps its current meaning.
- Nothing secret can enter it. Git is already invoked with credential helpers
  disabled and every protocol denied, and remote URLs are the thing to watch;
  the retained text needs the same redaction rule as any other evidence.
- A workspace that could not be materialized is distinguishable from an
  executable that could not be launched. `failed_to_start` should not be the
  answer to both.
- A test proves a failing git step surfaces its message rather than only a
  code.

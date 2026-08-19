---
id: BPR-0348
title: Keep the git subprocess error text that failures currently discard
status: In Progress
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

## The redaction rule this card assumes does not exist

"The retained text needs the same redaction rule as any other evidence" reads
as a reference to an existing rule. There is none. `grep -rn "redact" src/`
finds no executable redaction anywhere in the tree, and the closest sibling
boundary — the GitHub CLI process adapter — avoids the question entirely by
carrying no free text at all: every one of its failures is a fixed enumerator,
and `tests/github_contract/github_cli_process_runner_tests.cpp` asserts exactly
that.

Worse, the repository has twice written down the opposite principle, in the two
places that faced this choice:

> The alternative — carrying the text and redacting it on the way out — is
> worse in the way that matters. A redaction step is a place a mistake can
> live, and the mistake is silent.
>
> — `review_queue_notification.hpp:24-28`, echoed at
> `docs/architecture/ai-agent-boundary.md:253`

So this card cannot cite a rule; it has to make one, and it has to say why this
case differs from the two where the answer was to carry nothing.

It does differ, and the difference is the destination. Those two are about
repository content leaving the product permanently — into an operating-system
notification store that outlives the application and syncs to other devices,
and into a third-party AI tool. This is a local diagnostic reaching a log and a
test failure, and the thing being carried is git's own report about the
filesystem, not the reviewer's content. What has to be excluded is narrower and
nameable: a remote URL, which is the one thing in git's diagnostics that can
carry a host, an account, or a token.

That is the rule this card will write and test, rather than the rule it assumed
it could reuse. If a reviewer disagrees that the difference is enough, the
alternative is to keep the exit code and add a typed enumerator per known cause,
which is what the CLI adapter did — and BPR-0341 is the evidence for how that
goes when the cause is not one of the ones already enumerated.

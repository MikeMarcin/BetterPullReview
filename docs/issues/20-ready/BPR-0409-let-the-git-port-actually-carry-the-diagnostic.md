---
id: BPR-0409
title: Let the Git port actually carry the diagnostic
status: Ready
rfc: RFC-BPR-0003
depends_on: []
---

# Let the Git port actually carry the diagnostic

[[BPR-0348]]'s ruling revised the architecture rule; the rule now permits one
bounded, redacted diagnostic member on `git_process_port` and says why that
differs from the destinations `review-queue.md` and `ai-agent-boundary.md`
describe. Nothing yet uses the permission. This card is the restoration.

Until it lands the repository holds a permissive rule with no user, which is a
deliberate and stated intermediate state rather than an oversight.

## Read this before starting

**This card relinks the application.** Three operator packets bind the exact
bytes of `build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review`
(`sha256:7b9d6be9…a78fcd93`), and a relink invalidates all three at once. See the
dev-binary hold in the board README.

That is a sequencing constraint, not a dependency: the hold lifts when those
three attended verdicts come back, or on owner direction. Check it in one command
before starting, rather than trusting this paragraph:

```sh
shasum -a 256 build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
```

## Where the prior work is

An implementation existed and was reverted rather than landed against an
unrevised rule. It is recoverable:

- `git show ee4c675` — the fix, *"keep the standard error a failing subprocess
  explained itself with"*.
- `git show 302c2ce` — its revert.

Its redaction mechanism is the one to reuse rather than reinvent:
`remote_url_token()` and `redacted_diagnostic()` tokenize on whitespace, replace
any token containing `://` or matching an SSH-style `user@host:` shape — while
excluding git's own `HEAD@{1}` syntax — with a fixed placeholder, preserve
everything else verbatim, then truncate to `standard_error_limit`. The commit
message states the design intent exactly: *"exactly statable and exactly tested
rather than being general secret detection, which would fail silently."*

Its tests were mutation-checked, proving a real failing git step surfaces its own
`cannot lock ref` diagnosis naming the exact ref, that a URL is redacted in
place, that success carries no text, and that the message reaches the run
terminal unaltered.

## Required final state

- `git_process_port` declares exactly one text member, named `Detail`. The
  revised gate enforces both the name and the count, so a second string fails the
  build.
- A nonzero exit carries bounded, redacted standard error. Success stays
  textless, and `malformed_output` keeps its meaning.
- Remote URLs are redacted. Refs and local paths are **not** — they are the
  diagnosis, and [[BPR-0348]]'s mutation-checked evidence covers exactly that
  case.
- The text survives every propagation hop to where an operator reads it, and
  each hop has a test. [[BPR-0348]] records that the reverted work stopped at the
  cache boundary and that removing the text there broke nothing, because only the
  far end was asserted. Do not repeat that.
- An unmaterializable workspace is distinguishable from a launch failure.

## The hops, which are the real work

The port and adapter are the small half. Carrying text to an operator means
threading it through types that today keep bare codes:

| # | surface | what it needs |
|---|---|---|
| 1 | `src/application/include/bpr/application/git_process_port.hpp` | the `Detail` member; the file the gate scans |
| 2 | `src/adapters/git/src/installed_git_process.cpp` | retain stderr, redact, attach on the `nonzero_exit` path |
| 3 | `src/adapters/git/src/installed_git_cache.cpp` | `process_failure()` reads only the code today — this is where the reverted work stopped |
| 4 | `src/application/include/bpr/application/git_cache_port.hpp` | `git_cache_failure` is a bare enum |
| 5 | `src/application/include/bpr/application/agent_workspace.hpp` | same, plus `cleanup_agent_workspace` |
| 6 | `src/application/src/epoch_application_host.cpp` | where it becomes `agent_run_result::StandardError` |

Hops 3 through 6 are what turn "agent failed to start" back into the pack-write
failure it actually was.

## Also unaddressed, from [[BPR-0348]]

- `cleanup_agent_workspace` drops any explanation, independent of the above.
- `make_installed_git_cache` and `make_installed_git_process` keep bare-code error
  types, so construction failures carry nothing.

## Non-goals

- Do not widen the revised rule. If the implementation seems to need a second
  string or a collection, that is a signal to stop and reopen [[BPR-0348]]'s
  ruling, not to edit the gate.
- Do not redact refs or paths.
- Do not start while the dev-binary hold is in force without owner direction.

## Verification

```sh
ctest --test-dir build/core-only --output-on-failure \
  -R '^(architecture_boundaries|architecture_boundary_contract)$'
python3 scripts/verify_documentation.py --root .
git diff --check
```

plus the git contract selectors covering hops 1 through 6, and `clang-format` on
every touched file.

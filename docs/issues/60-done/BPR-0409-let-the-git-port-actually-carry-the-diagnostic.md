---
id: BPR-0409
title: Let the Git port actually carry the diagnostic
status: Done
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

## Owner release and claim

The product owner released the protected-binary hold for this slice on
2026-08-20:

> I authorize replacing the protected dev binary to implement BPR-0409,
> BPR-0410, and BPR-0334.

The exact held executable was checked immediately before this card moved to
`30-in-progress/`: it was still 51,085,872 bytes with SHA-256
`7b9d6be98c89b70a49f0616d2b32b10cb6b5d4a3f238366bed916379a78fcd93`.
BPR-0409 is the first separately implemented and verified slice in that bounded
batch.

## Read this before starting

**This card relinks the application.** Two attended packet families bind
the exact bytes of
`build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review`
(`sha256:7b9d6be9…a78fcd93`), and a relink invalidates both at once. See the
dev-binary hold in the board README. The notification packet was already run
successfully and no longer holds these bytes.

That was a sequencing constraint, not a dependency. BPR-0100's visual verdict
was recorded before the owner released these bytes. The failed appearance run
still needs [[BPR-0410]]'s diagnostic repair before its successor packet can be
run. The digest above was checked in one command before this claim rather than
trusted from prose:

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
| 6 | `src/application/src/epoch_application_host.cpp` | preserve the workspace materialization and cleanup detail instead of returning a bare `agent_workspace_failure` |
| 7 | `src/application/src/agent_invocation_host.cpp` | turn the workspace error into `agent_run_result::StandardError` alongside `workspace_unavailable` |
| 8 | `src/adapters/qt/src/agent_launch_panel.cpp` | render the bounded `StandardError` on the failed terminal; today every failure clears output and shows only the outcome |

Hops 3 through 8 are what turn "agent failed to start" back into the pack-write
failure it actually was. Reaching `agent_run_result::StandardError` is not the
operator-facing claim: the Qt terminal must display it too.

## Also unaddressed, from [[BPR-0348]]

- `cleanup_agent_workspace` drops any explanation, independent of the above.
- `make_installed_git_cache` and `make_installed_git_process` keep bare-code error
  types, so construction failures carry nothing.

## Non-goals

- Do not widen the revised rule. If the implementation seems to need a second
  string or a collection, that is a signal to stop and reopen [[BPR-0348]]'s
  ruling, not to edit the gate.
- Do not redact refs or paths.
- Do not widen the owner's release beyond BPR-0409, BPR-0410, and BPR-0334.

## Verification

```sh
ctest --test-dir build/dev --output-on-failure \
  -R '^(git_process_contract|git_cache_contract|application_epoch_host|application_agent_invocation_host|qt_agent_launch_panel|application_agent_invocation_workspace_diagnostic_contract|architecture_boundaries|architecture_boundary_contract)$'
python3 scripts/verify_documentation.py --root .
git diff --check
```

Each hop needs a negative mutation that proves removing `Detail` there breaks
the selector. The end-to-end failure fixture must assert the same redacted
diagnostic at the Git process, cache, workspace, application result, and visible
Qt terminal. Run `clang-format` on every touched file.

## Result — 2026-08-21

The port now carries exactly one `std::string Detail`. Installed Git retains
only bounded nonzero-exit stderr, replaces HTTP(S) and SSH-style remote tokens
with `<redacted-url>`, and keeps local paths, refs, and reflog syntax exact.
Success, accepted stderr (`malformed_output`), and the standard-error limit
remain textless.

The same exact diagnostic now crosses process construction, cache construction
and open, cache process mapping, workspace materialization and cleanup, the
agent application's `StandardError`, and the visible Qt terminal. A real
selected-diff composition course uses a test-only Git wrapper that forwards all
ordinary commands to installed Git and fails only the marker-authorized agent
workspace `pack-objects --revs` operation. It proves one workspace start, zero
agent-process starts, `Finished: workspace_unavailable`, the exact redacted
diagnostic, and host quiescence. The wrapper and Git-executable override are
testing-only; ordinary product composition is unchanged.

The final augmented selector passed 8/8 in 384.98 seconds on macOS. The positive
agent composition and conditional-platform branch gate also passed, the
repository-wide C++ format check passed across 384 files, all 98 documentation
tests passed, `DOCUMENTATION_OK` passed, and `git diff --check` was clean. The
independent RFC re-review found no blocking or non-blocking issue. Its Windows
wrapper branch was source-reviewed and covered by the conditional branch gate,
but this macOS slice makes no new target-native Windows runtime claim.

The final BPR-0409 development executable is 51,302,640 bytes with SHA-256
`cb044fcab11545b181ae35ea6b4114d1f5ab0eef4457ec1945a03bc39fa7e8b7`.
That identity supersedes the historical attended binary for development work;
no earlier attended packet is presented as evidence for these bytes.

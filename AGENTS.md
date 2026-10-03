# BetterPullReview Agent Guide

This file applies to the repository unless a nearer `AGENTS.md` adds more
specific instructions.

## Read first

1. `README.md` and `CONTRIBUTING.md`
2. `_coding-standard/STANDARD.md`
3. `docs/architecture/product-contract.md`
4. `docs/architecture/system-architecture.md`
5. The current RFC in `docs/rfcs/`
6. The live board and current card in `docs/issues/`
7. The nearest nested `AGENTS.md`

## Governing architecture

The core decides. The host performs. Qt presents.

| Scope | Owner |
|---|---|
| `src/core/` | Deterministic policy, compact interaction state, typed transitions, request identity, semantic deltas, diagnostics |
| `src/application/` | Effects, bounded observations, serialized completion lane, cancellation, lifecycle |
| `src/adapters/qt/` | Qt events, widgets, actions, models, focus, accessibility, rendering, incremental projection |
| `src/adapters/{github,git,sqlite,agent,syntax,mcp,workspace}` | Narrow application-host capability ports; no provider types cross inward. `mcp` is the shipped read-only context helper (its own executable, core-only link). `workspace` reads a chosen local root without following links, writing or running Git (Review Cycles) |
| `src/platform/` | Header-only platform definitions that several adapters share and none owns, so no adapter-to-adapter edge is needed: the Windows owner-private object, its descriptor and its exact check; and the Windows launch restriction that gives a `QProcess` child its three standard handles and nothing else, and refuses at creation a launch whose list cannot be built; and the bound a Git or `gh` launch puts on its child's descriptor sweep on macOS. No adapter state, Qt, provider, file-system policy or decision about what a process runs; `architecture_boundaries` refuses a second descriptor builder and keeps the handle-list APIs to the platform header and the named launch owners |
| `src/app/` | Concrete composition and application entry point |
| `tests/` | Core, host, contract, Qt, end-to-end, fixture, benchmark, and fuzz evidence |
| `packaging/` | Distribution metadata and release-time platform policy |

The functional core must not include Qt, provider, filesystem, process, clock,
thread, timer, network, database, credential, or hidden-global dependencies.
A widget or Qt model must not perform GitHub, Git, SQLite, AI, filesystem,
network, or process effects.

## Workflow

- RFCs close when their explicit dependencies and acceptance criteria are satisfied; index position is not a gate.
- Issue folder placement is status. `60-done/` is terminal.
- Use MASK-managed implement-rfc with `.mask/skill-config/implement-rfc/` context.
- Allocate issue IDs with `python3 scripts/allocate_issue_id.py`; commit returned counter and receipt paths.
- Cards use `implement-rfc.card.v1`; platform eligibility is metadata.
- Shared attended-review and wizard support partial sessions and local HTML.
- An attended candidate that asks for a file on disk names every path in its
  command absolutely. A copy label saying "in the repository" is not a working
  directory: BPR-0746's v009 probe was run from one directory above the
  checkout, so its relative `tee` opened nothing and the log was lost.
- A frozen attended build keeps its source and build tree inside its own
  root. A tests-on build compiles absolute paths to its source tree's
  `tests/fixtures` and its build tree's helpers. BPR-1250's help-v002 build
  came from a worktree that was removed after freezing, so at launch it
  printed `STARTUP_FAILED` and closed before any window. Export the commit
  into the root, build there, and have the launcher check the fixture tree
  before starting and report an exit within seconds.
- Ask one question per candidate criterion. A question riding a criterion about
  something else comes back unanswered (BPR-0744 v008 and v009).
- Ask a keep-or-change judgment as Accept or Reject, with the change in the
  note. Keep option lists for a choice between named alternatives. The owner
  asked for this on 2026-09-25, when most of fourteen RFC-BPR-0052 questions
  came as "Keep / Revise / Other" lists (session 06ad5734).
- Commit each completed issue slice intentionally.
- For release work, use the existing `publish-release` skill and package paths.
  Bind the separate versioned corresponding-source asset as well as the binary,
  SPDX and installed notices; an offline audit is not trust or release approval.
- Keep M8/M9 native, vault, crash and vulnerability gaps explicit in operator
  claims. Profile purge excludes shared updater/registry state and is not uninstall.
- Use fixtures or designated safe test repositories for remote mutations.
- Never commit credentials, private repository content, transient inspection
  artifacts, build output, or secret-bearing logs.

## Commands

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
cmake --build --preset dev --target coding-format-check coding-tidy

cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only --output-on-failure

python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
```

Both documentation commands require **Python 3.10 or newer** and refuse to run
otherwise, naming the version they found and the interpreter they found it at.
On macOS `/usr/bin/python3` is older than that, so `python3` must resolve to a
newer interpreter. Run a gate without a pipe: `| tail` reports the pipe's exit
status, not the gate's.

Before RFC closeout, run the documentation gate and focused acceptance/public-path checks. At pack or milestone integration, normally run one integration-focused pass; full relevant tests and critical review are appropriate when scope warrants them. Application-wide builds, exhaustive matrices, hermetic replay, repeated reviews and manual approvals are not defaults. Reuse unchanged passing evidence.

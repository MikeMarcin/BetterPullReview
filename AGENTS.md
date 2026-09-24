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
| `src/adapters/{github,git,sqlite,agent,syntax,mcp}` | Narrow application-host capability ports; no provider types cross inward. `mcp` is the shipped read-only context helper (its own executable, core-only link) |
| `src/platform/` | Header-only platform definitions that several adapters share and none owns, so no adapter-to-adapter edge is needed: today the Windows owner-private object, its descriptor and its exact check. No adapter state, Qt, provider, file-system policy or process decision; `architecture_boundaries` refuses a second descriptor builder |
| `src/app/` | Concrete composition and application entry point |
| `tests/` | Core, host, contract, Qt, end-to-end, fixture, benchmark, and fuzz evidence |
| `packaging/` | Distribution metadata and release-time platform policy |

The functional core must not include Qt, provider, filesystem, process, clock,
thread, timer, network, database, credential, or hidden-global dependencies.
A widget or Qt model must not perform GitHub, Git, SQLite, AI, filesystem,
network, or process effects.

## Workflow

- RFCs close sequentially in the canonical index order.
- Issue folder placement is status. `60-done/` is terminal.
- Use MASK-managed implement-rfc with `.mask/skill-config/implement-rfc/` context.
- Allocate issue IDs with `python3 scripts/allocate_issue_id.py`; commit returned counter and receipt paths.
- Cards use `implement-rfc.card.v1`; platform eligibility is metadata.
- Shared attended-review and wizard support partial sessions and local HTML.
- An attended candidate that asks for a file on disk names every path in its
  command absolutely. A copy label saying "in the repository" is not a working
  directory: BPR-0746's v009 probe was run from one directory above the
  checkout, so its relative `tee` opened nothing and the log was lost.
- Ask one question per candidate criterion. A question riding a criterion about
  something else comes back unanswered (BPR-0744 v008 and v009).
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

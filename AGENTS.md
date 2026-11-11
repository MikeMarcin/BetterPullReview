# BetterPullReview Agent Guide

This file applies to the repository unless a nearer `AGENTS.md` adds more
specific instructions.

## Read first

Read what the task touches rather than everything:

1. The `AGENTS.md` nearest each directory you change.
2. The RFC in `docs/rfcs/` and the card in `docs/issues/` the work belongs to,
   and `docs/issues/README.md` before moving a card.
3. `_coding-standard/STANDARD.md` before writing C++.
4. The parts of `docs/architecture/product-contract.md` and
   `docs/architecture/system-architecture.md` that cover the layer or
   behaviour you change. The system architecture is long: search it for your
   layer instead of reading it whole.
5. `README.md` and `CONTRIBUTING.md` when the checkout is new to you.

## Governing architecture

The core decides. The host performs. Qt presents.

| Scope | Owner |
|---|---|
| `src/core/` | Deterministic policy, compact interaction state, typed transitions, request identity, semantic deltas, diagnostics |
| `src/application/` | Effects, bounded observations, serialized completion lane, cancellation, lifecycle |
| `src/adapters/qt/` | Qt events, widgets, actions, models, focus, accessibility, rendering, incremental projection |
| `src/adapters/{github,git,sqlite,agent,syntax,mcp,workspace}` | Narrow application-host capability ports; no provider types cross inward. `mcp` is the shipped read-only context helper (its own executable, core-only link). `workspace` reads a chosen local root without following links, writing or running Git. Pure xdiff matching and C++ identifier refinement live in core, not in an adapter |
| `src/adapters/process_support/` | The QProcess supervision the gh and Git runners share. Qt Core only, no first-party dependency; it launches nothing and decides nothing about what a process runs |
| `src/platform/` | Header-only platform definitions several adapters share and none owns: the Windows owner-private object, the Windows launch handle restriction, and the descriptor-sweep bound for Git and `gh` launches. No adapter state, Qt, provider or policy; `architecture_boundaries` keeps the descriptor builder and handle-list APIs to this header and the named launch owners |
| `src/app/` | Concrete composition and application entry point |
| `tests/` | Core, host, contract, Qt, end-to-end, fixture, benchmark, and fuzz evidence |
| `packaging/` | Distribution metadata and release-time platform policy |

The functional core follows [external determinism](docs/architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md#21-external-determinism):
equal explicit inputs, state and options yield equal semantic results, identities,
ordering and diagnostics under the specified contract. Internal implementation
mechanisms do not disqualify computation when they only affect timing or layout.
Effects and observations remain explicit; the host owns scheduling, cancellation
and lifecycle. Qt independence and approved dependency/include/link ownership are
separate constraints. The [core implementation inventory](docs/architecture/system-architecture.md#core-implementation-inventory)
records current libraries, wrappers and use limits. It does not define exceptions
to the determinism rule.
A widget or Qt model must not perform GitHub, Git, SQLite, AI, filesystem,
network, or process effects.

## Workflow

### Board, RFCs and commits

- RFCs close when their explicit dependencies and acceptance criteria are satisfied; index position is not a gate.
- Issue folder placement is status. `60-done/` is terminal.
- `main` is the tracker's source of truth: mirror every card move made on a lane to `main` (the card's old and new paths, `docs/issues/NEXT_ID` and `docs/issues/_reservations`) in the same round.
- Use MASK-managed implement-rfc with `.mask/skill-config/implement-rfc/` context.
- Allocate issue IDs with `python3 scripts/allocate_issue_id.py`; commit returned counter and receipt paths.
- Commit each completed issue slice intentionally.
- Never commit credentials, private repository content, transient inspection
  artifacts, build output, or secret-bearing logs.
- Before hand-writing a major general-purpose system in `src/core/` (a parser,
  codec, hash, regex or diff engine and the like), ask the owner whether a
  library should be allowed instead. `src/core/AGENTS.md` says what the
  question must name.
- Release work uses the `publish-release` skill
  (`.codex/skills/publish-release/SKILL.md`, readable from any harness) and the
  existing package paths. `packaging/AGENTS.md` holds the release rules.

### Parking work on the owner

- Before parking anything on the owner, do every step an agent can do
  itself: live GitHub courses on BPR-Test through `gh` and an offscreen app,
  log reading, fixture checks. Only a human judgement, a human-only device or
  account, or a secret the owner must type reaches them.
- What is parked reaches the owner as an attended-review session or a wizard,
  never only as a chat note: a look-and-judge criterion gets a frozen build
  and an Accept/Reject question, a manual procedure (display scaling,
  VoiceOver, another machine or account) gets a wizard.
- Every attended review opens its own new, short session (a fresh `--state`
  directory). Never add candidates to or restart the long default session
  `cbd48b91…`.
- Ask one question per candidate criterion. A question riding a criterion about
  something else comes back unanswered.
- Ask a keep-or-change judgment as Accept or Reject, with the change in the
  note. Keep option lists for a choice between named alternatives.
- An attended candidate that asks for a file on disk names every path in its
  command absolutely. A copy label saying "in the repository" is not a working
  directory, and a relative path run from elsewhere writes nothing useful.
- Freeze an attended build with `scripts/freeze_attended_build.py freeze` and
  remove its root with `remove`; do not hand-write a launcher or delete a root
  directly. A tests-on build compiles absolute paths to its fixtures and
  helpers, so the tool exports the commit into the root, builds there and runs
  the launcher's own `--check` before the root is handed over. The fixtures a
  build serves are committed (`--github-fixture` names a `contracts/v1` tree
  and packs under `pull-requests`), so a freeze runs no
  `scripts/build_*_fixture.py`; a fixture that must be generated goes through
  `--generate`. Helpers a look launches are `--target`s and `--argument`s.
- Never copy authenticated attended-review or wizard URLs into tracked files.
  Record the session ID, external state directory and lifecycle status instead;
  keep credential-bearing URLs in the external session state.
- Any UX or visual work (mockups, design candidates, icons, layout, Qt
  surfaces the owner will look at) loads the `frontend-design:frontend-design`
  skill first and says so in its report.

### The owner's machine is shared

- Never drive or capture the owner's live macOS desktop (`screencapture`,
  `cliclick`, foreground windows): it may be screen-shared. Use offscreen
  renders and the frozen build; the owner looks themselves. The one exception
  is promotional capture: when the owner asks for promo assets in the session,
  `.codex/skills/update-promo-assets` may run
  `scripts/capture_promo_assets.py --confirm-desktop`, after saying so, for
  about a minute. It captures only the application's own window, never a
  screen region.
- Never read the owner's browser profiles, history, cookies, open-tab files,
  keychains or other personal application data to confirm a result. Verify
  through the product's own output or a stub, and ask the owner to confirm what
  only they can see.
- Never use `git stash`: the stash list is shared by every worktree of the
  checkout, so `stash pop` can take another agent's work. Use WIP commits on
  your own branch.
- Stop only processes you started, by their process ID. Never `pkill -f` or
  `killall` by pattern: several agents build at once.
- Before removing task checkouts, inspect Git alternates and linked worktrees.
  A `clone --shared` may borrow from another task clone: preserve or dissociate
  its reachable objects before removing that object owner, and remove linked
  worktrees before their common clone. Verify the archived object store and
  source snapshots first; keep native Git alternates paths LF-terminated.

### Live GitHub

- Use fixtures or designated safe test repositories for remote mutations.
- `github.com/MikeMarcin/BPR-Test` is the owner's designated live GitHub
  target for any testing need. Live cards may create branches, pull requests,
  reviews, comments and threads there, and resolve or close what they made. Do
  not delete the repository, change its settings or collaborators, or touch
  any other repository.
- `gh` on this Mac holds two github.com accounts: `MikeMarcin` (active, the
  owner) and `MikeMarcinTest`, a test account for second-identity courses on
  BPR-Test. Never run `gh auth switch`: the active account is global, other
  agents and the app's account lease use it, and the app refuses a read whose
  account changed. Act as the test account per command instead:
  `GH_TOKEN="$(gh auth token --user MikeMarcinTest)" gh …`, never printing or
  logging the token. For the app, give the course its own sign-in rather than
  switching the shared one.
- Live GitHub calls spend the owner's own `gh` rate limit (5,000 an hour,
  shared with everything else on that account). Before and during a live run,
  check the `X-RateLimit-Remaining` response header, not `gh api rate_limit`,
  which can keep reporting 5000 while the account runs dry. Keep a margin and
  stop when it runs low.

### Debug timings

- Debug performance timings are permanently nongating (owner, 2026-10-26).
  The policy has no per-release expiry; do not ask again when Debug timing
  misses recur. Omit dedicated timing and performance courses from Debug
  release qualification; record their exact names and omissions without
  passing or Debug functional coverage credit.
- Keep Debug functional checks. A timing-sensitive assertion in a mixed course
  does not block a release or need a waiver; retain its actual failure and
  unreached checks. If a short execution budget cuts off a functional course,
  a larger bounded Debug watchdog may finish its semantic checks. Actual hangs
  and functional failures still require investigation.
- Optimized Release timing, memory/resource, compilation, source/legal,
  security, signing, notarization and native trust requirements are separate
  and unaffected.

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

The format gate runs **clang-format 22, exactly** and refuses any other major
release: 23 orders includes and aligns assignments differently, so a file
formatted with Homebrew's default 23 fails Linux's 22. On macOS
`brew install llvm@22`; on Linux `apt install clang-format-22`. The scripts and
configure find either one ahead of `PATH`'s `clang-format`. Reformat with
`coding-format` or `_coding-standard/scripts/format.py`, never a bare
`clang-format -i` from `PATH`.

Both documentation commands require **Python 3.10 or newer** and refuse to run
otherwise, naming the version they found and the interpreter they found it at.
On macOS `/usr/bin/python3` is older than that, so `python3` must resolve to a
newer interpreter. Run a gate without a pipe: `| tail` reports the pipe's exit
status, not the gate's.

Before RFC closeout, run the documentation gate and focused acceptance/public-path checks. At pack or milestone integration, normally run one integration-focused pass; full relevant tests and critical review are appropriate when scope warrants them. Application-wide builds, exhaustive matrices, hermetic replay, repeated reviews and manual approvals are not defaults. Reuse unchanged passing evidence.

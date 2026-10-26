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
| `src/adapters/{github,git,sqlite,agent,syntax,mcp,workspace}` | Narrow application-host capability ports; no provider types cross inward. `mcp` is the shipped read-only context helper (its own executable, core-only link). `workspace` reads a chosen local root without following links, writing or running Git (Review Cycles). Pure xdiff matching and C++ identifier refinement live in core (BPR-1619) |
| `src/adapters/process_support/` | Shared adapter support: the QProcess supervision the gh and Git runners share (serialized lane, quarantine, process-group kill, bounded reap and abandonment). Qt Core only, no first-party dependency; it launches nothing and decides nothing about what a process runs |
| `src/platform/` | Header-only platform definitions that several adapters share and none owns, so no adapter-to-adapter edge is needed: the Windows owner-private object, its descriptor and its exact check; and the Windows launch restriction that gives a `QProcess` child its three standard handles and nothing else, and refuses at creation a launch whose list cannot be built; and the bound a Git or `gh` launch puts on its child's descriptor sweep on macOS. No adapter state, Qt, provider, file-system policy or decision about what a process runs; `architecture_boundaries` refuses a second descriptor builder and keeps the handle-list APIs to the platform header and the named launch owners |
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
  Freeze with `scripts/freeze_attended_build.py freeze`: it writes the
  launcher from one template and runs that launcher's own `--check` (fixture
  check, then an offscreen launch to the file) before the root is handed
  over. BPR-1331, BPR-1339 and BPR-1091's v001 launchers checked
  `tests/fixtures/github/pull-requests/conventional-comments-v1`, a path no
  commit has had, and the owner got `FIXTURES_MISSING`. The fixtures a build
  serves are committed: `--github-fixture` names a `contracts/v1` tree and
  packs under `pull-requests`, and the `scripts/build_*_fixture.py` scripts
  regenerate committed trees, so a freeze runs none of them. A fixture that
  is ever generated instead goes through `--generate`, which runs in the
  root's `src` before the build, so it lands where the binary reads. Helpers
  a look launches are `--target`s and `--argument`s (`{build}` is the root's
  build tree). Remove a root with `remove`, which restores write access to
  the read-only source first.
- Every attended review opens its own new, short session (a fresh `--state`
  directory). Never add candidates to or restart the long default session
  `cbd48b91…`; the owner asked for short sessions on 2026-10-04.
- Never copy authenticated attended-review or wizard URLs into tracked files.
  Record the session ID, external state directory and lifecycle status instead;
  keep credential-bearing URLs in the external session state.
- Implementers never drive or capture the owner's live macOS desktop
  (`screencapture`, `cliclick`, foreground windows): it may be screen-shared.
  Use offscreen renders and the frozen build; the owner looks themselves.
  The one exception is promotional capture ([[BPR-1676]]): when the owner
  asks for promo assets in the session, `.codex/skills/update-promo-assets`
  may run `scripts/capture_promo_assets.py --confirm-desktop`, after saying so,
  for about a minute. It captures only the application's own window, never a
  screen region (owner, 2026-10-20).
- Never read the owner's browser profiles, history, cookies, open-tab files,
  keychains or other personal application data to confirm a result. Verify
  through the product's own output or a stub, and ask the owner to confirm what
  only they can see (BPR-1338 v002 read Chrome's session and history files).
- Anything parked on the owner reaches them as an attended-review session or a
  wizard, never only as a chat note: a look-and-judge criterion gets a frozen
  build and an Accept/Reject question, a manual procedure (display scaling,
  VoiceOver, another machine or account) gets a wizard (owner, 2026-10-09).
- Before parking anything on the owner, do every step an agent can do
  itself: live GitHub courses on BPR-Test through `gh` and an offscreen app,
  log reading, fixture checks. Only a human judgement, a human-only device or
  account, or a secret the owner must type reaches them (owner, 2026-10-09,
  on BPR-0143's mixed-client wizard: "this all seems like stuff you can do
  without me").
- Ask one question per candidate criterion. A question riding a criterion about
  something else comes back unanswered (BPR-0744 v008 and v009).
- Ask a keep-or-change judgment as Accept or Reject, with the change in the
  note. Keep option lists for a choice between named alternatives. The owner
  asked for this on 2026-09-25, when most of fourteen RFC-BPR-0052 questions
  came as "Keep / Revise / Other" lists (session 06ad5734).
- Any UX or visual work (mockups, design candidates, icons, layout, Qt
  surfaces the owner will look at) loads the `frontend-design:frontend-design`
  skill first and says so in its report. The owner asked on 2026-10-06 after
  several design rounds ran without it.
- Before implementing a major general-purpose system by hand in `src/core/`
  (a parser or serializer, codec, compressor, hash, regex or diff engine,
  Unicode tables, protocol, or similar infrastructure), ask the owner whether
  a suitable library should be allowed instead. Name the candidate, its
  licence, the layer it would live in, and why it fits or does not; do not
  hand-roll it while that question is open. Core dependencies still require owner
  approval, pinned acquisition and the [current implementation constraints](docs/architecture/system-architecture.md#core-implementation-inventory);
  external determinism establishes semantic eligibility, not adoption approval.
  The 2026-10-13 audit found a hand-written regex
  engine with a correctness bug, eight JSON implementations, a DEFLATE
  inflater and hand-written hashes that libraries should have supplied.
- Never use `git stash` in a worktree: the stash list is shared by every
  worktree of the checkout, so `stash pop` can take another agent's work
  (BPR-1332's bisect swapped BPR-1327's uncommitted files). Use WIP commits on
  your own branch.
- Before removing task checkouts, inspect Git alternates and linked worktrees.
  A `clone --shared` may borrow from another task clone: preserve or dissociate
  its reachable objects before removing that object owner, and remove linked
  worktrees before their common clone. Verify the archived object store and
  source snapshots first; keep native Git alternates paths LF-terminated.
- Stop only processes you started, by their process ID. Never `pkill -f` or
  `killall` by pattern: several agents build at once, and BPR-1346's
  `pkill -f "cmake --build --preset dev"` stopped four other agents' builds.
- Commit each completed issue slice intentionally.
- Debug performance timings are permanently nongating. On 2026-10-26 the owner
  instructed: "From now and until the end of time I won't care about debug
  timings, we dont even have to run those in debug." Omit dedicated timing and
  performance courses from Debug release qualification; record their exact
  names and omissions without passing or Debug functional coverage credit.
  Keep Debug functional checks. A timing-sensitive assertion in a mixed course
  does not block a release or require another waiver; retain its actual failure
  and unreached checks. If a short execution budget cuts off a functional
  course, a larger bounded Debug watchdog may be used to finish its semantic
  checks. Actual hangs and functional failures still require investigation.
  Optimized Release timing, memory/resource, compilation, source/legal,
  security, signing, notarization and native trust requirements are separate.
  This policy has no per-release expiry and does not require a new permission
  request when Debug timing misses recur.

- For release work, use the existing `publish-release` skill and package paths.
  Bind the separate versioned corresponding-source asset as well as the binary,
  SPDX and installed notices; an offline audit is not trust or release approval.
- Keep M8/M9 native, vault, crash and vulnerability gaps explicit in operator
  claims. Profile purge excludes shared updater/registry state and is not uninstall.
- Use fixtures or designated safe test repositories for remote mutations.
- `github.com/MikeMarcin/BPR-Test` is the owner's designated live GitHub
  target for any testing need (owner, 2026-10-05: "For live github tests you
  can use the MikeMarcin/BPR-Test repo for any testing needs"). Live cards may
  create branches, pull requests, reviews, comments and threads there, and
  resolve or close what they made. Do not delete the repository, change its
  settings or collaborators, or touch any other repository.
- `gh` on this Mac holds two github.com accounts: `MikeMarcin` (active, the
  owner) and `MikeMarcinTest`, a test account the owner added on 2026-10-09
  for second-identity courses on BPR-Test. Never run `gh auth switch`: the
  active account is global, other agents and the app's account lease use it,
  and BPR-1370 refuses a read whose account changed. Act as the test account
  per command instead:
  `GH_TOKEN="$(gh auth token --user MikeMarcinTest)" gh …`, never printing or
  logging the token. For the app, give the course its own sign-in rather than
  switching the shared one.
- Live GitHub calls spend the owner's own `gh` rate limit (5,000 an hour,
  shared with everything else on that account). Before and during a live run,
  check the `X-RateLimit-Remaining` response header, not `gh api rate_limit`,
  which kept reporting 5000 while BPR-1344's probe ran the account dry.
  Keep a margin and stop when it runs low.
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

The format gate runs **clang-format 22, exactly** and refuses any other major
release: 23 orders includes and aligns assignments differently, so a file
formatted with Homebrew's default 23 fails Linux's 22 (BPR-1352). On macOS
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

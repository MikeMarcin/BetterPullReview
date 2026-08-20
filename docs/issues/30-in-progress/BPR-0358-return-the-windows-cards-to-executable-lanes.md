---
id: BPR-0358
title: Return the Windows cards to executable lanes
status: In Progress
rfc: RFC-BPR-0013
depends_on: []
---

# Return the Windows cards to executable lanes

[[BPR-0319]] created `50-blocked/` and put eleven cards in it, seven of them
under `blocked_by: platform-windows`. It also wrote the exit condition into the
lane definition, in the board README and in its own text:

> `50-blocked/` is not terminal. A card returns to `10-backlog/` or
> `20-ready/` on the day the platform exists, with no other change required.

The platform exists. This checkout now runs on a real Windows 11 x64 host with
Visual Studio 18 (MSVC 14.51) and the Windows SDK, and [[BPR-0347]] has already
been executed target-native on it: an AppContainer boundary was built here, its
denial was measured here against a working internet connection, and the suite
went from 0 of 84 executing to 60 of 84 passing. That is not a claim about a
host somebody could obtain. It is a record of work this host did.

So the seven `platform-windows` cards no longer satisfy the lane's own
admission rule, which is specifically "cannot be executed **on this host**
because it requires target-native execution on a **different** operating
system". Leaving them where they are would make `50-blocked/` mean "was blocked
once", which is exactly the reading [[BPR-0319]] built the lane to prevent.

## What this card does not decide

Returning a card to an executable lane is not a claim that its work is done,
easy, or even close. Four of the seven remain unstartable for reasons that have
nothing to do with the platform, and this card moves them to the lane that
records the real reason rather than leaving a platform label on a
non-platform blocker.

`platform-linux` is untouched. [[BPR-0310]] and [[BPR-0314]] stay in
`50-blocked/`, because no Linux host exists here.

## The moves

Each row cites the card's own text or its dependency graph, not a narrative.

| Card | To | Why it leaves `50-blocked/` | Why not further |
|---|---|---|---|
| BPR-0036 | `10-backlog/` | Windows 10/11 x64 is this host | its acceptance is a passing complete `windows-dev` CTest matrix; 24 tests still fail |
| BPR-0053 | `10-backlog/` | same | same |
| BPR-0235 | `10-backlog/` | its Linux criterion was discharged 2026-08-17 and its Windows criterion is executable here | that criterion is precisely [[BPR-0347]] and [[BPR-0350]]; its live-lane criterion additionally needs owner authority |
| BPR-0311 | `10-backlog/` | Windows is available | `BPR-0282` and `BPR-0318` are portable work still in `10-backlog/` |
| BPR-0315 | `10-backlog/` | Windows is available | depends on `BPR-0311` and `BPR-0306`, both open |
| BPR-0332 | `10-backlog/` | its stated premise — a host "that cannot compile the backend" — is now false | the backend still has to actually compile, which is [[BPR-0240]]'s Windows slice, and the card then carries an unmade product decision |
| BPR-0035 | `10-backlog/` | transitive only; its platform root left the lane | `BPR-0036` is open |
| BPR-0054 | `10-backlog/` | transitive only | `BPR-0035`, `BPR-0053` open |
| BPR-0083 | `10-backlog/` | transitive only | `BPR-0054` open |

Three cards gain a dependency they always had in substance:

- `BPR-0036` and `BPR-0053` each require a **passing** complete `windows-dev`
  matrix. That matrix cannot pass while the suite is 24 tests short, so both
  now name `BPR-0347` and `BPR-0350`. Without this, the board would offer two
  `20-ready/` cards whose acceptance criterion is known to be unreachable
  today, which is the failure mode `40-parked/`'s frontier-root rule exists to
  prevent in the other direction.
- `BPR-0332` now names `BPR-0240`. Its own text already says its terminal
  criterion is a true and final statement about Windows launch behaviour, and
  `src/adapters/agent/src/windows_agent_process.cpp` has never been compiled by
  any toolchain this project has had.

`blocked_by` is removed from all nine, because the verifier rejects that field
outside `50-blocked/`.

## The Windows program, and its order

This card is the durable anchor for the Windows program the moves open up.

- `RFC_BASE_SHA`: `605aa56c666610516b4af61e61fdd4fccb60d5ca`
- Host: Windows 11 x64 (10.0.26200), MSVC 14.51.36231, Windows SDK via
  `vcvars64.bat`, CPython 3.14.6, Qt 6.10.3 acquired by
  `cmake/BprQtProvider.cmake`.
- Build note, learned the hard way and recorded so nobody re-learns it: every
  `cmake`/`ctest` invocation must run with the MSVC x64 environment imported.
  Without it `cmake --preset core-only` fails inside
  `cmake/WindowsTestIsolationSetup.cmake` with `fatal error C1034: windows.h:
  no include path set`, which reads like an AppContainer-launcher defect and is
  not one.

The frontier is a chain, not a fan-out. Nothing downstream can be verified
while the suite it must run is failing.

1. `BPR-0347` — the suite's own remaining failures, and the Qt graph under the
   boundary.
2. `BPR-0350` — owner-private temporary roots inside an AppContainer, the
   single cause behind 17 of the 24.
3. `BPR-0240` — compile and prove the Windows agent backend.
4. `BPR-0332` — reconcile the six stale launch statements, once 3 makes a true
   statement possible.
5. `BPR-0036`, `BPR-0053` — the RFC-BPR-0002 and RFC-BPR-0003 target-native
   matrices.
6. `BPR-0235` — its Windows criterion, once 1 and 2 are terminal.
7. `BPR-0311`, `BPR-0315` — gated on portable siblings outside this program.

## Required final state

- No card carries `blocked_by: platform-windows`.
- `50-blocked/` holds exactly the two `platform-linux` cards.
- Every moved card's folder, `status`, and dependency set agree, and the
  dependency rule that forbids an unmet dependency outside a deferring lane
  still holds.
- `docs/issues/README.md`'s "Current frontiers" no longer states that Windows
  cards are waiting on hardware.
- The documentation gate and both documentation contract tests pass.

## Verification

```sh
py -3 scripts/verify_documentation.py --root .
py -3 tests/scripts/verify_documentation_tests.py
ctest --test-dir build/core-only --output-on-failure \
  -R '^(documentation_structure|documentation_issue_graph_contract)$'
```

## Progress — 2026-08-18

The lane repair landed at `d8404e6`. `50-blocked/` holds exactly
[[BPR-0310]] and [[BPR-0314]], both `platform-linux`, and no card carries
`blocked_by: platform-windows`.

Two slices of the frontier itself then landed, taking the Windows suite from
**60 of 84 to 68 of 84**:

| commit | card | effect |
|---|---|---|
| `daee1be` | [[BPR-0359]] | five byte-pinned files no longer change under a CRLF checkout; `architecture_boundaries` and `application_icon_assets_contract` pass |
| `acb8295` | [[BPR-0350]] | contained tests can create and use a temporary root; six further tests pass |

### What the frontier actually looks like now

The order this card recorded assumed [[BPR-0347]]'s three failure groups were
three causes. They were three symptoms. Working them exposed six causes, four of
which now have their own cards, and the chain is longer than step 1 → 2 → 3:

- [[BPR-0362]] is the largest single blocker, at six tests, and it is the one
  that carries a real product-owner decision rather than a repair. The container
  cannot read anything above its own `AC` directory — the drive root included —
  and both `Path.resolve(strict=True)` and the product's deliberate retained
  ancestor chain walk all the way up. Widening the sandbox, narrowing the
  security claim, and exempting the storage contracts from containment are three
  defensible answers that give up different things.
- [[BPR-0360]] sits behind it. The product builds the same owner-only descriptor
  [[BPR-0350]] fixed in the test helper, and that defect is proven, but
  `open_state_directory` never reaches the descriptor while [[BPR-0362]] stands.
  An implementation was written and reverted rather than landed unverifiable.
- [[BPR-0361]] and [[BPR-0363]] are independent and unblocked.

So the revised order is: [[BPR-0363]] and [[BPR-0361]] (unblocked, no decision),
then [[BPR-0362]] (needs the decision), then [[BPR-0360]], then [[BPR-0347]]'s
five still-undiagnosed tests and the Qt graph, and only then [[BPR-0036]] and
[[BPR-0053]], whose acceptance is a passing matrix.

### Verification matrix

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| no card claims a Windows platform blocker | issue board front matter | `grep -rn '^blocked_by:' docs/issues/` | only `platform-linux` remains | pass | `d8404e6`; `50-blocked/` holds 2 cards |
| lane, status, and dependency rules still hold | `scripts/verify_documentation.py` | `py -3 scripts/verify_documentation.py --root .` | the verifier rejects lane/status disagreement and an unmet dependency outside a deferring lane | `DOCUMENTATION_OK` | every commit in this run |
| the documentation contract suite still passes | `tests/scripts/verify_documentation_tests.py` | `py -3 tests/scripts/verify_documentation_tests.py` | 78 cases including the blocked-lane cases [[BPR-0319]] added | 78 tests, OK | `d8404e6` |
| byte-pinned files match their pins on Windows | `cmake/VerifyArchitectureBoundaries.cmake`, `app_icon_assets_tests.py` | `ctest -R '^(architecture_boundaries\|application_icon_assets_contract)$'` | both read working-tree bytes and compare to a constant | 2/2 pass | `daee1be` |
| a contained test can create and use a temporary root | the launcher plus `scripts/contained_temporary_support.py` | probe under `better_pull_review_network_sandbox.exe`, and `ctest -R '^core_boundary_contract$'` | `mkdtemp` denied inside and usable outside; the helper usable in both | pass, with negative control | `acb8295` |
| the suite improved and nothing regressed | full Qt-free graph | `ctest --preset core-only --output-on-failure` | 84 registered tests | 68 pass, 16 fail, was 60/24 | `acb8295` |
| the Qt graph under the boundary | `dev` / `windows-dev` presets | not run | — | **not attempted** | [[BPR-0347]] |

### Phase checklist

- [x] planning committed — `d8404e6`
- [ ] implementation cards done — 2 of the program's slices landed; 4 new cards opened
- [ ] simplify committed
- [ ] documentation gate passed over `RFC_BASE_SHA..HEAD`
- [ ] independent review done
- [ ] status flipped — no RFC in this program is close to closeout; all remain `Proposed`

## Run state at `c55f2d7` — 2026-08-18

**The Windows suite went from 60 of 84 to 72 of 85.** The extra registered test
is `conditional_branch_identifier_contract`, added by [[BPR-0366]] to a gate that
had none.

| commit | card | effect |
|---|---|---|
| `d8404e6` | [[BPR-0358]] | nine cards leave `50-blocked/`; the lane holds only the two `platform-linux` cards |
| `daee1be` | [[BPR-0359]] | five byte-pinned files stop changing under a CRLF checkout |
| `acb8295` | [[BPR-0350]] | contained tests can create and use a temporary root |
| `f372f83` | trackers | the failure taxonomy corrected from three symptoms to six causes |
| `4214d78`, `555400c` | [[BPR-0363]] | the preset contract asserts the generator the preset declares |
| `4ea0c0c` | [[BPR-0361]] | the live-mutation guard reads objects, because a linked MSVC image has no symbols |
| `acb36e5` | [[BPR-0350]] | the Dif tool's import boundary restored; that conversion reverted |
| `9c084bc` | [[BPR-0365]] | the Dif asset canon stops rejecting its own assets |
| `c55f2d7` | [[BPR-0366]] | the conditional-branch gate stops reading a call as a declaration |

Cards closed: [[BPR-0359]], [[BPR-0363]], [[BPR-0361]], [[BPR-0365]],
[[BPR-0366]]. Cards opened by the work: [[BPR-0360]], [[BPR-0362]],
[[BPR-0364]], [[BPR-0367]].

### The 13 that remain

| cause | tests | owner |
|---|---|---|
| the container cannot read any path ancestor | 4 SQLite storage, 2 RFC-0016 security | [[BPR-0362]] — **parked on a product decision** |
| no Windows symbol inspector for the composed host | `composed_host_verifier_contract` | [[BPR-0364]] |
| `git init` returns 128 inside the container — **the same ancestor-read denial as the row above, not a separate cause** | `operator_response_validation_contract` | [[BPR-0362]] — **parked on a product decision** |
| ~~the isolation verifier rejects the Windows runner~~ — stale attribution, see below | `test_network_isolation_structure_contract` | unmeasured |
| temporary root, inside the Dif tool's import boundary; and a PNG canon that may be zlib-version-dependent | `dif_assets_contract` | [[BPR-0350]] records the boundary |
| ~~undiagnosed~~ — **fixed after this table was written** | `architecture_boundary_contract` | [[BPR-0371]], closed at `8127b88` |
| ~~undiagnosed~~ — **fixed after this table was written** | `github_contract_fixture_security` | [[BPR-0372]], closed at `ffd1b3a` |
| ~~undiagnosed~~ — **never a Windows defect; failed everywhere** | `dependency_modernization_contract` | [[BPR-0373]], closed at `6e068a1`/`9050b93` |

### What is blocked and what is not

[[BPR-0362]] is the only card in this program waiting on a person. It is parked
with an operator decision packet, and it does not block the rest: four cards
closed while it stood.

Everything else that remains is ordinary diagnosis. Nothing in this program now
waits on hardware, which is the thing this card set out to establish.

### Gates

```
py -3 scripts/verify_documentation.py --root .        DOCUMENTATION_OK
py -3 tests/scripts/verify_documentation_tests.py     78 tests, OK
git diff --check 605aa56..HEAD                        clean
clang-format on the one C++ file this run changed     clean
ctest --preset core-only                              72/85
cmake --build --target coding-format-check            FAILS, pre-existing, [[BPR-0367]]
```

The format gate's failure is one file, `src/app/src/github_application.cpp`,
last touched before this program's base and untouched by it. [[BPR-0367]] carries
it, including the open question of whether the file is misformatted or the tool
version disagrees.

The Qt graph under the boundary is still not attempted, unchanged by any of this.

## The SQLite storage path, closed out — 2026-08-18

Three defects stood between Windows and a working epoch catalog, and none of
them was the AppContainer:

| card | defect | evidence |
|---|---|---|
| [[BPR-0368]] | `validate_ancestor_chain` doubled the root separator, so the retained chain never compared equal | instrumented; without it the chain fails, with it nothing does |
| [[BPR-0369]] | `SetFileInformationByHandle` does not honour `RootDirectory`; the publication rename needed a qualified name | probe across every parent access mask: 87 each time, qualified form OK |
| [[BPR-0370]] | three fixtures narrowed their roots only on POSIX, so Windows roots were never owner-private | `permissions` returns 0 and leaves `is_owner_private` false |

`open_state_directory` had **never** worked on Windows. It does now.

### The container's cost, re-measured

Stripping the launcher from each failing test and running the real command:

| | at the start of this program | now |
|---|---|---|
| passes uncontained | 1 of 13 | **4 of 13** |

The cost went **up** as the product was repaired, which is the opposite of what
the first measurement suggested and the correct reading now that the product
defects underneath are gone. `sqlite_epoch_catalog_behavior_contract`,
`sqlite_review_ledger_storage`, `sqlite_review_mutation_recovery_storage`, and
`test_network_isolation_structure_contract` all pass uncontained and fail
contained.

So [[BPR-0362]] is not a small decision that can be deferred indefinitely: it is
the single thing standing between four tests and green, and it is the only card
in this program waiting on a person.

The contained matrix still reads 72 of 85. That number understates the run:
four of the thirteen failures are now the boundary rather than the code.

## One of the thirteen was never diagnosed here — 2026-08-19

`test_network_isolation_structure_contract`'s row above named
`unsafe-fixture-root` and `profile-template-contains-network-allow` as its cause.
[[BPR-0347]] carries the measurement that retires that attribution: both strings
live only in `scripts/run_network_isolated_test.py`, both were reproduced and
bounded on macOS, and the only suite shape that could emit the first was replaced
by [[BPR-0349]] at `3b8964e` on 2026-08-17 — hours **before** this table and
BPR-0347's were written at `c55f2d7` and `f372f83` on 2026-08-18.

So the row was a symptom string carried forward from an older tree, not a
diagnosis. It is now `unmeasured`: the recorded cause cannot occur, and whether
the test still fails on Windows needs a fresh capture on that host.

This is the shape worth noticing rather than the single row. A failure taxonomy
copied forward across runs will keep a cause that a later commit removed, and
nothing on this board re-checks one.

### So the other rows were re-checked the same way, and two more moved

Applying that method to the remaining `undiagnosed` rows, from macOS, without a
Windows host:

- **`architecture_boundary_contract` is stale.** BPR-0347 guessed it was the
  CRLF class "through a different door" and labelled the guess unmeasured. The
  guess was right and it has already been fixed:
  `tests/scripts/architecture_boundary_tests.py:1375` now writes every fixture
  with `newline=""`, landed by [[BPR-0371]] at `8127b88` on 2026-08-18 19:51 —
  after both this table (`c55f2d7`, 11:05) and BPR-0347's (`f372f83`, 10:37).
  BPR-0371 is in `60-done/` with a reverified 119-test run.
- **`operator_response_validation_contract` was mis-filed rather than
  undiagnosed.** Its cause is real and already known: `d348be9` records that
  `git init` returns 128 with "unable to get current working directory:
  Permission denied" because git resolves its working directory at start-up and
  that walks the unreadable ancestor chain. That is exactly [[BPR-0362]]'s
  cause, and BPR-0362 already names this test at `:370`. It belongs in that
  bucket, which makes BPR-0362 worth **seven** tests rather than six.

Both were verified against the live tree rather than taken from the reports that
found them.

The last two resolved the same way:

- **`github_contract_fixture_security` is stale.** [[BPR-0372]] — "repair the
  fixture contract's two Windows failures" — closed it at `ffd1b3a` on
  2026-08-18 19:54, after this table. `.gitattributes` now pins
  `tests/fixtures/github/**/*.json` and `*.md` to `eol=lf` and cites that card.
- **`dependency_modernization_contract` was never a Windows defect.**
  [[BPR-0373]] opens by saying so outright: it "fails everywhere", because
  `tests/scripts/dependency_modernization_tests.py:115` asserted a Qt policy
  `fbf0f18` had deliberately replaced. Closed at `6e068a1` and `9050b93` on
  2026-08-19. It was *found* during the Windows program and recorded so it would
  not be mistaken for one — and this table mistook it for one anyway.

`dif_assets_contract` is the only row not re-checked here; [[BPR-0350]] and
[[BPR-0378]] hold its two halves.

### What the table actually says now

Of the thirteen, **five rows are gone**: three stale, one never platform-specific,
one merged into [[BPR-0362]]. Not one of them needed a Windows session — they
needed the taxonomy read against the commits that landed after it, which is work
any host can do.

That changes what the next Windows session should do first. The remaining real
work is [[BPR-0362]]'s decision, now worth **seven** tests rather than six, plus
[[BPR-0364]] and a fresh capture of the two rows this pass could only mark
unmeasured. Re-running the suite to find out what still fails is now more
valuable than diagnosing any single row, because the last measurement is a day
old and five of its thirteen entries did not survive contact with the log.

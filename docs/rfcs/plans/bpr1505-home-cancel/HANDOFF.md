# BPR-1505 Home cancellation alignment checkpoint

Assignment: milestone `home-cancel-alignment`, plan `bpr1505-home-cancel`,
mandate `owner-testing-note-20261014-r1`: "Put cancel on the left of changes
etc on home so it doesn't move the alignment."

## Ownership and recovery

Exclusive source/tracker root:
`/Users/jaeger/.codex/worktrees/94c94eaf-62c4-4a7e-bc48-6b5471b7aea7/BetterPullReview`,
branch `bpr-1505-home-cancel`. Coordinator owns main integration elsewhere.
The original implementer's runtime disappeared; the coordinator reconciled its
absence before recovery. This recovery preserved its three dirty product/test
files, plan, claim and untracked binding checkpoint. No other writer in this
checkout was found. Generic CMake/Ninja processes were inspected by PID/cwd;
they belonged to main and BPR-1506, and were left alone.

Baseline `3e8793add`; plan `8c2ae58bb`; claim `b970d67bf`.
Product candidate `90958011c` changes only:

- `src/adapters/qt/src/home_row_delegate.cpp`
- `tests/adapters_qt/home_list_tests.cpp`
- `tests/end_to_end/home_open_feedback_public_path_tests.cpp`

Coordinator-authorized metadata commit `5a105579c` was cherry-picked as
`e0048271f`; it adds the separate BPR-1521 card/reservation and current counter.
It changes no product input. Use `git log` to identify the later acceptance and
handoff commits; the verified product candidate remains `90958011c`.
Canonical finish commit `d6b33559d` moves the card to Done and records all three
passing criteria. `completion.json` is bound to that exact revision; its
checkpoint check returned `action: stop` with quiescent ownership. The following
receipt commit changes only continuation documentation.

## Result and review

The idle row action keeps its original reservation. A separate measured opening
action sits before the Reviewing tag, account chip and facts. Opening, Cancel,
Retry and idle retain the same trailing-column positions. The shared row layout
serves both rendering and the actual button's geometry. Cancellation dispatch
is unchanged. The frontend-design skill was loaded; the design follows the
owner's precise placement instruction and inherits the native font, palette and
control style.

Self-review covered the narrow product diff, idle geometry equivalence, empty
action handling, existing tag/chip spacing, button measurement and text wrapping.
No required human, independent or native-platform acceptance is specified for
this card. No attended session or server was created.

## Verification

- Focused build: `cmake --build --preset dev --target test_home_open_feedback_public_path test_better_pull_review_qt_home_list_tests --parallel 2` passed.
- Earliest public path: `ctest --preset dev --output-on-failure -R '^application_home_open_feedback_public_path$' --parallel 1` passed in 60.29 seconds.
- After final assertion refinements and rebuild, the same network-isolated public
  course passed with `--render-directory` and exit 0. It exercises Home's actual
  double-click, delayed read, Cancel widget/wire cancellation and Retry widget.
  The command used `QT_QPA_PLATFORM=offscreen`,
  `BPR_AUTOMATED_TEST_BOUNDARY=1` and
  `python3 -I -S scripts/run_network_isolated_test.py --sandbox-exec /usr/bin/sandbox-exec --profile-template cmake/sandbox/macos-network-deny.sb --loopback-port-count 0 -- build/dev/tests/end_to_end/home_open_feedback_public_path --render-directory build/dev/bpr1505-renders`,
  with the script, profile, executable and output resolved absolutely inside
  this checkout.
- `ctest --preset dev --output-on-failure -R '^qt_home_list_scale_(100|125|200)$' --parallel 1`: 3/3 passed (21.11, 20.55 and 22.12 seconds).
- Canonical `_coding-standard/scripts/format.py --dry-run` on the three changed
  C++ files passed with clang-format 22.1.8; `git diff --check` passed.
- Focused `_coding-standard/scripts/tidy.py --build-dir build/dev --paths`
  on all three changed C++ files passed. The public test's existing
  `reviewing_course` local naming warning remains; no new finding was emitted.
- `python3 tests/scripts/verify_documentation_tests.py`: 67 tests passed.
- Scoped `issue_board.py validate --include BPR-1505 --json`: valid.
- `workflow_preflight.py` reports ready/ready_pinned; no registration was redone.
  It does not certify host loading.
- Full documentation gate first reported the branch's stale counter. After the
  authorized metadata import it reports only BPR-1521's prerequisite BPR-1520
  still Ready on this older baseline. Main integration owns that dependency
  reconciliation and the final global documentation pass.

Local renders are intentionally untracked under
`build/dev/bpr1505-renders/`. Inspected light/dark idle, waiting and failed PNGs
show Cancel/Retry before the facts; public assertions bind those positions to
the measured rendered row. They are offscreen layout evidence on macOS arm64,
Qt 6.11.2, at the course's 1100 × 760 window. Scale suites exercise 1/1.25/2.
These observations do not claim native desktop appearance acceptance.

## Continuation

The card is canonically Done, the completion observation is checked and all
build, test and tidy commands have exited. No review server, protected attended
candidate, background writer or task-owned temporary process remains. Build
outputs and inspection renders stay under this checkout's ignored build tree
for optional inspection.

Coordinator may cherry-pick the product and bookkeeping commits onto main,
omitting the already present metadata import, and run the integrated
documentation gate. No further issue is assigned to this implementer.

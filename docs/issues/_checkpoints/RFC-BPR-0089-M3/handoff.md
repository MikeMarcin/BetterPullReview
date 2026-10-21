# RFC-BPR-0089 M3 checkpoint: split per-commit diffs and rematch

Assignment: [[BPR-1693]], then milestone M3 of RFC-BPR-0089 (revision r3),
from the RFC implementation coordinator, over main `111235987`. Branch
`worktree-agent-a18ca971c2a750ecd`, rebased onto main `e9d5297ac` (after
BPR-1694). Code candidates: [[BPR-1693]] `51106f759`, M3 `2f3b95815` and
`dfea666ec` (the Qt test's renders written through Qt, as main's rebased change
to that test does). The
commits that close the cards and add this checkpoint change only
documentation. New issue IDs are provisional (`BPR-1695` to `K3`); the
coordinator allocates them on main. No card's metadata carries a
`provisional_id` key.

`frontend-design:frontend-design` was loaded before the generated rows gained
Open diff, and before the split strip and the rematch offers were drawn. No
M3 criterion is a look-and-judge one, so nothing is parked on the owner and no
attended session was opened.

## Result

| RFC criterion | Status | Where |
|---|---|---|
| Generated row "opening one shows its diff on request" | pass, fixture through the shipped composition | [[BPR-1693]] |
| AC03 | pass, fixture and live (BPR-Test #114) | [[BPR-1696]] |
| AC06 | pass, fixture and live (BPR-Test #115); M1 named the gone commit | [[BPR-1697]] |
| AC05 (M3 gestures) | pass: no review mutation terminal; Viewed unchanged for 325 files in both live runs | [[BPR-1696]], [[BPR-1697]] |
| AC08 (M3 decisions) | pass under `core-only` | [[BPR-1695]] |
| AC07 | M4, not started | |

## Decisions recorded

- **The same change** is identical path and blob pairs. The RFC also allows
  identical diff text where it is computed within existing bounds; the
  chain's tree diffs compute none, so it is not compared. A headline and
  author match is offered only when no same change exists, labelled a guess.
- **A split file's row** is as far read as its least read per-commit diff;
  accepted only when every diff is. It stays in the walk until the reviewer
  leaves it: its actionability is never taken from one diff's receipt.
- **The strip** sits under the document tabs and over the diff, takes no room
  while hidden, and shortens its wording (elided headlines, hidden commits by
  ID, then IDs only) instead of widening the window.

## Live evidence

`tests/end_to_end/rfc0089_commit_filter_live_course.py` on MikeMarcin/BPR-Test,
signed in through gh as MikeMarcin (sparse review):

- #114 (`--prefix rfc0089-m3-split1`): the owner's shape, 325 files. Under
  Hide C2 `src/version.txt` read C1's and C3's own diffs, each equal to
  `git diff C^ C -- P` (endpoints, both blobs, every line), every blob in a
  commit of the chain, C2 named between. Show only C1, C2, C3 matched as
  before. `X-RateLimit-Remaining` 4966 then 4870.
- #115 (`--force-push --prefix rfc0089-m3-push1`): the course hid C3 and
  waited while the script force-pushed C3 reworded on C2 with its own tree.
  After Refresh the chip named C3 gone and offered exactly the reworded
  commit as the same change; nothing applied until chosen; the receipt was
  kept; after the choice the new commit was hidden. 4868 then 4699.

Both pull requests are closed and their branches deleted. Viewed state was
unchanged for all 325 files in each. The live runs used this tree before two
clang-tidy renames and one string build in the course binary, which change no
behavior.

## Verification on the candidate

- 34 focused dev rows pass on `2f3b95815`'s tree before the last rebase; after it the composed course, `qt_commit_filter_view` at 100 and 200 and both pin contracts pass again on `dfea666ec`: `core_commit_filter`,
  `core_generated_files`, `application_commit_filter_model`,
  `application_commit_filter_public_path` (five courses), the epoch host and
  contract, the workbench host, the snapshot observation, the Overview
  model, `qt_commit_filter_view` at 100 and 200, `qt_files_tree` (three),
  `qt_pull_request_overview` (five), `qt_workbench_window` at 100, 125 and
  200, `qt_drawer_parts` (three), `gh_cli_git_smart_transport`,
  `commit_filter_fixture_contract`, `github_contract_fixture_security`,
  `github_recorded_fixture_contract`, `architecture_boundaries`,
  `architecture_boundary_contract`, `test_network_isolation_manifest`,
  `command_correctness_registry_contract` and
  `application_review_round_trip_composition_contract`.
- Negative controls: without the composition's segments the split course
  fails at opening the file; without recording the hidden commit's changes
  the force-push course fails waiting for the same-change offer; with the
  open dispatch disconnected the [[BPR-1693]] course fails at Open diff.
- `core-only`: `core_commit_filter` and `core_generated_files` pass.
- `coding-format-check` passes with clang-format 22.1.8. clang-tidy with
  warnings as errors passes on all 14 changed translation units of M3 and the
  6 of [[BPR-1693]].
- The documentation gate passes apart from the provisional IDs.
- Command-correctness and round-trip composition pins resealed for each
  commit, with a note at the top of the contract.

## Changed paths (M3)

Core `commit_filter.hpp/.cpp`. Application `commit_filter_model`,
`commit_filter_projection.hpp`, `workbench_runtime_ports.hpp`,
`workbench_application_host`, `workbench_snapshot_observation`,
`epoch_application_host.cpp`. Qt `commit_filter_view`, `workbench_window`.
Composition `github_application.cpp`, fixture descriptor and enum. Fixture
`commit-filter-split-v1` and its builder. Tests: core, model, Qt, the composed
course, the live course and script, fixture contract and security registries,
pins.

## Open obligations and next action

- M4: comments from filtered diffs (AC07) and thread markers under filters.
  Before M4 is claimed, the RFC's engineering question asks how GitHub shows a
  review comment whose `commit_id` is an earlier commit and whose line later
  changed; measure it on BPR-Test.
- A split segment's own diff now exists, so M4's anchoring must also cover a
  per-commit diff (its right side is a shown commit, its left that commit's
  parent).
- [[BPR-1688]]: Find in files on the filtered set and restoring the filter
  with the window (also its recorded hidden-commit changes).
- Windows and Linux are unmeasured.

## Command state

No task-owned build, test or app process is running. Live clones and build
trees are removed at the end of the task.

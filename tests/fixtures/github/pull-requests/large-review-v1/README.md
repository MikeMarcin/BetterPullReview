# Synthetic large-review workload, version 1

All content and identities are synthetic. Nothing was fetched from GitHub and
none of the source files is executed. GitHub-shaped URLs identify fixture
comments only; they are not designated live test targets.

`recipe.json` defines 512 source files of eight lines, a 20,000-line comparison,
an 8,192-line generated table, a 4,096-byte binary file, 32 head revisions and
1,000 source-line review comments in ten pages of 100. The final comparison
changes all 515 files. The large file changes 20 separated lines; generated
content changes its first line. Every head after the first changes one distinct
source file. This gives a stable base, real nonempty history and heterogeneous
file content without embedding private data or a buildable third-party project.

The base is `878e9d7043aa7dcbee8ce1fecd6633e7c3eee4ff` and the latest head is
`3a8e16042ce05cae93fa7782aa9d013e49c71cbb`. `manifest.json` records every retained head, exact
base/head blob IDs and sizes, Git additions/deletions, pack bytes/hash and
comment page count/hash/continuation. Each comment anchors line one of a changed
source file at the latest head. No replies, credentials or mutation requests
are included. The comment pages have REST review-comment shapes; the manifest
supplies their explicit pagination sequence for the test-only composed replay.

Regenerate with the repository helper and an explicit installed Git path:

```sh
python3 scripts/build_large_review_fixture.py --git /absolute/path/to/git
```

The helper uses a private disposable bare repository, fixed authors/timestamps,
empty templates/hooks, disabled credential helpers and forbidden network
protocols. It creates objects via `fast-import`, validates them with Git fsck,
and writes a pack. Object identities are deterministic; compressed pack bytes
may differ between Git versions and are therefore pinned independently.
`--output` can generate a separate reviewable fixture directory.

The network-isolated `large_review_fixture_contract` imports the **stored** pack,
runs strict fsck, independently checks the linear revision graph and every
file/tree/blob/diff claim, and checks comment pagination and anchors. It also
rejects a wrong anchor, duplicate identity and corrupted pack even when their
manifest hashes are updated. The check does not use the generator to recreate
its acceptance observations.

`application_large_review_benchmark` serves the latest head through the ordinary
provider, installed Git and application selection path. Its test-only Glaze
replay converts the stored comments to 1,000 source threads and serves six REST
file pages plus GraphQL file/thread pages. All 29 account requests must match
exact compiled requests in order; production paging validates continuations.
`large_review_replay_contract` covers cursor/order/body refusal and preserves
the legacy strict single-page transport boundary.

The composed course verifies all files/comments and both exact 20,000-line
source panes, renders the full window, navigates changes, sends 1,000 native
wheel events after work settles, and checks final-selection precedence across
32 rapid selections with outstanding work. Run it locally with:

```sh
ctest --test-dir build/dev --output-on-failure -R '^application_large_review_benchmark$'
```

The network-denied test writes `large-review-measurement.json` and
`large-review-window.png` under the build's `tests/end_to_end/` directory. The
report includes platform/Qt/build/scale context, elapsed phases, 5 ms event-loop
intervals, work-overlap samples and cache/scroll counters. Timings are local
observations. BPR-0624 records the initial Mac measurements; BPR-0627 tracks
measured UI stalls. This is one application epoch, not 32 epoch refreshes.
Memory/pruning ceilings, scrolling during active work, native Windows/Linux
runtime evidence and justified responsiveness thresholds remain open. Smaller
retained workbench and epoch-model fixtures remain useful for focused checks.

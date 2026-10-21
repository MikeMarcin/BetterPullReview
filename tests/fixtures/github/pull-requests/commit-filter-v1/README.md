# Commit-filter fixture

Synthetic public-path course for RFC-BPR-0089 M1. No remote data or
credentials. `scripts/build_commit_filter_fixture.py` builds one first-parent
chain from the merge base `M`: `C1` changes `src/generator.in` and line 5 of
`src/app.cpp`, `C2` regenerates the 96 files under `gen/`, and `C3` changes
line 40 of `src/app.cpp` and edits `gen/item_007.txt` and `gen/item_008.txt`
by hand. It writes `construction.json` with GitHub's Files changed for
`M...C3` (98 files, one recorded page) and the three commits with their
parents, packs every object reachable from `C3`, verifies each commit's own
change, and generates the recorded read-only contract under
`tests/fixtures/github/contracts/v1/commit-filter-v1`, whose commit list
serves GitHub's `parents`. The committed artifacts are consumed directly;
ordinary builds regenerate nothing. The owner's hundreds of regenerated files
are the live BPR-Test course's.

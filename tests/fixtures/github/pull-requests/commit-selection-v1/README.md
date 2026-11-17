# Commit-selection fixture

Synthetic public-path course for [[BPR-2248]], the Commits tab's agent
prompts. No remote data or credentials. `scripts/build_commit_filter_fixture.py
--merges` builds commit-filter-v1's chain with a merge of the base branch in
it: `C1` changes `src/generator.in` and line 5 of `src/app.cpp`, `C2`
regenerates the 96 files under `gen/`, `X` merges `main` (which gained `B1`,
adding `docs/release-notes.md`) into the branch, and `C3` changes line 40 of
`src/app.cpp` and edits two generated files by hand. Every commit but the
merge has a message body. `B1` is the base's tip and the merge base, so Files
changed is `B1...C3` (98 files). The builder packs every object reachable from
`C3`, verifies the shape, and generates the recorded read-only contract under
`tests/fixtures/github/contracts/v1/commit-selection-v1`, whose commit list
serves GitHub's `parents`. The committed artifacts are consumed directly;
ordinary builds regenerate nothing.

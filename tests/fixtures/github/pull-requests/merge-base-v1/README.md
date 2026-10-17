# Merge-base fixture

Synthetic public-path course for [[BPR-1560]]. No remote data or credentials.
`scripts/build_merge_base_fixture.py` builds a three-commit graph: the merge
base `M`, the recorded base `T` (`base.sha`, the base branch moved on from `M`)
and the head `H` (from `M`). It writes `construction.json` with GitHub's Files
changed for `M...H`, packs every object reachable from `T` and `H`, verifies
that `T...H` would revert the base branch's lines and hide `b.txt`'s change, and
generates the recorded read-only contract under
`tests/fixtures/github/contracts/v1/merge-base-v1`. The fixture server answers
GitHub's compare with `M` as the merge base. The committed artifacts are
consumed directly; ordinary builds regenerate nothing.

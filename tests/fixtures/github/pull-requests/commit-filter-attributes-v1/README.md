# Commit-filter fixture with linguist-generated

Synthetic public-path course for RFC-BPR-0089 M2 ([[BPR-1692]]). No remote
data or credentials. `scripts/build_commit_filter_fixture.py --attributes`
builds commit-filter-v1's chain (`M`, then `C1` changing the source, `C2`
regenerating the 96 files under `gen/`, and `C3` editing the source and
`gen/item_007.txt` and `gen/item_008.txt` by hand), with two rule files
committed in `M`:

- `.gitattributes`: `gen/** linguist-generated`
- `gen/.gitattributes`: `item_007.txt -linguist-generated`

Neither changes in the pull request, so GitHub's Files changed for `M...C3`
is the same 98 files. At the head the rules mark 95 of `C2`'s paths as
generated and return `gen/item_007.txt` to normal; the builder checks that
with `git check-attr --source`. It writes `construction.json`, packs every
object reachable from `C3` and generates the read-only contract under
`tests/fixtures/github/contracts/v1/commit-filter-attributes-v1`. The
committed artifacts are consumed directly; ordinary builds regenerate
nothing.

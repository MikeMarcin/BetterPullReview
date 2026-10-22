# Commit-filter fixture for comments on a filtered diff

Synthetic public-path course for RFC-BPR-0089 M4. No remote data or
credentials. `scripts/build_commit_filter_fixture.py --comment` builds
commit-filter-v1's chain (`M`, then `C1` changing the source, `C2`
regenerating the 96 files under `gen/`, and `C3` editing the source and
`gen/item_007.txt` and `gen/item_008.txt` by hand), with one more file,
`src/notes.txt`, of 30 lines, shaped as BPR-Test #116, where GitHub was
measured: `C1` changes its lines 5, 15 and 25, and `C3` inserts two lines at
its top and changes `C1`'s lines 5 and 25 again. With `C3` hidden the file's
diff is `M` to `C1`: a comment on its line 15 goes to the head at line 17, and
one on its line 5 goes to `C1`. The review threads hold two already, as GitHub
reports them: one written on `C1` at line 25, which GitHub keeps there,
outdated, and one at the head's line 17. GitHub's Files changed for `M...C3`
is 99 files.

The builder writes `construction.json`, packs every object reachable from
`C3`, and generates the read-only contract under
`tests/fixtures/github/contracts/v1/commit-filter-comment-v1`. The committed
artifacts are consumed directly; ordinary builds regenerate nothing.

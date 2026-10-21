# Commit-filter fixture with a split file and a force push

Synthetic public-path course for RFC-BPR-0089 M3. No remote data or
credentials. `scripts/build_commit_filter_fixture.py --split` builds
commit-filter-v1's chain (`M`, then `C1` changing the source, `C2`
regenerating the 96 files under `gen/`, and `C3` editing the source and
`gen/item_007.txt` and `gen/item_008.txt` by hand), with one more file,
`src/version.txt`, that every commit changes: `C1` its version, `C2` its
stamp, `C3` its version again. Hiding `C2` splits it into `C1`'s and `C3`'s
own diffs. GitHub's Files changed for `M...C3` is 99 files.

The pack also holds `C3r`, `C3` reworded on `C2` with `C3`'s own tree. The
public-path descriptor serves it as the second head, after a refresh in the
seed phase: a force push whose new head carries exactly `C3`'s change. The
commit list's head is the served placeholder, so the one contract under
`tests/fixtures/github/contracts/v1/commit-filter-split-v1` serves both heads.
The builder writes `construction.json`, packs every object reachable from
`C3` and `C3r`, and generates that read-only contract. The committed artifacts
are consumed directly; ordinary builds regenerate nothing.

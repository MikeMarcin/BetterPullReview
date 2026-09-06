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
supplies their explicit pagination sequence for a future replay transport.

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

These are benchmark inputs. They do not demonstrate application latency,
responsiveness, resident memory, cache ceilings, provider paging or an accepted
hardware threshold. A following RFC-0013 slice must serve them through the
ordinary provider/installed-Git/application path and measure actual interaction
under load. Smaller retained workbench and epoch-model fixtures remain useful
for focused structural checks.

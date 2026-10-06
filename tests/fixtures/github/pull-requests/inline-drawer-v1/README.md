# Constructed inline-drawer pull request, version 1

Synthetic public test data, written for [[BPR-1290]]. Nothing was fetched from
GitHub, no identity is real, and none of the source is built or run.
`bpr-fixtures/inline-drawer#22` names no real repository.

The owner judges the inline Comments drawer against its clickable prototype,
[`BPR-1290-inline-drawer-v1`](../../../../../docs/design/BPR-1290-inline-drawer-v1/index.html),
whose code is a long file with several threads hanging from its lines. No
fixture had threads on a long file. This one is
[`conventional-comments-v1`](../../contracts/v1/conventional-comments-v1/manifest.json)
(the [`nested-folders-v1`](../nested-folders-v1/README.md) files with six
labelled threads) plus one long file, `src/ui/conversation_cards_view.cpp`: this
repository's `src/adapters/qt/src/conversation_cards_view.cpp` at `fbbabb3ef`,
1557 lines, the prototype's code verbatim. Its head is
[`sources/conversation_cards_view.cpp.txt`](sources/conversation_cards_view.cpp.txt);
its base is the head without the prototype's four added ranges (601-630,
656-673, 1129-1150, 1166-1188), which are illustrative.

The long file carries the prototype's eight threads, at lines 38 (resolved),
430, 656, 667, 927, 1080, 1176 and 1510, in the voices of the
conventional-comments people. The prototype's draft at line 1142 is a local
note, not a thread, so it is not here. Fourteen threads, 44 posts.

## Files and how they are made

- `construction.json` is written by `scripts/build_inline_drawer_fixture.py`,
  from the nested-folders construction and the long file's source.
- `objects.pack` is built from it by `scripts/build_recorded_pull_request_pack.py`:
  base `916aad158d0a7cd7aaf15d6ca64123c71deaea2d`, head
  `89df5e592e5db97a526a05a445735733395e4832`.
- `tests/fixtures/github/contracts/v1/inline-drawer-v1/` is written from it by
  `scripts/build_recorded_pull_request_contract.py`, and its review threads by
  the same fixture script's `--threads` step.

```sh
python3 scripts/build_inline_drawer_fixture.py tests/fixtures/github/pull-requests/inline-drawer-v1/construction.json
python3 scripts/build_recorded_pull_request_pack.py tests/fixtures/github/pull-requests/inline-drawer-v1/construction.json tests/fixtures/github/pull-requests/inline-drawer-v1/objects.pack
python3 scripts/build_recorded_pull_request_contract.py tests/fixtures/github/pull-requests/inline-drawer-v1/construction.json tests/fixtures/github/contracts/v1/inline-drawer-v1
python3 scripts/build_inline_drawer_fixture.py --threads tests/fixtures/github/contracts/v1/inline-drawer-v1
```

`github_recorded_fixture_contract` regenerates the construction and the served
tree, threads included, and compares bytes. `github_contract_fixture_security`
holds the served tree to the same per-operation fields as `conventional-comments-v1`.

## Served

`--github-fixture inline-drawer-v1` serves it on github.com's host profile,
signed in as `fixture-author`. The attended launch opens it with
`--attended-launch --attended-open-file src/ui/conversation_cards_view.cpp`,
which prints `ATTENDED_FILE_SHOWN:src/ui/conversation_cards_view.cpp` once the
file is shown in Read. It has no public-path row of its own: the course's
presentation stage is not written for a pull request with threads, as
`conventional-comments-v1` has none either.

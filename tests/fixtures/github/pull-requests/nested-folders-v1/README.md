# Constructed nested-folders pull request, version 1

Synthetic public test data, written for [[BPR-1213]]. Nothing was fetched from
GitHub, no identity is real, and none of the source is built or run.
`bpr-fixtures/nested-folders#21` names no real repository.

The owner rejected the first attended collapse question because the recorded
`uiforetw-74` pull request has one folder and no subfolder, so hiding and
showing a folder's files could not be judged on it. This pull request has 17
changed files in 12 folders that hold files:

| Folder row | Files |
|---|---|
| `/` | `README.md` |
| `docs` | `guide.md` |
| `docs/api` | `reference.md` |
| `docs/api/v2` | `endpoints.md` (added) |
| `src` | `main.cpp` |
| `src/engine` | `renderer.cpp`, `renderer.h` |
| `src/engine/shaders` | `basic.frag`, `basic.vert` |
| `src/engine/shaders/post` | `bloom.frag`, `tonemap.frag` (both added) |
| `src/ui/widgets` | `button.cpp`, `button.h`, `slider.cpp` (added) |
| `tests/engine` | `renderer_tests.cpp` |
| `tests/ui/widgets` | `button_tests.cpp` (added) |
| `tools/build/scripts` | `package.py` |

`src/ui`, `tests/ui`, `tools` and `tools/build` hold no file of their own, and
`src`, `src/engine` and `src/engine/shaders` are each a folder with files and
subfolders with files.

## Files and how they are made

- `construction.json` is written by `scripts/build_nested_folders_fixture.py`,
  which holds every file's base and head text. It has the shape a recording
  has, carries `constructed` instead of `recorded_from` and `recorded_on`, and
  names no URL.
- `objects.pack` is built from it by `scripts/build_recorded_pull_request_pack.py`:
  base `c58160c2ffe27d7cf5e6759d78b80f023547c789`, head
  `b20cb443428212543f5911e091a0ffeb407d23cf`, from the builder's fixed identity
  and date. The builder refuses to write a pack whose diff does not reproduce
  every file's additions and deletions.
- `tests/fixtures/github/contracts/v1/nested-folders-v1/` is written from it by
  `scripts/build_recorded_pull_request_contract.py`.

```sh
python3 scripts/build_nested_folders_fixture.py tests/fixtures/github/pull-requests/nested-folders-v1/construction.json
python3 scripts/build_recorded_pull_request_pack.py tests/fixtures/github/pull-requests/nested-folders-v1/construction.json tests/fixtures/github/pull-requests/nested-folders-v1/objects.pack
python3 scripts/build_recorded_pull_request_contract.py tests/fixtures/github/pull-requests/nested-folders-v1/construction.json tests/fixtures/github/contracts/v1/nested-folders-v1
```

`github_recorded_fixture_contract` regenerates the construction and the served
tree into a temporary directory and compares bytes, re-diffs every file's
blobs against its counts, and checks the pack header. `github_contract_fixture_security`
holds the served tree to the same per-operation fields as `uiforetw-74`.

## Served

`--github-fixture nested-folders-v1` serves it through the public path on
github.com's host profile, signed in as `fixture-author`. Paste
`https://github.com/bpr-fixtures/nested-folders/pull/21` into Home's Open field.
`application_github_public_path_nested_folders_v1` runs that path, and
`application_nested_overview_folders_public_path` drives the composed Overview:
its flat folder rows, and a click or Left/Right on a folder row hiding and
showing only that folder's own files.

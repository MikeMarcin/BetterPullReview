# Constructed long-lines pull request, version 1

Synthetic public test data, written for [[BPR-1334]]. Nothing was fetched from
GitHub, no identity is real, and none of the source is built or run.
`bpr-fixtures/line-details#34` names no real repository.

Line details shows the current line's base version above its head version,
full width and wrapped. No other fixture had a changed line longer than 148
characters, so this pull request's changes are long lines:

| File | What changed |
|---|---|
| `config/flags.json` | One minified line; two values in the middle of it |
| `docs/export.md` | Unwrapped paragraphs: a clause rewritten, one paragraph replaced |
| `src/report/query.cpp` | A long SQL literal and a long call gained columns and arguments |
| `web/static/app.min.js` | One minified line of about twelve thousand characters |

## Files and how they are made

- `construction.json` is written by `scripts/build_line_details_fixture.py`,
  which holds every file's base and head text and reuses the nested-folders
  builder's helpers.
- `objects.pack` is built from it by `scripts/build_recorded_pull_request_pack.py`:
  base `37b3ffda05bf3693487e4f92a5a1118d8647bede`, head
  `a7603d7f2fe157ae72a0d4e13dada90fab83b31a`.
- `tests/fixtures/github/contracts/v1/line-details-v1/` is written from it by
  `scripts/build_recorded_pull_request_contract.py`.

```sh
python3 scripts/build_line_details_fixture.py tests/fixtures/github/pull-requests/line-details-v1/construction.json
python3 scripts/build_recorded_pull_request_pack.py tests/fixtures/github/pull-requests/line-details-v1/construction.json tests/fixtures/github/pull-requests/line-details-v1/objects.pack
python3 scripts/build_recorded_pull_request_contract.py tests/fixtures/github/pull-requests/line-details-v1/construction.json tests/fixtures/github/contracts/v1/line-details-v1
```

## Served

`--github-fixture line-details-v1` serves it through the public path on
github.com's host profile, signed in as `fixture-author`. Paste
`https://github.com/bpr-fixtures/line-details/pull/34` into Home's Open field.

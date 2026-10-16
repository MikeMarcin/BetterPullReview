# Constructed intraline pull request, version 1

Synthetic public test data, written for [[BPR-1537]]. Nothing was fetched from
GitHub, no identity is real, and none of the source is built or run.
`bpr-fixtures/intraline#37` names no real repository.

The owner judges how a changed line's emphasis reads. Every changed line here
is a one-for-one replacement between unchanged lines, so each pairs as similar
and carries intraline emphasis:

| File | What changed |
|---|---|
| `config/review.json` | A number, a revision hash, a string value and a word's case |
| `docs/review-guide.md` | A changed word, a rewritten clause, an added phrase, a link's folder, a replaced sentence, shortcut symbols |
| `locale/messages.txt` | Accented, CJK, symbol and emoji text |
| `src/review/comparison.cpp` | Renames, an inserted and a removed argument, edited strings, operators, spacing, numbers, swapped operands, a rewritten line |
| `tools/report.py` | A renamed function, an added argument, an edited f-string |

## Files and how they are made

- `construction.json` is written by `scripts/build_intraline_fixture.py`,
  which holds every file's base and head text and reuses the nested-folders
  builder's helpers.
- `objects.pack` is built from it by `scripts/build_recorded_pull_request_pack.py`:
  base `a3f915b5baa6b05b2ffc1ac6502b218b5d16bec7`, head
  `d7e2f04de93e5e51097627e58093a42b68de36ce`.
- `tests/fixtures/github/contracts/v1/intraline-v1/` is written from it by
  `scripts/build_recorded_pull_request_contract.py`.

```sh
python3 scripts/build_intraline_fixture.py tests/fixtures/github/pull-requests/intraline-v1/construction.json
python3 scripts/build_recorded_pull_request_pack.py tests/fixtures/github/pull-requests/intraline-v1/construction.json tests/fixtures/github/pull-requests/intraline-v1/objects.pack
python3 scripts/build_recorded_pull_request_contract.py tests/fixtures/github/pull-requests/intraline-v1/construction.json tests/fixtures/github/contracts/v1/intraline-v1
```

## Served

`--github-fixture intraline-v1` serves it through the public path on
github.com's host profile, signed in as `fixture-author`. Paste
`https://github.com/bpr-fixtures/intraline/pull/37` into Home's Open field.

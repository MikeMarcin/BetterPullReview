# Constructed promo pull request, version 1

Synthetic public test data, written for [[BPR-1676]]. Nothing was fetched from
GitHub, no identity is real, and none of the source is built or run.
`bpr-demo/lantern#512` names no real repository.

This is the pull request the promotional captures show: a small game engine
moving its frame pacing from a sleep loop to the display's vblank. Its code
changes in place across five files and adds a test file, and eight review
threads with Conventional Comments labels sit on the changed lines. The viewer
is `sam`, reviewing `mira`'s pull request; `read_state.json` leaves six posts
unread.

## Files and how they are made

- `construction.json` and `read_state.json` are written by
  `scripts/build_promo_fixture.py`.
- `objects.pack` is built from the construction by
  `scripts/build_recorded_pull_request_pack.py`: base
  `c8d078714e30667585ceffdf7f4c20d9131cd4a2`, head
  `069cbf66992b253bf0bd73ed3e46e4a8019a0dc8`.
- `tests/fixtures/github/contracts/v1/promo-v1/` is written by
  `scripts/build_recorded_pull_request_contract.py`, and its review threads
  and viewer by the fixture script's `--threads` step.

```sh
python3 scripts/build_promo_fixture.py tests/fixtures/github/pull-requests/promo-v1/construction.json
python3 scripts/build_recorded_pull_request_pack.py tests/fixtures/github/pull-requests/promo-v1/construction.json tests/fixtures/github/pull-requests/promo-v1/objects.pack
python3 scripts/build_recorded_pull_request_contract.py tests/fixtures/github/pull-requests/promo-v1/construction.json tests/fixtures/github/contracts/v1/promo-v1
python3 scripts/build_promo_fixture.py --threads tests/fixtures/github/contracts/v1/promo-v1
```

`github_recorded_fixture_contract` regenerates the construction and the served
tree, threads included, and compares bytes.

## Served

`--github-fixture promo-v1` serves it on github.com's host profile, signed in
as `sam`. `scripts/capture_promo_assets.py` opens it with
`--attended-launch --attended-open-file src/render/frame_pacer.cpp --attended-read-state`.

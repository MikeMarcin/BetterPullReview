# Constructed Conversations pull request, version 1

Synthetic public test data, written for [[BPR-1293]]. Nothing was fetched from
GitHub, no identity is real, and none of the source is built or run.
`bpr-fixtures/conversations#418` names no real repository.

The owner judges Conversations, the pull request's finder, against its
clickable prototype,
[`BPR-1290-inline-drawer-v2`](../../../../../docs/design/BPR-1290-inline-drawer-v2/index.html),
whose made-up pull request has twenty-two conversations in ten files. This is
that pull request: the prototype's ten files, verbatim from its `code.js`
(this repository's own sources at the revisions named there), and its
conversations, extracted verbatim from the prototype's `C0` into
[`sources/conversations.json`](sources/conversations.json). The prototype's
draft at line 1142 is a local note, not a thread, so it is not here:
twenty-one threads, 91 posts. Line 667 of `conversation_cards_view.cpp` has
branching-thread-v1's shape: three branches off its first comment, the third
forking after its third reply into a continuation and an aside, each with
replies of its own. The changed-line ranges are the prototype's (illustrative).

A post's age in the prototype decides its posting order, oldest first, so the
deep thread's posts are in the order they were written, as GitHub keeps them.
The prototype's new posts are then the last posts of every conversation, and
`read_state.json` names, for each thread, the last post a reader has seen.

## Files and how they are made

- `sources/conversations.json` is the prototype's `C0`, extracted once with
  Node from the prototype's own script; edit the prototype, not this copy.
- `construction.json` and `read_state.json` are written by
  `scripts/build_conversations_fixture.py`.
- `objects.pack` is built from the construction by
  `scripts/build_recorded_pull_request_pack.py`: base
  `9a0c46377e514e4760639ffa972cd5812b30c5b2`, head
  `f05373e1184e7067994839ecbe6e2db7a933fa97`.
- `tests/fixtures/github/contracts/v1/conversations-v1/` is written by
  `scripts/build_recorded_pull_request_contract.py`, and its review threads by
  the fixture script's `--threads` step.

```sh
python3 scripts/build_conversations_fixture.py tests/fixtures/github/pull-requests/conversations-v1/construction.json
python3 scripts/build_recorded_pull_request_pack.py tests/fixtures/github/pull-requests/conversations-v1/construction.json tests/fixtures/github/pull-requests/conversations-v1/objects.pack
python3 scripts/build_recorded_pull_request_contract.py tests/fixtures/github/pull-requests/conversations-v1/construction.json tests/fixtures/github/contracts/v1/conversations-v1
python3 scripts/build_conversations_fixture.py --threads tests/fixtures/github/contracts/v1/conversations-v1
```

`github_recorded_fixture_contract` regenerates the construction and the served
tree, threads included, and compares bytes. `github_contract_fixture_security`
holds the served tree to the same per-operation fields as `inline-drawer-v1`.

## Served

`--github-fixture conversations-v1` serves it on github.com's host profile,
signed in as `fixture-author`. The attended launch opens it with
`--attended-launch --attended-open-file src/adapters/qt/src/conversation_cards_view.cpp --attended-read-state`,
which records `read_state.json` through the review ledger, prints
`ATTENDED_READ_STATE:18` (three threads are not seen at all) and then
`ATTENDED_FILE_SHOWN:src/adapters/qt/src/conversation_cards_view.cpp` once the
file is shown in Review.

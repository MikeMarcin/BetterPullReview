---
schema: "implement-rfc.card.v1"
id: "BPR-0892"
title: "The files page cap does not account for the patch payload GitHub always sends, and no fixture carries one"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable github+git partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0892: The files page cap does not account for the patch payload GitHub always sends, and no fixture carries one

## Defect

`list_pull_request_files` pages are bounded at 4 MiB
(`src/adapters/github/src/github_paged_read.hpp:21`, and the same cap in
`native_https_transport.hpp:26`), and the parser reads only four keys —
`additions`, `deletions`, `filename`, `status`, `previous_filename`
(`github_paged_read.cpp:2471-2485`). The ceiling is 30 pages of 100
(`github_read_contract.cpp:743-744`).

GitHub's REST files endpoint **always includes `patch`**, and offers no way to
ask it not to. So the product pays for a large string per file that it then
discards, against a 4 MiB page budget. One large lockfile or generated-file
diff, or a hundred moderate patches, can exceed it — and the result is
`response_limit`, an atomic failure of the **whole pull request read**, not a
degraded one.

**No fixture exercises this.** The recorded bodies contain only the four keys
the parser wants:
`tests/fixtures/github/contracts/v1/uiforetw-74/responses/files.json` is 530
bytes and `.../github-com/responses/files.json` is 108. So the round-trip and
public-path courses have never seen a realistically shaped files page, and the
cap has never been tested against one.

The reviewer did not verify this live, by mandate. The size claim is from
GitHub's documented response shape, not from an observed failure — so the
first task is measurement, not a fix.

## Scope

- Measure first: fetch a large public pull request from a station and record
  the real page sizes, with and without large generated diffs. That decides
  whether this is theoretical or already reachable.
- Then pick the remedy the measurement supports: raise the per-operation page
  bound, lower `per_page` for the files operation, or stream and discard
  `patch` as it parses. Lowering `per_page` costs round trips; raising the cap
  costs memory; the third is the only one that removes the waste, and it may
  not be worth it.
- Record a realistic fixture either way, so the courses stop passing on
  530-byte pages.

## Acceptance

- The card records measured page sizes from a real large pull request.
- A pull request of that shape loads, or is refused with a message that says
  the page was too large rather than a bare `response_limit`.
- A fixture with realistic `patch` payloads is in the recorded contracts and is
  used by at least one course.

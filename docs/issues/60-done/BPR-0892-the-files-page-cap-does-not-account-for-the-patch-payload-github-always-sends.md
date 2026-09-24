---
schema: "implement-rfc.card.v1"
id: "BPR-0892"
title: "The files page cap does not account for the patch payload GitHub always sends, and no fixture carries one"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable github+git partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
acceptance: [{"criterion":"The card records measured page sizes from a real large pull request","outcome":"pass","revision":"f3d19c2a","evidence":{"kind":"observation","detail":"Read-only anonymous files-page reads, sizes only: facebook/react#9101 largest page 747,548 B; microsoft/TypeScript#51387 (656 files, +282,700 -284,842) largest page 1,176,273 B with 1,048,253 B of patch and 61 files sent without patch; NixOS/nixpkgs#322537 (21,294 files) 30 pages then [], 12,665,213 B in all, largest page 723,104 B. The largest patch seen was 100,612 B.","path":"docs/issues/60-done/BPR-0892-the-files-page-cap-does-not-account-for-the-patch-payload-github-always-sends.md#measurement-ac1"}},{"criterion":"A pull request of that shape loads, or is refused with a message that says the page was too large rather than a bare response_limit","outcome":"pass","revision":"f3d19c2a","evidence":{"kind":"command","detail":"github_paged_read test_measured_files_page_shapes, with production limits: a 1,243,605 B first page shaped like TypeScript page 6 loads 101 files, and 3,000 files in 30 pages of about 781 KB (23,443,590 B in all) load. The served sizes are asserted too. With the page limit lowered to 1 MiB, or the aggregate to 16 MiB, the row fails.","path":"docs/issues/60-done/BPR-0892-the-files-page-cap-does-not-account-for-the-patch-payload-github-always-sends.md#a-pull-request-of-that-shape-loads-ac2"}},{"criterion":"A fixture with realistic patch payloads is in the recorded contracts and is used by at least one course","outcome":"pass","revision":"f3d19c2a","evidence":{"kind":"command","detail":"contracts/v1/uiforetw-74/responses/files.json now carries the real patch, sha and changes GitHub served (7,723 B of patches, CRLF, tabs, two links), generated from recording.json. The validator admits and checks these keys; the security test treats links inside a patch as reviewed content. application_github_public_path_uiforetw_74 passes with it, where it failed before the validator change. application_uiforetw_selected_diff_contract reads it with every fixture marker OK, and stays red only on BPR-1036's two lines. Linux and Windows confirmation runs are named on the card.","path":"docs/issues/60-done/BPR-0892-the-files-page-cap-does-not-account-for-the-patch-payload-github-always-sends.md#a-recorded-contract-with-real-patches-served-by-a-course-ac3"}}]
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

## Measured: the cap holds with room to spare, and a course now serves real patches (bpr-main-mac, Mac batch 5, 2026-09-23)

Station `bpr-main-mac` (Apple Silicon, AppleClang, Qt 6.11.2, `build/dev`
Debug), claimed at `fd223974`.

**Re-check at HEAD.** The limits are as described. They moved a few lines:
`github_paged_read.hpp:24` (4 MiB a page, 64 MiB in all),
`native_https_transport.hpp:26` (4 MiB), `github_read_contract.cpp:769-770`
(30 pages for the files operation), and the gh CLI transport's standard output
(4 MiB, `gh_cli_transport.cpp:72`). Two claims were partly stale:

- The paged-read contract's own synthetic files
  (`github_paged_read_tests.cpp`) already carry every key GitHub sends,
  `patch` included, and a `large_ignored_patch` scenario sends a 500 KiB
  patch. What was missing was a *recorded* contract with a patch, and so a
  course that serves one.
- That gap was on purpose. The recorded-fixture validator
  (`validate_files_fixture` in `github_read_contract.cpp`) admitted exactly
  four keys. `github_contract_fixture_security` pinned the same four, and the
  generator (`scripts/build_recorded_pull_request_contract.py`) wrote only
  those. So no recorded page *could* hold a patch.

### Measurement (AC1)

Unauthenticated, read-only `GET /repos/{owner}/{repo}/pulls/{n}/files?per_page=100&page=k`
with `Accept-Encoding: identity`, so the sizes are the bodies the product
would parse. Only sizes were kept, never content (script and logs under the
session scratchpad, not committed). 52 of the hour's 60 anonymous requests.
nixpkgs' total includes the 2-byte `[]` of page 31.

| Pull request | Shape | Pages | Largest page | Pages in all | Largest `patch` | Files sent without `patch` |
| --- | --- | --- | --- | --- | --- | --- |
| facebook/react#9101 (Prettier adoption) | 328 files, +12,794 −11,435 | 4 | 747,548 B | 1,957,898 B | 59,416 B | 4 |
| microsoft/TypeScript#51387 (conversion to modules) | 656 files, +282,700 −284,842 | 7 | **1,176,273 B** | 6,839,105 B | 100,071 B | 242 |
| NixOS/nixpkgs#322537 (treewide reformat) | 21,294 files, +706,935 −433,885 | 30, then `[]` | 723,104 B | **12,665,213 B** | 100,612 B | 1 |

- **The largest page measured is 1.18 MB, a quarter of the 4 MiB cap.** The
  `patch` field is most of every page: 88% of react's first page, and
  1,048,253 of the 1,176,273 bytes on TypeScript's page 6.
- **GitHub bounds the patch itself.** No patch exceeded about 100 KB. On
  TypeScript's pages 2 to 6, between 39 and 66 files arrived with no `patch`,
  while each page's patches totalled 0.97 to 1.05 MB. That is consistent
  with GitHub keeping to a per-response patch budget near 1 MiB. It is an
  observation, not a documented limit.
- **GitHub stops at 3,000 files.** nixpkgs' page 31 returned `[]`. So the
  product's 30-page ceiling is GitHub's own, and the largest measured files
  read, 12.67 MB, is a fifth of the 64 MiB aggregate.

**Remedy: none for the cap.** Measured shapes never come near it. Raising the
bound, lowering `per_page` or discarding `patch` while parsing would all cost
something to guard a page GitHub does not appear to send. The card's
"hundred moderate patches" case does not reach it: a hundred 10 KB patches
make about 1 MB.

### A pull request of that shape loads (AC2)

`test_measured_files_page_shapes` (row `github_paged_read`) runs two
scenarios through `perform_paged_github_read` with the **production** limits:

| Scenario | What is served | Result |
| --- | --- | --- |
| `measured_largest_files_page` | 101 files. The first page has TypeScript page 6's shape: one 100,071-character patch, 38 more that fill the page, and 61 files with no `patch`. The page is 1,243,605 bytes, at least as large as the measured 1,176,273 | Loads, 101 files |
| `measured_file_cap_pages` | 3,000 files in 30 pages, each 780,761 to 781,601 bytes, heavier than nixpkgs' heaviest. 23,443,590 bytes in all, against the measured 12,665,213 | Loads, 3,000 files |

The row asserts the served sizes as well as the load, so a smaller fixture
cannot pass for a large one. With the page limit lowered to 1 MiB, the first
scenario fails. With the aggregate limit lowered to 16 MiB, the second fails
(the read stops before page 30). Both breaks were temporary header edits,
restored and re-hashed.

The "refused with a message that says the page was too large" branch does
not arise, because a page of the measured shape loads.

### A recorded contract with real patches, served by a course (AC3)

- `scripts/build_recorded_pull_request_contract.py` now writes each file's
  `changes`, `patch` and `sha` from `recording.json`, which has always held
  them as GitHub served them. The committed
  `tests/fixtures/github/contracts/v1/uiforetw-74/responses/files.json` is
  regenerated from it. It is the only file in that tree that changed, and
  `github_recorded_fixture_contract` confirms the tree matches the generator.
  The five real patches total 7,723 bytes, with CRLF source lines, tabs and
  two msdn.microsoft.com links.
- `validate_files_fixture` (`github_read_contract.cpp`) now requires the four
  keys, and admits `previous_filename`, `changes`, `patch` and `sha` and
  nothing else. `changes` must equal additions plus deletions, as GitHub
  guarantees. `patch` must be a string, and `sha` an object id. URL fields
  such as `blob_url` are still refused, since the fixture policy forbids
  them. New rows in `github_read_contract` accept a file with all three keys
  and one without a patch. They refuse a wrong `changes` total, a string
  `changes`, a numeric `patch`, a malformed `sha`, and a `blob_url`. Against
  the old validator the first new row fails:
  `FAILED: a file carrying GitHub's sha, changes and patch is a valid fixture`.
- `github_contract_fixture_security` pins uiforetw-74's files fields as the
  seven keys. It treats links **inside a `patch`** as reviewed content, the
  rule `recorded_pull_request_fixture_tests.py` already applies to the
  recording. Only the URL-host scan skips them. Credentials, the canary,
  e-mail addresses, IP addresses and home paths are still scanned inside a
  patch. `test_patch_links_are_reviewed_but_nothing_else_is` pins that
  boundary. Without the exemption the regenerated page fails on its two msdn
  links.
- **The course.** `application_github_public_path_uiforetw_74` serves the
  page through the recorded transport and the product's paged read, and
  passes (`PUBLIC_PATH_OK`, `GITHUB_FIXTURE_OK:uiforetw-74`). Before the
  validator change, the same regenerated page made it fail:
  `PUBLIC_PATH_DIAGNOSTIC:GitHub returned data that BetterPullReview could not
  safely use` and `RUN_BUDGET_EXPIRED … stage=2`. That was the strict
  validator refusing the page.
  `application_uiforetw_selected_diff_contract` reads the same page. Every
  marker that depends on the fixture reads OK (`GITHUB_FIXTURE_OK`, the
  commit pair, both gestures, the blobs, intraline, roles, layout, trace).
  The row is still red only on the two lines [[BPR-1036]] owns:
  `UIFORETW_NAVIGATION_ROW_UNEXPECTED:actions=28` and
  `UIFORETW_CAPTURE_LAYOUT_FAILED:985:835`.
- `tests/fixtures/github/pull-requests/uiforetw-74/README.md` no longer says
  the served page carries no patch.

No `src/app` file changed, so the round-trip contract's pins are untouched.

### Verification (macOS)

| Check | Result |
| --- | --- |
| `cmake --build --preset dev` | Exit 0, no `warning:` line |
| `ctest --test-dir build/dev -j1 --output-on-failure -R '^(github_contract_fixture_security\|github_recorded_fixture_contract\|github_read_contract\|github_paged_read\|application_github_public_path_uiforetw_74)$'` | 5/5 passed (1.10, 0.12, 0.40, 2.71, 11.33 s) |
| `application_uiforetw_selected_diff_contract` | Red on BPR-1036's two lines only, as above |
| Negative controls | Page limit 1 MiB, aggregate 16 MiB, and the old validator: each fails its new row |
| clang-format 23.1.1 on the three C++ files | Clean |
| `_coding-standard/scripts/tidy.py` on the three C++ files | No finding on a changed line |

### Confirmation runs owed (Linux, Windows)

The acceptance names no platform, and the change is portable C++, Python and
JSON. The generator writes UTF-8 with LF, and the page's CRLFs are JSON
escapes, so the bytes are the same everywhere. These runs confirm it and may
follow the finish:

- **Linux (bpr-main-linux):** `cmake --build --preset dev && ctest --test-dir build/dev -j1 --output-on-failure -R '^(github_contract_fixture_security|github_recorded_fixture_contract|github_read_contract|github_paged_read|application_github_public_path_uiforetw_74)$'`
- **Windows (bpr-main-win):** `cmake --build --preset windows-dev && ctest --preset windows-dev -j1 --output-on-failure -R '^(github_contract_fixture_security|github_recorded_fixture_contract|github_read_contract|github_paged_read|application_github_public_path_uiforetw_74)$'`.
  Whichever of these rows are registered there.

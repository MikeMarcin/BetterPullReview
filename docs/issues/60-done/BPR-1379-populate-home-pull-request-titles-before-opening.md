---
schema: "implement-rfc.card.v1"
id: "BPR-1379"
title: "Refresh Home PR titles and submitted-review state before opening"
status: "Done"
rfcs: ["RFC-BPR-0026","RFC-BPR-0010"]
depends_on: []
priority: "P1"
metadata: {"owner_request":{"date":"2026-10-10","channel":"chat","detail":"Work directly. Home on the real GitHub CLI account does not populate PR titles until the PR is opened; it should."},"reservation":"docs/issues/_reservations/BPR-1379--BPR-1379--98a06b8a885c.json","execution_strategy":"direct","milestone_plan":{"id":"bpr1379-home-metadata","path":"docs/rfcs/plans/bpr1379-home-metadata/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1379:titles","outcome":"pass","revision":"76c5143f0bc5d629ce69fd7e24da349c1bf2c8ae","evidence":{"kind":"command","detail":"application_review_queue_enrollment and both native/CLI application_personal_discovery public-path courses passed: retained titles populate before opening, including a followed row outside personal searches and rows without saved repositories."}},{"criterion":"BPR-1379:review-state","outcome":"pass","revision":"76c5143f0bc5d629ce69fd7e24da349c1bf2c8ae","evidence":{"kind":"command","detail":"application_review_queue_enrollment and both composed Home courses passed: approved unopened PRs move to Following at startup and explicit Refresh; changed reviewed heads return to Needs me; pending reviews do not replace submitted evidence."}},{"criterion":"BPR-1379:bounded","outcome":"pass","revision":"76c5143f0bc5d629ce69fd7e24da349c1bf2c8ae","evidence":{"kind":"command","detail":"github_paged_read and application_review_queue_enrollment passed: at most 30 retained jobs, two concurrent reads, four review pages/400 reviews, complete evidence only. Both composed courses exclude snapshot files, diff, history and conversations during Home restoration; architecture_boundaries passed."}},{"criterion":"BPR-1379:identity","outcome":"pass","revision":"76c5143f0bc5d629ce69fd7e24da349c1bf2c8ae","evidence":{"kind":"command","detail":"application_review_queue_enrollment and github_paged_read passed authority/session replacement, altered request envelopes, late removals, closed rows with stale search results, cancellation, malformed/duplicate review records and provider failures; no immediate projection retry loop."}},{"criterion":"BPR-1379:public-path","outcome":"pass","revision":"76c5143f0bc5d629ce69fd7e24da349c1bf2c8ae","evidence":{"kind":"command","detail":"Offscreen fixture courses application_personal_discovery_public_path (17.14 s) and application_personal_discovery_cli_public_path (7.19 s) passed through real private-profile storage/restart and two-window Refresh. CLI course verifies accepted viewer lease and zero native vault access. test_network_isolation_manifest, documentation gate/tests, clang-format 22 and focused implementation clang-tidy passed."}}]
---

# Refresh Home PR titles and submitted-review state before opening

Restored enrollment records carry membership but no titles. Personal discovery
fills the titles of authored and directly requested PRs; a followed, reviewed or
rule-enrolled PR outside those results has no metadata read until it is opened.
The same missing retained-PR read leaves an already submitted review in Needs me.
Refresh previously repeated the personal searches without fetching that evidence.

## Acceptance

- `BPR-1379:titles`: Home populates retained PR titles under the accepted account
  without opening a review, including a retained row outside personal searches
  and a row whose repository is not saved.
- `BPR-1379:review-state`: A review already submitted on GitHub moves the retained
  row out of initial Needs me before opening; explicit Refresh also observes a
  review submitted during the current session and detects a changed reviewed head.
- `BPR-1379:bounded`: Metadata reads are bounded, share the profile's discovery
  scheduler, and fetch no diff, files, commit history or conversations. Submitted
  reviews supply review/head evidence; title text alone establishes no attention.
- `BPR-1379:identity`: Wrong sessions, removed enrollments, late completions,
  cancellation and provider failures cannot repopulate retired rows or publish
  another account's metadata. Failed reads do not cause an immediate retry loop.
- `BPR-1379:public-path`: The composed offscreen application restores a followed
  row's title before that PR is opened; focused provider and host regressions pass.

## Execution

Direct execution in the owner's current checkout, preserving unrelated work.
No layout or style changes are required; this slice restores provider metadata
to the existing Home row presentation.

Owner clarification: the titleless rows are in **Needs me**. A PR already
accepted on GitHub also stayed in Needs me until opened, when it moved to
Following. Home's Refresh did not correct either observation. The same bounded
refresh must supply both title and submitted-review evidence.

## Verification and execution record

Home now refreshes retained PRs before the personal searches, including retained
rows outside saved repositories. The existing profile scheduler admits at most
30 retained PRs and two concurrent reads per cycle. Each read fetches PR metadata
and at most four review pages (400 reviews), through the accepted account
transport. Complete viewer review/head evidence updates attention; an observed
submitted review retains a discovered PR as Reviewed before its old request is
withdrawn. Metadata merges preserve local unread and defect observations.

Provider and host regressions cover startup restoration, explicit Refresh,
approval before opening, head movement, missing saved repositories, pagination
bounds, cancellation, wrong authority, replaced sessions, removed enrollments,
closed rows, provider failures and prevention of projection retry loops.

Validation on macOS:

- `application_review_queue_enrollment` and `github_paged_read` passed.
- `application_personal_discovery_public_path` and
  `application_personal_discovery_cli_public_path` passed offscreen against
  fixtures. Both verify approval placement in two windows on Refresh, then
  restore retained titles and review state through a real private-profile
  restart without opening a PR or fetching files, diffs, history or conversations.
  The CLI variant checks its viewer lease and no native vault access.
- `architecture_boundaries`, documentation verification and its 67 script
  tests passed. Changed C++ files passed clang-format 22; changed application
  and provider implementation files passed focused clang-tidy.

This is provider/host data work; no Home layout or styling changes were made.
No live GitHub mutations or reads from the owner's private repositories were
needed; the CLI account course uses the product's fixture transport and consent.

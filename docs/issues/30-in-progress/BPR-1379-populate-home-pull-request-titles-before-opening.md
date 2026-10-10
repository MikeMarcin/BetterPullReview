---
schema: "implement-rfc.card.v1"
id: "BPR-1379"
title: "Populate Home pull request titles before opening a review"
status: "In Progress"
rfcs: ["RFC-BPR-0026","RFC-BPR-0010"]
depends_on: []
priority: "P1"
metadata: {"owner_request":{"date":"2026-10-10","channel":"chat","detail":"Work directly. Home on the real GitHub CLI account does not populate PR titles until the PR is opened; it should."},"reservation":"docs/issues/_reservations/BPR-1379--BPR-1379--98a06b8a885c.json","execution_strategy":"direct"}
---

# Populate Home pull request titles before opening a review

Restored enrollment records carry membership but no titles. Personal discovery
fills the titles of authored and directly requested PRs; a followed, reviewed or
rule-enrolled PR outside those results has no metadata read until it is opened.

## Acceptance

- `BPR-1379:titles`: Home populates retained PR titles under the accepted account
  without opening a review, including a retained row outside personal searches
  and a row whose repository is not saved.
- `BPR-1379:bounded`: Metadata reads are bounded, share the profile's discovery
  scheduler, and fetch no diff, files, history or conversations. Titles do not
  establish attention facts or change membership.
- `BPR-1379:identity`: Wrong sessions, removed enrollments, late completions,
  cancellation and provider failures cannot repopulate retired rows or publish
  another account's metadata. Failed reads do not cause an immediate retry loop.
- `BPR-1379:public-path`: The composed offscreen application restores a followed
  row's title before that PR is opened; focused provider and host regressions pass.

## Execution

Direct execution in the owner's current checkout, preserving unrelated work.
No layout or style changes are required; this slice restores provider metadata
to the existing Home row presentation.

---
schema: "implement-rfc.card.v1"
id: "BPR-1786"
title: "The review tour link goes at the top of the pull request description"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation": "docs/issues/_reservations/BPR-1786--BPR-1786--0909e26d5713.json", "kind": "feature", "found_by": "owner chat, 2026-10-27", "related_rfcs": ["RFC-BPR-0040"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "7da50279d992ac2922ee75f6c9b4bdd3af6a11cf", "evidence": {"kind": "command", "detail": "application_review_submission_application_host test_description_link_leads: with the link added, the description is the Review tour link line, a blank line, then the desired bytes unchanged; CRLF descriptions use CRLF; without it the description is unchanged."}}, {"criterion": "AC2", "outcome": "pass", "revision": "7da50279d992ac2922ee75f6c9b4bdd3af6a11cf", "evidence": {"kind": "command", "detail": "application_review_tour_publication_* (9 courses) and application_review_tour_cli_ordinary_*: the native and compatibility first publications write a description starting with the Review tour link and still containing the original template bytes; composition source contracts pass."}}]
---

# BPR-1786: The review tour link goes at the top of the pull request description

## Request

The owner, 2026-10-27, verbatim:

> I think the review tour link should go at the top of the PR description not
> the end

## Grounded behavior before

A tour's first publication, when the author chose to update the description,
wrote the desired description and appended `\n\n[Review tour](…)`
(`staged_review_publication.cpp` `description_body`, mirrored in
`staged_tour_update.cpp` for the local baselines). Later updates never add it.

## Built

- `application::staged_description_body` writes `[Review tour](…)`, a blank
  line, then the desired description, keeping the description's own line
  endings. Both the write and the recorded baseline use it.
- The publication preview now says the returned tour link will be placed at
  the top.

## Acceptance

- AC1: the description body puts the link first, unchanged bytes after it.
- AC2: the fixture publication courses write a description that starts with
  the link.

## Not verified

- Descriptions already published with the link at the end keep it there; no
  update moves it.
- No live run on BPR-Test.

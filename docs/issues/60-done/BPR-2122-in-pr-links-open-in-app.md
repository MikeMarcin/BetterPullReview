---
schema: "implement-rfc.card.v1"
id: "BPR-2122"
title: "Links into the open pull request, including tour links, open in the app"
status: "Done"
rfcs: []
depends_on: ["BPR-2046"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2122--BPR-2122--cfdc7a60bcf5.json","source_commit":"d1f641ab1"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d1f641ab1","evidence":{"kind":"command","detail":"application_review_tour_link_public_path: clicking the description's tour link imports and opens the tour at its first stop, no confirmation sheet, recording browser counts zero opens."}},{"criterion":"AC2","outcome":"pass","revision":"d1f641ab1","evidence":{"kind":"command","detail":"core_pull_request_link_route covers this-PR tour links, other PR/repo, malformed, look-alike hosts and non-tour links; application_github_window_read_host in-app links case; markdown, overview, setup destination, cold import and composition contract regressions, architecture, format, tidy and docs pass."}}]
---

# Links into the open pull request, including tour links, open in the app

Owner rule: the external-link confirmation is only for external links written
by people. Links into this pull request open in BetterPullReview. Core
`route_pull_request_link` routes tour index links (`#pullrequestreview-…`,
compatibility `#issuecomment-…`) to the tour, and the PR page, `/files`,
`/commits`, a PR commit, file/line, review and review-comment links to their
in-app views. Anything else keeps the confirmation sheet.

## Acceptance

- AC1. A review tour link in the description opens the tour in the app with no
  sheet and no browser.
- AC2. Other links into this PR with an in-app destination open in the app;
  links to other PRs, look-alikes and unknown pages keep the confirmation.

Left in the browser: non-tour `#issuecomment-` links, `/checks` and unknown
pages, and commits or comments the current read lacks.

---
schema: "implement-rfc.card.v1"
id: "BPR-2046"
title: "Ask before opening a PR body link; explain links while loading"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2046--BPR-2046--bcefddc2b10b.json","source_commit":"3a8cc1374"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3a8cc1374","evidence":{"kind":"command","detail":"qt_composed_markdown and qt_composed_github_scale courses click a description link, answer Cancel/Open/Escape/Return and check the recording browser."}},{"criterion":"AC2","outcome":"pass","revision":"3a8cc1374","evidence":{"kind":"command","detail":"application_github_window_read_host covers file:, javascript:, no-browser, stale and missing-body addresses; refused schemes raise no question."}},{"criterion":"AC3","outcome":"pass","revision":"3a8cc1374","evidence":{"kind":"command","detail":"Composed Qt course clicks a description link while the pull request loads and sees the explanatory status message."}}]
---

# Ask before opening a PR body link; explain links while loading

Owner report: clicking an external link in the PR description did not open the
browser. While a pull request was loading, the window cleared the description's
link handler, so a click did nothing and said nothing.

## Acceptance

- AC1. An http(s) link in a description, comment or check asks with the full
  address and opens the system browser only on Open.
- AC2. A non-web scheme raises no question and opens nothing.
- AC3. A link clicked while the pull request is loading says why it does not
  open.

Follow-ups not taken: admitting links in the early body during loading; agent
answer links still open without the question.

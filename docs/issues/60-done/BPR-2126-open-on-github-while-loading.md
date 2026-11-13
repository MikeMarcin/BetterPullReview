---
schema: "implement-rfc.card.v1"
id: "BPR-2126"
title: "Open on GitHub works while the pull request loads, with no confirmation"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2126--BPR-2127--8a16bd6f852c.json","source_commit":"c6dd12853"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c6dd12853","evidence":{"kind":"command","detail":"qt_composed_markdown_*: with early metadata during load the title row offers Open on GitHub, a click sends the pending read's PR address to the recording browser with no confirmation, and a click naming another PR opens nothing; failed without the fix."}}]
---

# Open on GitHub works while the pull request loads, with no confirmation

Owner report: the title loaded quickly but its Open on GitHub link waited for
the whole pull request. Core now admits the page from the pending read's
metadata and the host opens the browser directly; the app's own PR link never
asks for confirmation.

## Acceptance

- AC1. Open on GitHub appears and opens the pull request in the browser as soon
  as the loading title does, with no confirmation sheet.

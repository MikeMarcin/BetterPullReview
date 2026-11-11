---
schema: "implement-rfc.card.v1"
id: "BPR-2011"
title: "Open every review in the default tray state"
status: "Done"
rfcs: ["RFC-BPR-0105"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0023-review-opens-fresh.md"}
depends_on: []
acceptance: [{"criterion":"RFC-BPR-0105 AC06","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","command":"ctest --preset dev -R application_dependable_session_public_path","detail":"Course step 6: #17 left on the Tour tab with the tour side chat open; #18 opened from Home shows tray index 0 (Comments), side chat column closed, tour page closed; the same when #18 is reopened from Home. Links and restored reviews take the same signalled open; a notification's open was not exercised."}},{"criterion":"RFC-BPR-0105 AC07","outcome":"pass","revision":"31f9009103e26314c3ec69cbf15e08807e4ef64b","evidence":{"kind":"command","command":"ctest --preset dev -R application_dependable_session_public_path","detail":"view_filters() and the Comments drawer width (320) are equal before and after opening the other pull request."}}]
---

# Open every review in the default tray state

## Scope and acceptance

Implements
[RFC-BPR-0105](../../rfcs/RFC-BPR-0105-a-review-opens-fresh-and-a-submission-always-ends.md).

Reset the tray to Comments, close the tour's side chat and page, clear the
selected chat thread and show the pull request's own agent session whenever a
pull request is opened in a window, while keeping the remembered filter
defaults, the window layout, sessions, drafts and reading position.

Covers AC06 and AC07.

This is part of `dependable-session` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Record

Every open of a pull request in a window closes the tour page and the tour's
side chat and puts the tray on Comments. The composition tells the window when
it dispatches an open (`begin_review_open`): the window's own open gestures
(Home rows, the Home bar, recent reviews), a link opened in the window, and a
restored review. The surface is reset then, so a tab chosen while the pull
request loads is kept, and a refresh never resets it. Two earlier attempts
were wrong and are recorded: resetting on every presentation that did not
preserve navigation also fired on each refresh and crashed
`application_review_tour_publication_repairs` by closing its open tour page;
resetting on the first presentation after an open discarded the Agent tab a
course chose while the pull request loaded
(`application_native_agent_sessions`).
Course step 6 of `application_dependable_session_public_path` checks both the
other pull request and the same one reopened, and that the remembered filters
and the Comments drawer width are unchanged ([verification](../../verification/RFC-BPR-0105.md#results-at-the-candidate)).

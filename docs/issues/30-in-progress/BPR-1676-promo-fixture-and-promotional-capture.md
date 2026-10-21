---
schema: "implement-rfc.card.v1"
id: "BPR-1676"
title: "A promo fixture and repeatable promotional captures of the real window"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"tooling","origin":"Owner chat, 2026-10-20, redesigning betterpullreview.com","reservation":"docs/issues/_reservations/BPR-1676--BPR-1676--27572c2d2d15.json","owner_decision":{"date":"2026-10-20","fixture":"Build a promo fixture","desktop":"Desktop, only when asked"},"acceptance":["BPR-1676:fixture","BPR-1676:served","BPR-1676:capture","BPR-1676:skill","BPR-1676:owner-look"]}
---

# BPR-1676: A promo fixture and repeatable promotional captures of the real window

## Origin

Redesigning betterpullreview.com on 2026-10-20, the owner said the site's
interactive demo "doesn't match at all what the app actually looks like or
does well", that the app's real benefits are "a way better comment and code
browsing experience than the github website", and that AllTheThings has
tooling to create marketing materials that BetterPullReview should share.
Looking at captures of `conversations-v1`, the owner added that "with the file
drawer open its very hard to see much code" and that "it would be better if we
could find a diff with an actual change rather than just pure additions".
No existing fixture pairs code changed in place with review threads.

Owner decisions, 2026-10-20: build a promo fixture; capture promotional media
from the real window on the desktop, only when the owner asks for it, as a
narrow exception to AGENTS.md's no-desktop rule.

## Acceptance

- BPR-1676:fixture — `scripts/build_promo_fixture.py` constructs `promo-v1`:
  code changed in place (not only added blocks), eight Conventional Comments
  threads on changed lines with replies, resolved threads and unread posts,
  synthetic identities, and a viewer reviewing someone else's pull request.
  `recorded_pull_request_fixture_tests.py` regenerates it byte for byte and
  `github_contract_fixture_security_tests.py` passes.
- BPR-1676:served — `--github-fixture promo-v1` serves it in the test-enabled
  application (`PUBLIC_PATH_OK`), and `--attended-read-state` applies its read
  state.
- BPR-1676:capture — `scripts/capture_promo_assets.py --confirm-desktop`
  produces the Review, Overview, Discuss and Threads stills and the demo
  recording from the application's own window only, with the files drawer
  hidden, and refuses identical stills.
- BPR-1676:skill — `.codex/skills/update-promo-assets` documents the
  workflow and its safety rules; AGENTS.md records the exception.
- BPR-1676:owner-look — the owner accepts the captures.

## Boundaries

No production behaviour changes: the fixture and its launch branch exist only
in tests-enabled builds. The capture never records a screen region.

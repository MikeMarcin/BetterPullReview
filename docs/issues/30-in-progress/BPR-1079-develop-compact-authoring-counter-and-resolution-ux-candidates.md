---
schema: "implement-rfc.card.v1"
id: "BPR-1079"
title: "Develop compact authoring, counter and resolution UX candidates"
status: "In Progress"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0050"]
depends_on: ["BPR-1075"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-05","milestone":"design","acceptance":["R30","R33"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1079: Develop compact authoring, counter and resolution UX candidates

Pack alias: **PACK-CC-05** (live `BPR-1079`, allocated at import).

## Outcome and work

Inspect the current tour-enabled workbench before proposing changes. Produce a small set of concrete candidates covering optional authoring, native review preflight, Settings and Overview with zero/typical/all label columns. Include unknown coverage and labelled replies. Do not prescribe icons from the chat.

## Acceptance

- Candidates demonstrate actual narrow/wide constraints, long content and full twelve-column overflow behavior.
- Each candidate explains counting units, catchall remainder, native review choices and public/local ownership.
- Tradeoffs are recorded without claiming a mockup is implemented.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Candidates, round v1 (2026-10-05)

Before drawing anything, the current surfaces were inspected at `7ff4126ca` through
offscreen renders from `qt_pull_request_overview_page` (`BPR_QT_RENDER_PATH`),
`qt_review_submission_editor_scale_100` and `qt_review_draft_editor`, and through
the workbench spec. The Overview Comments cell shows an amber unresolved bubble and
a grey total bubble. Compose review chooses the disposition in a combo box and has
no preflight. The inline page has Publish and Discard over plain text. Comment's
arrow menu is reserved for labels and offers none.

The [design record](../../design/BPR-1079-feedback-ux-v1.html) sets out the
rules every candidate keeps: counting unit, Other as the catchall, the
Not on a file remainder, four resolution states, labels versus states,
identification, drill-down, public versus local, native review and suggestions.
It also records the candidates, their tradeoffs and the open items.
[Frames](../../design/BPR-1079-feedback-ux-frames-v1.html) are drawn at 1440×820
and at 960×700–760, with Settings at its 760×560 minimum, in light, and three of
them in dark:

- Overview counts: **A** a column per label, against **B** label chips in one
  Comments column. Each is shown with no columns, issue + question and all
  twelve. Twelve columns at 960 px overflow with File pinned (A) or fold into
  "+n" (B).
- Open/All switch, the ? coverage chip, the state bar in All, and the Label key (⌥K).
- Authoring: **A** a label chip with a menu, **B** an icon strip, **C** typing
  with completion. Each has a labelled reply under a differently labelled root
  and a long body with a native suggestion.
- Thread cards: square label marks, round state pills, a mark on each reply,
  a plain tour stop, applicable and fallback suggestions, and the private
  defect badge.
- Review preflight: **A** a list beside the summary, against **B** one chip
  with its list on demand. Both have three visible dispositions.
- Settings → Reviews & queue → Comment labels.

The counts are computed in the frame from conversation units. HairStrandsLighting.cpp
equals the pack fixture `src/cache.cpp`: Open gives 2/1/4 of 7 and All gives
4/1/5 of 10. Label marks are two-letter placeholders, and the icons are BPR-1080's.
Limitations: these are static HTML frames rendered with headless Chrome. They
are not the product, and they are not native, scaled, high-contrast or
assistive-technology evidence (R33 applies to the built surfaces). Nothing is
implemented.

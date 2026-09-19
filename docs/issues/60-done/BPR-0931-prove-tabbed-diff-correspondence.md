---
schema: "implement-rfc.card.v1"
id: "BPR-0931"
title: "Prove exact tabbed diff text correspondence and pane alignment"
status: "Done"
rfcs: ["RFC-BPR-0004", "RFC-BPR-0005", "RFC-BPR-0006"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"report-followups-20260919","path":"docs/rfcs/plans/report-followups-20260919/milestone-plan.json"},"source_report":"report-followups-run-20260919"}
acceptance: [{"criterion":"A focused Qt contract publishes representative one-, two- and three-tab lines, including styled syntax spans, and proves the pane documents equal their decoded left/right sources exactly.","outcome":"pass","revision":"4e409bc967a9814e5a492f3eaf644254352e1df8","evidence":{"kind":"command","detail":"All three qt_diff_workbench_scale variants pass the new exact toPlainText and styled-token-range contract."}},{"criterion":"Corresponding equal-depth lines have the same pane-relative first-glyph x coordinate under the same appearance profile; distinct indentation depths remain distinct and a tab-width change updates geometry without changing source text, line identity or mapping.","outcome":"pass","revision":"4e409bc967a9814e5a492f3eaf644254352e1df8","evidence":{"kind":"command","detail":"The Qt test measures equal pane-relative positions for one/two/three tabs, distinct depths, widened geometry at eight columns and unchanged source/comparison/semantic-map identity."}},{"criterion":"The recorded epoch and UIforETW observations are documented as authored whitespace, not silently reindented. No synthetic rows, whitespace rewrite, proportional token layout or horizontal authority coupling is introduced.","outcome":"pass","revision":"4e409bc967a9814e5a492f3eaf644254352e1df8","evidence":{"kind":"artifact","detail":"milestone-syntax-and-text-presentation-evidence.md records decoded fixture tabs and the natural-layout render path; the implementation commit changes only the Qt test, not product text/layout."}},{"criterion":"Focused diff semantics, measured-axis, diff-workbench and appearance tests pass; any real discrepancy uncovered by the executable contract is fixed at its owning boundary before the card closes.","outcome":"pass","revision":"4e409bc967a9814e5a492f3eaf644254352e1df8","evidence":{"kind":"command","detail":"Corrected-candidate integration passed core_diff_compute, core_diff_semantics, all qt_diff_workbench scales and qt_appearance_profile; no product discrepancy was found."}}]
---

# Prove exact tabbed diff text correspondence and pane alignment

## Owner report and provenance

The owner asked that the apparent excessive indentation and misalignment in
the BetterPullReview captures be traced through the real model/render path and
corrected at the owning boundary. Exact left/right line identity and diff
correspondence must not be changed to improve appearance.

Read-only source captures:

- BetterPullReview synthetic diff presentation:
  `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/codex-clipboard-48f40a37-a463-4717-8834-3e142edb4b90.png`
  (`sha256:4c791dfaf09d97cf6cf69786eba35af62c7e481719d4b11e4a815da0efcd2055`).
- BetterPullReview UIforETW C++ presentation:
  `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/codex-clipboard-039e15ef-c534-41b5-9de6-e929f7668bbe.png`
  (`sha256:5c4379d59152e663e5b4f80b29c1a813e9cb91f024bf8a8746ccf2cf81e464d2`).

The original report path and digest are recorded on [[BPR-0930]]. This card
does not reopen RFC-BPR-0037 acceptance.

## Diagnosed boundary

Both captures preserve fixture-authored tabs. The epoch fixture intentionally
uses one- and two-tab rows; the recorded UIforETW source uses one, two and three
tabs. Matching tab depths begin at matching pane-relative x positions. The Qt
path publishes each complete source document directly into a `QPlainTextEdit`,
uses the same fixed-font four-column tab distance in both panes, and adds only
foreground/emphasis/background formats. Natural vertical displacement is the
RFC-BPR-0004 design; synthetic alignment rows would corrupt line identity.

## Acceptance

- A focused Qt contract publishes representative one-, two- and three-tab
  lines, including styled syntax spans, and proves the pane documents equal
  their decoded left/right sources exactly.
- Corresponding equal-depth lines have the same pane-relative first-glyph x
  coordinate under the same appearance profile; distinct indentation depths
  remain distinct and a tab-width change updates geometry without changing
  source text, line identity or mapping.
- The recorded epoch and UIforETW observations are documented as authored
  whitespace, not silently reindented. No synthetic rows, whitespace rewrite,
  proportional token layout or horizontal authority coupling is introduced.
- Focused diff semantics, measured-axis, diff-workbench and appearance tests
  pass; any real discrepancy uncovered by the executable contract is fixed at
  its owning boundary before the card closes.

## Verification and execution record

Accepted at source candidate
`4e409bc967a9814e5a492f3eaf644254352e1df8`. The captures contain authored
tabs and natural line displacement, not a syntax-layout or correspondence
defect. The new Qt contract proves exact documents, exact line mappings and
equal pane-relative glyph positions through a four-to-eight-column tab-width
change. No product source, source whitespace or synthetic row was changed.
See
[`milestone-syntax-and-text-presentation-evidence.md`](../../rfcs/plans/report-followups-20260919/milestone-syntax-and-text-presentation-evidence.md).

---
schema: "implement-rfc.card.v1"
id: "BPR-2266"
title: "A report's inline code is coloured again"
status: "Done"
rfcs: []
depends_on: []
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2266--BPR-2266--0872a4edcad1.json","origin":"owner, 2026-11-18","related_cards":["BPR-2166","BPR-1753","BPR-2219"]}
acceptance: [{"criterion":"AC-a","outcome":"pass","revision":"926baf88e","evidence":{"kind":"command","detail":"qt_agent_report_view check_inline_code: a dressed report's inline code (summary and finding) is asked for as code.cpp through the body's dispatch and, when the spans arrive, coloured on the page only inside each span; a place stays an uncoloured link; a render again keeps the colours"}},{"criterion":"AC-b","outcome":"pass","revision":"926baf88e","evidence":{"kind":"command","detail":"same check fails on the unfixed source at \"the page colours its inline code, and only inside the span: window_extent()\"; passes with the fix"}}]
---

# A report's inline code is coloured again

## Origin

The owner, 2026-11-18: "Seems like we lost syntax highlights for inline code
on reports."

## Scope and acceptance

- **AC-a.** Inline code spans on a report's page are coloured as the Agent
  tab colours an answer's inline code: the pull request's language, through
  the same syntax dispatch.
- **AC-b.** A Qt test asserts syntax formats on a rendered report's inline code.

## Root cause

The dressed report page (BPR-2166, `16e8e2dc9`) draws a report with
sections as Qt rich text from `report_page_html`, laid over the report's
Markdown body. Inline code became a monospace `<span>`. The colouring of
inline code (BPR-1753) lives in the Markdown reader: `markdown_code_store`
finds fixed-pitch runs and asks the syntax host for their spans as the pull
request's language. The report's Markdown body still asked for and received
those spans, but it is hidden behind the page. The page had no fixed-pitch
runs and nothing coloured it: Qt's HTML import does not set fixed pitch, even
for `<code>`.

## Fix

`mark_report_inline_code` marks the page's code runs (code font, not a link)
fixed pitch after `setHtml`. `agent_report_view` colours them with the spans
its Markdown body holds (`markdown_body_view::style_inline_code_of`) after
every render and on every publication that arrives.

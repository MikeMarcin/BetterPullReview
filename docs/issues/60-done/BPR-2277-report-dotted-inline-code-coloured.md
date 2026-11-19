---
schema: "implement-rfc.card.v1"
id: "BPR-2277"
title: "A report's member-access inline code is coloured"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","target":"0.5.0","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2277--BPR-2279--424d9c5fc0e9.json","found_by":"owner attended review, 2026-11-19, frozen fixture build of 91dedb8a3","related_cards":["BPR-2266","BPR-1753","BPR-2166"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"64f2f1733","evidence":{"kind":"command","detail":"qt_markdown_code check_report_member_access and core_markdown_code inline_code_members: member access, calls, keywords and literals keep or get their colours through the same dispatch the drawer and Agent tab use"}},{"criterion":"AC2","outcome":"pass","revision":"64f2f1733","evidence":{"kind":"command","detail":"qt_markdown_code_1_0/1_25/2_0: the fixture text `Target.resize` on a report page through the composed classification path has coloured Target and resize ranges inside the span only; fails with the helper disabled"}},{"criterion":"AC3","outcome":"pass","revision":"64f2f1733","evidence":{"kind":"command","detail":"core_markdown_code, qt_markdown_code_*, qt_agent_report_view, application_review_report_public_path, application_ask_report_public_path, syntax_classification_contract, architecture_boundaries, coding-format-check, tidy on the changed core file, verify_documentation"}}]
---

# BPR-2277: A report's member-access inline code is coloured

## Observed

Owner, 2026-11-19, on the conventional-comments-v1 fixture's agent report
"Review of #21": "it doesn't seem like `Target.resize` is getting
highlighting here." The span is monospace on its grey chip but has no syntax
colour. Reopens [[BPR-2266]].

## Acceptance

- AC1. Inline code on a report's page is coloured as the comments drawer and
  the Agent tab colour it: identifiers, member access, calls, keywords and
  literals, through the same syntax dispatch.
- AC2. A Qt test renders a report holding `Target.resize` (the fixture's own
  text) and asserts syntax formats inside that span.
- AC3. Focused tests, format and the documentation gate pass.

## Cause

Not the report page: [[BPR-2266]]'s colouring reached it, and the drawer and
the Agent tab colour inline code through the same store. The classifier, run
as `code.cpp` on the text `Target.resize` alone, gives one span: the `.` as an
operator. Its C++ identifier pass names a member only when it is called
(`resize(` is a function) or declared in the same text, and a mention in
prose is neither, so both names stayed `syntax_normal`, the code's plain
ink. A call such as `window_extent()` was coloured, which is why BPR-2266's
test, and the owner's other reports, looked right.

## Fix

`core::markdown_inline_code_members` names inline code's member access: a
name before or after `.` or `->` that the classifier left plain becomes
`syntax_variable`; a call keeps its function colour, a keyword or literal
its own. A file name or path (`README.md`, `src/engine/renderer.cpp`,
`tonemap.frag`), a number such as `1.5e3`, an ellipsis and text over several
lines are left as they came. The Markdown reader's code store applies it to
every inline span it is given (`markdown_code_store::apply`), so the report
page, the comments drawer, the Agent tab and the tour's readers colour the
same text the same way.

## Verification

- `markdown_code` (core): `Target.resize`, `node->next`, `Target.resize()`,
  `this->Width`, a lone name, a number, an ellipsis, six file names and
  paths, and text over lines.
- `qt_markdown_code_1_0`: `check_report_member_access` presents the
  fixture's own finding text ("The width is clamped, but the height goes to
  `Target.resize` as it arrives…") in an `agent_report_view` through the
  composed path (packaged C++ definition, the reader's classification host)
  and asserts coloured ranges on `Target` and `resize` inside the span and
  nothing outside it. With the core helper returning the classifier's spans
  unchanged it fails at "`Target.resize` on a report's page: the object and
  its member are coloured"; with the fix it passes.
- `qt_agent_report_view` passes unchanged.

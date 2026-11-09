---
schema: "implement-rfc.card.v1"
id: "BPR-1957"
title: "Compact Go to line and support line:column"
status: "Ready"
rfcs: ["RFC-BPR-0097"]
depends_on: ["BPR-1948"]
priority: "P1"
metadata: {"mandate_revision":"go-to-line-columns-r4","assigned_implementer":"/root/go_to_line_design","reservation":"docs/issues/_reservations/BPR-1957--BPR-1957--ea6d03ed4be4.json","milestone_plan":{"id":"go-to-line-v1","path":"docs/rfcs/plans/go-to-line-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC05","outcome":"pass","revision":"dd968668313c5abe343273de960ea9f00bfefae1","evidence":{"kind":"observation","detail":"Exact split/unified line:column and plain-line native caret, parser and side-dependent validation, Unicode/CRLF, unchanged source-line draft anchor, cancellation/stale guards and bounded work pass in the composed Git/cache application at 100/125/200%. Focused production Qt course adds tabs, empty source lines, half-surrogate refusal, overflow/EOL and horizontal caret visibility at all three scales. Architecture, format22, focused tidy and documentation pass. See docs/verification/RFC-BPR-0097.md."}},{"criterion":"AC06","outcome":"pending","revision":"dd968668313c5abe343273de960ea9f00bfefae1","evidence":{"kind":"observation","detail":"Shared Find close control, no filename/Cancel, keyboard/accessibility/theme/scaling and inspected actual-widget renders pass. Exact v005 frozen appearance requires owner Accept/Reject; prepared session identity is recorded in the handoff. Historical v002 navigation and v003 rejection remain bound to their original binaries."}}]
---

# Compact Go to line and support line:column

Owner amendment to [RFC-BPR-0097](../../rfcs/RFC-BPR-0097-go-to-line.md):
reuse Find's close control, remove the repeated filename and support optional
one-based UTF-16 columns. The source-side selection and single-line comment
anchor remain exact in split and unified views. This is the `line-column-panel`
milestone; BPR-1948's completed original acceptance remains historical.

## Acceptance

- AC05: Exact plain-line and line:column caret navigation, validation,
  Unicode/tab/CRLF/empty/end-of-line semantics, horizontal visibility,
  single-line draft anchoring and unchanged bounded work.
- AC06: Shared Find close control, compact layout without filename or Cancel,
  keyboard/accessibility/theme/scale checks, and owner acceptance of the exact
  frozen appearance candidate.

## Evidence and frontier

[Verification](../../verification/RFC-BPR-0097.md) and
[checkpoint](../_checkpoints/go-to-line-v1/post-review.md) retain source,
checks and exact review/session identity. AC05 passes at frozen revision
`dd968668313c5abe343273de960ea9f00bfefae1`: composed three-scale navigation and
focused native column courses, architecture, format/tidy and documentation.
AC06 has passing keyboard/accessibility/theme/scale observations and inspected
400×72 logical-pixel actual-widget previews; its exact frozen v005 appearance
is pending owner judgment in session `9827b9b1187675faf91e0e097db5a23f`.
The launcher check passes; source, build and fixture trees are self-contained.
No prior appearance verdict is transferred to this candidate.

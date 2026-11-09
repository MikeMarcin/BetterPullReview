---
schema: "implement-rfc.card.v1"
id: "BPR-1957"
title: "Compact Go to line and support line:column"
status: "Ready"
rfcs: ["RFC-BPR-0097"]
depends_on: ["BPR-1948"]
priority: "P1"
metadata: {"mandate_revision":"go-to-line-columns-r4","assigned_implementer":"/root/go_to_line_design","reservation":"docs/issues/_reservations/BPR-1957--BPR-1957--ea6d03ed4be4.json","milestone_plan":{"id":"go-to-line-v1","path":"docs/rfcs/plans/go-to-line-v1/milestone-plan.json"}}
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
checks and exact review/session identity. Functional verification and the new
frozen appearance candidate are in preparation. No prior appearance verdict
is transferred to this candidate.

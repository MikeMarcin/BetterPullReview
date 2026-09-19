---
schema: "implement-rfc.card.v1"
id: "BPR-0930"
title: "Match C++ and Windows resource syntax roles to the Visual Studio reference"
status: "Ready"
rfcs: ["RFC-BPR-0006"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"report-followups-20260919","path":"docs/rfcs/plans/report-followups-20260919/milestone-plan.json"},"source_report":"report-followups-run-20260919"}
---

# Match C++ and Windows resource syntax roles to the Visual Studio reference

## Owner report and provenance

The owner reported that `void` was yellow instead of blue and that nearly an
entire Windows `.rc` file appeared yellow. The requested reference distinguishes
language built-ins/directives, class or struct types, functions, strings,
macros/resource keywords, identifiers and comments instead of applying one
broad type colour.

Read-only source captures:

- Visual Studio C++ reference:
  `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/codex-clipboard-a271dd2c-28c0-4eb9-8a40-bc1f798f5836.png`
  (`sha256:1400c9771e0b3f1bffc59de48299207f253d5e1949eb798822744f2de0090861`).
- Visual Studio resource reference:
  `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/codex-clipboard-8da4b557-6b03-4a31-b818-fe772ae1dff9.png`
  (`sha256:f7d7aed2ff1142a66d192ed6ac363e24780b7ab802e0bcc9c999d66932a04ee8`).
- BetterPullReview resource rendering:
  `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/codex-clipboard-9112cedb-47bd-4b35-841a-6839a28c66e1.png`
  (`sha256:9e02fcfb241e2c8cc9d95a14601a5681f2f0cfbab3d618ef94b40cfd474c7450`).
- BetterPullReview C++ rendering:
  `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/codex-clipboard-039e15ef-c534-41b5-9de6-e929f7668bbe.png`
  (`sha256:5c4379d59152e663e5b4f80b29c1a813e9cb91f024bf8a8746ccf2cf81e464d2`).

The original attended report is
`/Users/jaeger/.codex/attachments/debd9a4a-8afd-4461-9096-48d41c498dc8/Pasted text.txt`
(`sha256:74d3788db7c7eee4c692a0d4bcfa36ce7a3c35a68fcc10ad4385a32bb69683be`).
RFC-BPR-0037 classified the observation as outside its first-review workflow;
this card is the canonical follow-up and does not reopen that accepted scope.

## Diagnosed boundary

KSyntaxHighlighting emits C++ built-in types such as `void` as `DataType`,
which the adapter currently maps to the yellow `syntax_type` role also used by
BetterPullReview's user-type heuristic. KSyntaxHighlighting 6.28.1 has no
Windows-resource definition; its TOML definition claims `*.rc`, styles bare
keys as `DataType`, and recognizes `#` rather than `//` comments. That explains
both reported symptoms.

## Acceptance

- C and C++ upstream data-type spans such as `void`, `bool` and `wchar_t` use
  the blue built-in role while heuristically identified user types such as
  `CUIforETWDlg` and `NameToString` remain yellow type spans.
- `.rc` selection cannot fall through to TOML. A representative resource file
  has distinct directive/macro, resource-keyword, string, comment and ordinary
  identifier spans, with no blanket yellow treatment.
- Existing directive subranges, include operands, macro roles and exact source
  offsets remain intact; malformed or unsupported input still fails safely.
- Focused syntax and Qt appearance/composition contracts pass on packaged
  definitions and the checked-in synthetic resource fixture.


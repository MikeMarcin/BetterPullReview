---
schema: "implement-rfc.card.v1"
id: "BPR-1303"
title: "After a refresh in Review a newly chosen line draws no draft, so the label-authoring course stalls at its suggestion phase"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"application_label_authoring_contract on macOS at c01e04974 and later; passed at 6984585b9 (2026-10-06)","reservation":"docs/issues/_reservations/BPR-1303--BPR-1303--546a2bd2ce3f.json","related_cards":["BPR-1083","BPR-1084","BPR-1132","BPR-1277","BPR-1292","BPR-1293"],"first_bad_commit":"7778886c9"}
---

# BPR-1303: After a refresh in Review a newly chosen line draws no draft

## Defect

`application_label_authoring_contract` ([[BPR-1083]], [[BPR-1084]]) fails on
`main` on macOS, every run. The course publishes its labelled comment,
refreshes, and then stops at its first suggested-change phase until the run
budget expires:

```text
PUBLIC_PATH_BUDGET:expired=1 budget_ms=40000
LABEL_AUTHORING_FAILED
LABEL_AUTHORING_DIAGNOSTIC:... | phase=9 line=1 draft=- comment=0
```

Line 1 is selected on the right side, but no draft is drawn there and Comment
stays unavailable ("No editable draft is available"). It passed on this Mac at
`6984585b9` and fails on `c01e04974`.

## Outcome

Find the first bad commit and the cause, fix it in the product or the test,
show a control that fails without the fix, and pass the row 10 times in a row.
Offscreen only.

## Acceptance

- `BPR-1303:bisect`: the first bad commit between `6984585b9` and `c01e04974`.
- `BPR-1303:cause`: the cause, with the facts that show it.
- `BPR-1303:fix-and-control`: the fix, and a control that fails without it.
- `BPR-1303:stable`: 10 consecutive passes of the row at the fix.
- `BPR-1303:neighbours`: rows that share the changed path still pass, and
  the round-trip pins are resealed.

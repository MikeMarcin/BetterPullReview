---
schema: "implement-rfc.card.v1"
id: "BPR-1943"
title: "Wait for current round-trip fixture input before the off-hunk gesture"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"kind":"test-correctness","found_by":"BPR-1942 normal native round-trip failure and bounded diagnostic","related_cards":["BPR-1942"]}
---

# BPR-1943: Current fallback fixture input

## Acceptance

- AC1. Before the existing single off-hunk gesture, the round-trip fixture waits
  for the current accepted selected-diff publication's generation/file and the
  existing comparison/private-draft readiness conditions. Preserve the original
  gestures, 15 s phase/40 s course budgets, assertions, provider sequence and every
  product admission guard. Retain useful bounded failure facts only; temporary
  diagnostic phase snapshots and relay are absent.
- AC2. Independently review/reseal the unchanged 142-file app inventory for this
  tests-only correction, retaining the other content/mutation pins. The final
  uninstrumented combined native source builds affected targets and passes the
  original registered round-trip and invocation plus approved tooling/normal
  preflight with the configured owned-root route. Retain the actual original
  failure and diagnostic evidence separately, with no diagnostic acceptance.
- AC3. Focused format/tidy/docs/board and latest authoritative main integration
  pass. Bind source, binary, configuration, runtime and command ownership in the
  handback. Full frozen release/native/trust/publication remains coordinator work.

## Observed input defect and mandate

BPR-1942's actual normal native runner at source 861be passes preflight and 6/7
selected rows; round-trip fails after its existing 15 s fallback-draft wait with
ReviewDraft absent and outstanding work zero. The 40 s total budget is not expired.
Its source, stdout, JUnit, input/property and storage proofs remain external.

The one approved diagnostic at private source 22f9064ca passes, without acceptance
credit. Its off-hunk-selected snapshot has private readiness absent, zero file
rows, publication 1 and BOTH generation/file matches false. The fixture acts on
an old displayed comparison after the host refresh advanced. Later readiness
has publication 2, current generation/file, private readiness and a drafting
comment. The presenter correctly refuses the earlier inadmissible request.
This proves a fixture input defect; it does not prove every observed missing
notification has the same cause or attribute it to BPR-1941's timing changes.

The coordinator explicitly approves a separate owning fixture correction:
wait for current publication and readiness BEFORE the same gesture, under
original budgets; no product-policy change, extra/replayed gesture or weaker
verdict. Reuse one final normal registered invocation/round-trip +tooling/
preflight run for this correction and BPR-1942. Diagnostics are deliberately
restored, with their private commits retained and binary labelled rebuild-required.

---
schema: "implement-rfc.card.v1"
id: "BPR-1943"
title: "Wait for current round-trip fixture input before the off-hunk gesture"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"kind":"test-correctness","found_by":"BPR-1942 normal native round-trip failure and bounded diagnostic","related_cards":["BPR-1942"],"milestone_plan":{"id":"bpr-1943-current-fallback-input","path":"docs/rfcs/plans/bpr-1943-current-fallback-input/milestone-plan.json"}}
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


## Bounded correction and self-review

A test-only probe-state boolean makes the existing Review choice once. The
same phase then awaits a current selected publication, matching host generation
and selected file, its displayed semantic comparison, and existing private
readiness/settled command-selection work. The exact off-hunk gesture loop is
byte-identical to source 861be. No product guard, phase value/transition,
provider/verdict sequence, gesture retry or timer changes. `keep_waiting` keeps
the same phase Since value even when its diagnostic term changes, so the 15 s
watchdog and 40 s course budget remain the original bounds. Five useful boolean
failure facts remain; temporary phase snapshots and relay are absent.

The unchanged 142-file manifest independently reproduces 51f86c4a… and binds the
two reviewed test-only files as 913e8b38770b835ddb1aa6a8e7fca85d363764901c4ce608a930113824e9ef26.
The maintained digest printer agrees; other pins and inventories are unchanged.
Focused clang-format22 and warnings-as-errors clang-tidy22 pass, as does the
documentation gate after merging main 8f3102c2a's BPR-1944 card/reservation
without any functional source delta. Native uninstrumented normal verification
is pending. External fixture-self-review-r3.json records the mechanical
sequence/timer/source-contract comparisons and retained failure evidence.


## Final normal native acceptance and main integration

Final uninstrumented combined source 9c3936cb5ad659a7487a44b7d17ebddadf1207ca
is rebuilt and verified on the native Linux station, then intentionally pushed
to authoritative main including latest 8f3102c2a. The affected app, invocation
and helpers build passes 159.63 s with all 79 protected feature/compiler/hardening/
tool/cache keys unchanged. The source is clean; main and invocation binaries
contain no temporary diagnostic markers, and the app contains the real input
guard. The normal 927-node configured graph changes exactly 25 writable root
arguments and their exact logical-command mirrors; all other properties are
identical and compile commands do not change across root selection.

The ordinary suite runner passes all seven selected original registered rows:
invocation 6.16 s, round-trip 1.92 s, the new runtime-root contract and four existing
network-manifest/structure, suite-runner and command-registry tooling gates.
Its three original documentation/issue/allocation preflight rows also pass.
No projection, assertion/gesture/provider-sequence change or budget relaxation
supplies these results. The 14-bit composition evidence and source contract
remain enforced by the original registered course. The R1 functional negative
and R2 diagnostic positive remain separately retained; the latter is not acceptance.

The final full Git source is exact on both machines, with `src` tree
67f3e76a720956a9ae653e699bd48825610b54d3 and `tests` tree
eafc2a4942e4b48bfaf3cff919d8ec34d83c63e7. Native app SHA256 is
8b232d317fa9e52309d91ce6e254b7fd80d12bec9fe5baf1016a31a6cb430dbc;
invocation SHA256 is
f7402c3337b7b1352b31bc2deeb49de9ab9dbe2c9871e3262aea3fd0b5f22dc2.
Raw receipts/XML/stdout/cache/graph proofs are external
candidate39/implementer1942/native-final-r3 and verification-r3-results.json.
The guarded runtime peaks at 3,997,696 allocated bytes (256 MiB cap, original
D guard, C32 GiB/root8 GiB reserves). Every owned descendant is reaped,
normal empty-root cache restored and the exact owned runtime root removed.

Focused format22/tidy22, documentation and board checks pass. This closes the
bounded engineering criteria; selected 7+preflight 3 is not comprehensive or
frozen-source release qualification. The coordinator owns the final frozen
Mac/Windows/Linux graphs, full suites, security/trust/package/publication and
full-suite runtime forecasts. Native development ownership is returned only
after the final clean/idle handback.

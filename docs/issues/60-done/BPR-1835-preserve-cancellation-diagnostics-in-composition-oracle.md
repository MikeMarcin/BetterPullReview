---
schema: "implement-rfc.card.v1"
id: "BPR-1835"
title: "Keep cancellation diagnostics in the composed agent oracle"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-1827"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"release_issue":"BPR-1811","mandate":"release-v041-full-stable-N8-oracle-followup","external_state":"/Users/jaeger/.codex/release-runs/v0.4.1-20261029","frozen_release_source":"d9d4a822dbf603c7bb22c83574f5a64d2bee5645","integration":"Separate future-main test-oracle repair; frozen publisher, source and producers remain D9.","frontend_design":"Existing frontend-design guidance loaded; no product words, controls, layout or policy change."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a3cd2295c70819d0b710ff8e7ae294b2b8da6128","evidence":{"kind":"artifact","detail":"Narrow independent test-oracle correction and positive/hostile controls reviewed; all other ownership, schema/privacy or cleanup predicates retained. Frozen D9 products unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"a3cd2295c70819d0b710ff8e7ae294b2b8da6128","evidence":{"kind":"artifact","detail":"Full applicable Mac32/Windows26semantic/Ubuntu32 native proofs with source/image/argv/context seals. Original failures and Windows Debug45s limits retained; complete functional supplement under permanent policy has no performance credit. Exact proof hashes in card body and actions.json."}},{"criterion":"AC3","outcome":"pass","revision":"a3cd2295c70819d0b710ff8e7ae294b2b8da6128","evidence":{"kind":"command","detail":"Original native compiler/config/header/library/helper bindings verified; unchanged clang-format22 four-file receipt SHA 419902166e0d5d323da472bdc62f534d51a05b9330a6d73da51ea5a75b7a955d; documentation-before-finish passed. Separate unpushed future-main commit only; final moved-board gate follows before milestone completion."}}]
---

# Keep cancellation diagnostics in the composed agent oracle

N8 macOS Release records a failed cancellation public course. The expected
cancelled status has arrived, but the old empty-output assertion rejects a
53-byte log before the workspace-cleanup wait is entered. BPR-1827 deliberately
shows a bounded diagnostic summary for a stopped run that wrote stdout. This
course first requires the canary to write, so empty output is no longer its
accepted public contract. The raw failure and its unreached cleanup checks
remain preserved; the immediate cleanups=0 snapshot is not a cleanup timeout.

## Acceptance

- AC1: Correct only the test's cancellation-output oracle to require the accepted
  bounded diagnostic summary without raw canary/private content. Keep staleness
  output expectations and production behavior unchanged, and add meaningful
  positive and hostile controls for the corrected predicate.
- AC2: Replay the complete public cancellation contract against the exact
  unchanged product/helper inputs and original budgets. Retain typed terminal
  state, descendant death, exactly one workspace cleanup, quiescence and shutdown
  assertions; no earlier failure or unreached assertion receives passing credit.
- AC3: Bind any private native test build to the original configuration, compiler,
  headers, libraries and helper hashes, with only its test translation unit,
  object and output paths differing. Complete focused format/documentation
  checks and leave a separately reviewable future-main commit, unpushed until
  stable publication compatibility is reconciled.

Terminal diagnostics is outside this card; its later native resolution is bound by BPR-1836 and BPR-1837.

## Prepared correction

The cancellation predicate accepts only the bounded stopped-run summary with a
positive stdout count, zero stderr bytes and an optional integer exit code.
It rejects empty output, zero-byte claims, nonzero stderr, raw canary output,
private text before or after the summary and an overlong count. Two valid and
seven hostile cases execute in the cancellation course. Staleness still requires
empty output. All existing cleanup, descendant-stop and shutdown waits remain
unchanged, including their original budgets.

Complete private native compilation and full public replay are now supplied below.
Original failed or unreached checks retain their original results.

## Accepted native evidence

The corrected test sources at `a3cd2295c70819d0b710ff8e7ae294b2b8da6128`
completed the applicable full public contracts against unchanged D9 product
libraries on macOS, Windows and Ubuntu 26.04 under WSL2. Mac and Ubuntu each
completed 16 courses per configuration. Windows has 13 eligible courses per
configuration; the other three are excluded by their committed UNIX guards.
The source, compiler flags, headers, link inputs, helpers, registered arguments,
environment projection and final private-image hashes are bound in the external
receipts. All original registered and earlier private failures remain recorded.

Mac's 32-course proof is `main-refresh-r3-mac-private-oracles-public-r2/receipt.json`
(SHA-256 `9ca6e550b8f0b99074638a3d85bdf98bfa347a5d5081cfc6928177343b9492e7`).
Ubuntu's 32-course proof is `main-refresh-r3-ubuntu-private-oracles-public-r1/receipt.json`
on the native PC (SHA-256 `5439a651dcd19d858939523936ba197a98bfdce1a0ad925c316b2cf47b3ec294`).
Windows' 26-course semantic union is
`main-refresh-r3-windows-focused-26-course-union-r1.json`
(SHA-256 `6e233fdce03434dc6ef74959a7bddf84872320abc9b85be81f4a02862c42b152`).
Its two Debug 45-second readiness failures remain failed; observed progress
supported a separate, bounded 240-second running-phase watchdog to finish all
original cancellation/staleness business, descendant, cleanup, privacy and
shutdown assertions under the permanent Debug policy. No timing credit or
original-row PASS is inferred. The historical environment-observation limits
remain in that receipt.

The original terminal control later captured a category failure; its earlier
missing stderr remains a historical gap. Complete corrected category and
privacy courses now pass, including terminal recovery. The old private images
and failed results remain preserved. No product policy, release source,
configuration, cohort or any of the 30 producer asset bytes changed.

The four changed C++/header files still match the successful clang-format 22
read-only check. Documentation gates accompany this metadata closeout.
This remains a separate future-main follow-up: it is not pushed and must not
advance the frozen publisher before stable-operation compatibility is reconciled.

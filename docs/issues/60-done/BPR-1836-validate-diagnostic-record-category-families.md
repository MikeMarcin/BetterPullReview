---
schema: "implement-rfc.card.v1"
id: "BPR-1836"
title: "Validate diagnostic record categories by their operation family"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-1828"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"release_issue":"BPR-1811","mandate":"release-v041-full-stable-N8-oracle-followup","external_state":"/Users/jaeger/.codex/release-runs/v0.4.1-20261029","frozen_release_source":"d9d4a822dbf603c7bb22c83574f5a64d2bee5645","integration":"Independent test-oracle follow-up only; frozen publisher, product sources, libraries and producers remain D9."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a3cd2295c70819d0b710ff8e7ae294b2b8da6128","evidence":{"kind":"artifact","detail":"Narrow independent test-oracle correction and positive/hostile controls reviewed; all other ownership, schema/privacy or cleanup predicates retained. Frozen D9 products unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"a3cd2295c70819d0b710ff8e7ae294b2b8da6128","evidence":{"kind":"artifact","detail":"Full applicable Mac32/Windows26semantic/Ubuntu32 native proofs with source/image/argv/context seals. Original failures and Windows Debug45s limits retained; complete functional supplement under permanent policy has no performance credit. Exact proof hashes in card body and actions.json."}},{"criterion":"AC3","outcome":"pass","revision":"a3cd2295c70819d0b710ff8e7ae294b2b8da6128","evidence":{"kind":"command","detail":"Original native compiler/config/header/library/helper bindings verified; unchanged clang-format22 four-file receipt SHA 419902166e0d5d323da472bdc62f534d51a05b9330a6d73da51ea5a75b7a955d; documentation-before-finish passed. Separate unpushed future-main commit only; final moved-board gate follows before milestone completion."}}]
---

# Validate diagnostic record categories by their operation family

The N8 macOS Release logging and capability-failure courses fail the shared
independent JSONL oracle's category assertion. It special-cases github.*
and otherwise requires the complete operation as category. The accepted
production vocabulary now includes agent.* and other dotted families; its
documented category is the first operation component. No producer correction
is authorized or implied by this stale test assumption.

The helper has four direct test consumers: application_logging_tests.cpp,
quill_logging_tests.cpp, diagnostic_controls_tests.cpp and
session_retention_tests.cpp. The terminal-diagnostic recovery subcourse uses
the first binary. Its original missing child stderr remains a historical gap;
a later original control and complete corrected native replays are recorded below.

## Acceptance

- AC1: Correct only the independent test category predicate to the documented
  operation-family rule. Preserve every complete-schema, UTC/time ordering,
  event identity, severity, build identity and privacy assertion.
- AC2: Add meaningful valid-family and wrong-category controls. Replay all
  affected real logging courses against exact unchanged D9 product libraries;
  retain original raw failures, child context and unreached assertions.
- AC3: Bind private native test builds to the original flags, compiler, headers,
  libraries and helpers with explicit input/output seals. Complete focused
  format/documentation checks and commit this separate future-main fix without
  pushing or changing the frozen publisher before stable compatibility is
  reconciled.

## Prepared correction

The test helper now independently compares category with the first operation
component. The production vocabulary's 23 operation names are listed explicitly
in focused controls with their expected families, including the newer agent,
credential, updater, storage, workspace, search, image, notification, tour and
cycle families. Every case rejects a wrong category, and each dotted operation
also rejects its entire operation name as category. This does not call the
production category helper or change any other JSON record predicate.

Native builds and all applicable full public courses are now supplied below.
Their results are independent evidence; original failures are not relabelled.

The original failed category assertion prints no operation/category values, so
the specific first offending family is unknown. The earlier terminal wrapper
also discarded its child's stderr and exact exit value. Neither historical gap
is reconstructed or attributed to agent events; the complete documented
first-component rule and all 23 operation controls bind the correction.

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

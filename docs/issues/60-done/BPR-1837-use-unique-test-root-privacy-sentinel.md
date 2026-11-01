---
schema: "implement-rfc.card.v1"
id: "BPR-1837"
title: "Use the unique temporary parent as the log privacy sentinel"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-1831"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"release_issue":"BPR-1811","related_cards":["BPR-1836"],"mandate":"release-v041-full-stable-N8-oracle-followup","external_state":"/Users/jaeger/.codex/release-runs/v0.4.1-20261029","frozen_release_source":"d9d4a822dbf603c7bb22c83574f5a64d2bee5645","integration":"Test privacy-oracle correction only; frozen product sources, publisher, configuration and producers remain D9."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a3cd2295c70819d0b710ff8e7ae294b2b8da6128","evidence":{"kind":"artifact","detail":"Narrow independent test-oracle correction and positive/hostile controls reviewed; all other ownership, schema/privacy or cleanup predicates retained. Frozen D9 products unchanged."}},{"criterion":"AC2","outcome":"pass","revision":"a3cd2295c70819d0b710ff8e7ae294b2b8da6128","evidence":{"kind":"artifact","detail":"Full applicable Mac32/Windows26semantic/Ubuntu32 native proofs with source/image/argv/context seals. Original failures and Windows Debug45s limits retained; complete functional supplement under permanent policy has no performance credit. Exact proof hashes in card body and actions.json."}},{"criterion":"AC3","outcome":"pass","revision":"a3cd2295c70819d0b710ff8e7ae294b2b8da6128","evidence":{"kind":"command","detail":"Original native compiler/config/header/library/helper bindings verified; unchanged clang-format22 four-file receipt SHA 419902166e0d5d323da472bdc62f534d51a05b9330a6d73da51ea5a75b7a955d; documentation-before-finish passed. Separate unpushed future-main commit only; final moved-board gate follows before milestone completion."}}]
---

# Use the unique temporary parent as the log privacy sentinel

After the independent category predicate is corrected, the private macOS
Release application-logging course reaches its later privacy assertion and
fails. The course assumes root.filename() is a unique random token, but
private_data_root deliberately returns <QTemporaryDir>/data. Searching every
log byte for the fixed word data rejects path-free storage.database events.

## Acceptance

- AC1: Preserve full native-root, repository and all other privacy/schema
  checks. Compare case-folded full-root spellings and the actual unique
  randomized temporary-parent leaf, with safe storage.database positive
  evidence and full/random-root disclosure, slash and case negative controls.
- AC2: Replay complete affected native public courses against unchanged
  product inputs. Preserve the old private image, failure and source identity;
  no privacy or terminal acceptance is inferred before the full courses pass.
- AC3: Rebuild only this changed test translation unit/object/link in a distinct
  private version, with original flags, helpers, headers and libraries sealed.
  Retain valid unrelated images/objects, complete focused format/documentation
  checks and leave the future-main commit unpushed until stable compatibility
  is reconciled.

## Prepared correction and current proof frontier

The oracle keeps the earlier exact native-root and repository checks. Its
additional case-insensitive check now binds the full root and the randomized
temporary-parent leaf, after asserting that this sentinel belongs to the
course's explicit temporary-directory pattern. One path-free storage.database
record is admitted; native-root, case-folded generic-root, alternate-slash and
unique-parent-only disclosures are all rejected.

The old private Release application-logging image is preserved under
main-refresh-r3-mac-private-oracles-build-r1 with its exact source/image seals.
Its normal and terminal-recovery courses reach this privacy failure after the
category correction. The original D9 terminal control separately repeats with
captured child exit 1 and category-failure stderr; its earlier historical
stderr gap remains unchanged. New private compilation and complete affected
privacy/terminal courses have now passed as recorded below, with original failed
results retained separately.

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

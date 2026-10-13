---
schema: "implement-rfc.card.v1"
id: "BPR-1420"
title: "Shared Unicode source matcher and repository path filters"
status: "Done"
rfcs: ["RFC-BPR-0077"]
depends_on: ["BPR-1419"]
metadata: {"milestone":"shared-matching","source_id":"DRAFT-0016","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","mandate_revision":"find-and-find-in-files-v1-r1","milestone_plan":{"id":"find-and-find-in-files-v1","path":"docs/rfcs/plans/find-and-find-in-files-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c88131c9d0ca331da95eab1d701de718ef557f9e","evidence":{"kind":"observation","detail":"Shared public compiler/scanner and pane execution: Unicode 17 simple fold and L/M/N word rules, exact LF/original UTF-8 spans, nonoverlap, zero-width scalar progress and supported RE2-subset. Actual RE2 oracle passes 4891 span comparisons; malformed/unsupported queries clear current host records.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m2-scoped-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-self-review.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-re2-reference.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-negative-control.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-measurement.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-tidy.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-format.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-unicode.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-package.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-documentation.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-documentation-tests.json"]}},{"criterion":"AC2","outcome":"pass","revision":"c88131c9d0ca331da95eab1d701de718ef557f9e","evidence":{"kind":"observation","detail":"Compiled case-sensitive path policy parses nested braces, classes, commas and escapes; basename/root/globstar semantics, either-side include and exclude wins pass. Invalid patterns compile no program, and resource refusal stays an explicit path decision before corpus reads.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m2-scoped-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-self-review.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-re2-reference.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-negative-control.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-measurement.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-tidy.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-format.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-unicode.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-package.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-documentation.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-documentation-tests.json"]}},{"criterion":"AC3","outcome":"pass","revision":"c88131c9d0ca331da95eab1d701de718ef557f9e","evidence":{"kind":"observation","detail":"Unified scans actual source versions through the existing validated semantic index and displayed row order, removes exact unchanged occurrences, excludes hidden left whitespace-equal text, and cannot fabricate cross-version multiline matches. Same-entry identical blob and bytes scan once with both side locations; different blobs remain separate.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m2-scoped-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-self-review.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-re2-reference.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-negative-control.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-measurement.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-tidy.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-format.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-unicode.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-package.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-documentation.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-documentation-tests.json"]}},{"criterion":"AC4","outcome":"pass","revision":"c88131c9d0ca331da95eab1d701de718ef557f9e","evidence":{"kind":"observation","detail":"Compile, VM, logical text, indexes, records and previews have explicit 64 MiB owned and 16 MiB result ceilings plus deterministic instruction/nesting/work limits. Compile and regex execution stops are polled within 1024 steps; resource terminals retain honest prefixes. Core/host/source-span checks run without QApplication; large-line structural and mutation evidence recorded.","references":["docs/issues/_checkpoints/find-and-find-in-files-v1/m2-scoped-integration.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-self-review.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-re2-reference.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-negative-control.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-measurement.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-tidy.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-format.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-unicode.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-package.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-documentation.json","docs/issues/_checkpoints/find-and-find-in-files-v1/m2-documentation-tests.json"]}}]
---

# BPR-1420: Shared Unicode source matcher and repository path filters

## Acceptance

- **AC1:** Both commands share deterministic literal and bounded RE2-subset semantics: Unicode simple fold, Unicode word characters, nonoverlap, zero-width progress, multiline LF view and exact original UTF-8 spans.
- **AC2:** Include/exclude glob parsing follows comma/braces/classes/escaping and either-side include/exclude-wins rules; invalid patterns are refused before reads.
- **AC3:** Unified scanning follows actual version-contiguous source ranges, removes duplicate unchanged hits and cannot fabricate cross-version multiline matches; identical-blob same-entry deduplication is exact.
- **AC4:** Compile, scan, preview and result memory have explicit ceilings and cancellable bounded work; source-span tests need no QApplication.

## Implementation and evidence

Completed on exact product candidate `c88131c9d0ca331da95eab1d701de718ef557f9e`
under `find-and-find-in-files-v1-r1`. The
[acceptance](../_checkpoints/find-and-find-in-files-v1/m2-acceptance.json),
[scoped integration](../_checkpoints/find-and-find-in-files-v1/m2-scoped-integration.json),
[self-review](../_checkpoints/find-and-find-in-files-v1/m2-self-review.json) and
[RFC ledger](../../verification/RFC-BPR-0077.md) bind all four M2 criteria.
The [source-search contract](../../architecture/source-search.md) documents
syntax, source plans, exact records, limits and remaining corpus/UI ownership.

The public core/host paths, both architecture checks, touched format/tidy 22,
Unicode regeneration/reference/negative controls, package notice regression and
documentation gates pass. No QApplication, live desktop or remote mutation is
involved. No attended decision is required for this bounded matching milestone.
Full RFC AC01–AC08 and the existing failed broad-core obligations remain pending.

## Continuation

M2 stops at shared matching. [[BPR-1421]] owns the next exact epoch corpus and
aggregate streaming/acquisition work; the coordinator starts a fresh implementer.
The [canonical checkpoint](../_checkpoints/find-and-find-in-files-v1/continuation.md)
preserves candidate, commands, whole-pack gaps and next action.
The [import checkpoint](../_checkpoints/find-and-find-in-files-v1/import-r1.md)
retains the original main-import authority and mapping.

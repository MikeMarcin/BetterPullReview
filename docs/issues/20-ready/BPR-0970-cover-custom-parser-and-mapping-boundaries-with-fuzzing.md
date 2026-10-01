---
schema: "implement-rfc.card.v1"
id: "BPR-0970"
title: "Cover custom parser and mapping boundaries with fuzzing"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC0013 closeout dependency","source_candidate":"3efe61dfcf6874d35400bf777a7e86dbbf0a4693","milestone_plan":{"id":"bpr0970-parser-fuzzing","path":"docs/rfcs/plans/bpr0970-parser-fuzzing/milestone-plan.json"}}
---

# Cover custom parser and mapping boundaries with fuzzing

## Transferred obligation

Original RFC0013 fuzzing goal and acceptance row 7, transferred in full.

Owner amendment r4 on 2026-09-21 transfers this work from RFC0013 terminal
acceptance. The [amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its provenance. This card is unfinished; no observation is supplied by
the transfer and it creates no RFC0014 release qualification.

Deterministic parser, malformed-input and property tests are valuable existing evidence, but the RFC0013 closeout inventory found no complete registered fuzz targets/corpora. They are not fuzz acceptance. This card preserves the entire unfulfilled requirement across diff parsing/mapping, Markdown, settings/themes, API normalization, Git output and AI structured output.

## Acceptance

- AC1: Inventory every custom entry point in the seven named families, existing deterministic tests and uncovered trust boundaries; bind the inventory to a source revision and actual build registrations.
- AC2: Add bounded runnable fuzz targets and representative/hostile corpora for uncovered custom parsers and mappings, preserving production validation and authority boundaries without test backdoors.
- AC3: Run and record reproducible sanitizer-backed campaigns with input/work/memory limits, seeds, duration and findings. Fix concrete defects with regression cases; no finite campaign claims absence of all bugs.

## Continuation

Start with the inventory or diagnosis above on a separately assigned exact
candidate. Reuse relevant passing evidence with its actual input limits.
Use safe fixtures; no live account, private-target access, remote mutation or
release is authorized by this card. Required native or human evidence remains
pending until actually observed.

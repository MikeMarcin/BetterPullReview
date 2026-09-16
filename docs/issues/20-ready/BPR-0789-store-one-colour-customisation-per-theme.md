---
schema: "implement-rfc.card.v1"
id: "BPR-0789"
title: "Store one colour customisation per theme"
status: "Ready"
rfcs: ["RFC-BPR-0034"]
depends_on: ["BPR-0770"]
metadata: {"pack":"bpr-shell-surfaces-v1","milestone":"appearance","acceptance":["C1"],"milestone_plan":{"id":"bpr-shell-surfaces-v1","path":"docs/rfcs/plans/bpr-shell-surfaces-v1/milestone-plan.json"}}
---

# BPR-0789: Store one colour customisation per theme

## Outcome

Give the appearance settings record one stored palette per variant; bump the appearance settings store schema with a transactional, restart-tested migration that files the existing single palette under its recorded variant; key the Qt appearance profile's override table by variant; scope commit, reset, import and export intents to the resolved variant supplied by the composition; publish per-variant customised counts in the projection.

## Acceptance

- Prior-schema fixtures with a stored light or dark palette migrate with nothing lost and the other variants render built-ins.
- Committing on one variant leaves the others' customisations intact across reopen; export and import name the variant they act on.
- Future or malformed schema versions fail closed at the appearance store's scope; no widget reads or writes the store.

## Evidence

Record the focused command or gesture, the direct surface, the result, the producing revision and the remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native or attended verdict from a fixture or a mockup.

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference is authorized by this card. The core decides, the host performs, Qt presents: no widget reaches a store, a vault, a provider or a process. No stylesheet or hard-coded palette. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.

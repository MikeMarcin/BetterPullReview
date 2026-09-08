---
schema: "implement-rfc.card.v1"
id: "BPR-0698"
title: "Separate profile appearance preferences from window classification"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696"]
---

# Separate profile appearance preferences from window classification

## Scope

Extract independent window syntax-classification owners from the combined
appearance host. Keep one durable appearance preference owner per profile with
bounded explicit consumer attachment/detachment; each consumer resolves shared
accepted settings against its own native appearance observation. Keep the current
application interface as a delegating compatibility facade where useful, so the
shipped path exercises the extraction. Durable restore, mode/palette/typography,
reset, theme import/export and close-cancellation behavior remain intact.

Classification stays bound to each window's own epoch/blob capability, cache and
request lifecycle. Cancelling one window must not cancel its sibling or profile
settings effects. No second settings store and no Qt-owned persistence.

This independent foundation runs alongside BPR-0697. Actual same-profile Qt
window composition and one Settings window per profile require the later profile
runtime extraction; this card does not claim those behaviors complete.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, CMake, bookkeeping and acceptance. Workers receive
exclusive appearance-related source/tests and preserve parallel GitHub work.

## Acceptance

Two window consumers receive accepted preferences from one durable owner while
retaining independent classification caches, native observations and cancellations.
Detach/close rejects late window results without clearing sibling data. Existing
restore, preview/import/export/reset and persistence-failure contracts pass;
appearance changes do not reparse unchanged code. One existing composed appearance
gesture path proves the shipped facade uses the extracted owners. Focused host
checks and risk-focused independent review cover consumer lifetime and fan-out.

## Evidence

Scout identified the combined owner in appearance_application_host and the
per-window epoch/blob and Qt appearance_profile dependencies. No implementation
or new acceptance check has run for this card. Temporary logs from completed
BPR-0696 were consumed and removed; its raster remains for attended preparation.

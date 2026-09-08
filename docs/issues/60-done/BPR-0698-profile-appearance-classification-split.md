---
schema: "implement-rfc.card.v1"
id: "BPR-0698"
title: "Separate profile appearance preferences from window classification"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696"]
acceptance: [{"criterion":"Shared appearance preferences with independent window classification and safe consumer lifetime","outcome":"pass","revision":"HEAD plus explicit finish paths","evidence":{"kind":"observation","detail":"Existing appearance and new consumer suites pass (0.30 s each); four changed TUs pass tidy/format. Independent review found no blockers. Actual appearance gesture/restart course passes (84.73 s) after fixing its private-root fixture setup. Settings effect boundary (0.63 s) and management public path (1.40 s) pass. Shared Settings presentation and multiple Qt windows remain later scope."}}]
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

Implemented a profile preference owner and an independent classifier behind the
existing facade. The new shared constructor attaches to the explicit profile
owner; native observations and resolved publications remain per consumer. The
32-consumer bound and opaque identities fence detached and reentrant delivery.

Existing appearance-host and new consumer tests pass (0.30 s each), covering one
write/two consumers, independent observation/cache/cancel, late detach, transient
origin, reentrant detach, capacity and retired identity. Four changed TUs pass
canonical tidy, rebuild and format checks. Independent medium review found no
evidence-backed blocker in lifetime, fan-out or origin handling.

The actual appearance gesture/restart course passes (84.73 s). Its old fixture
created a 0755 root, which the admitted-root contract correctly refused; the test
now uses the existing private-directory helper for its own temporary root.
Settings effect-boundary (0.63 s) and management public-path (1.40 s) checks pass.
The application builds with the extracted facade. The gesture root and temporary
extraction files were removed after use; ordinary build outputs remain. No actual
same-profile Qt window or native Settings attendance is claimed here.

Implementation decision: ordinary consumer cancel retains its preference
subscription and cancels only its classifier and initiating theme import.
Shutdown/destruction detach that consumer. Only explicit profile-owner shutdown
cancels shared preference effects. Preview/export results retain the initiating
live attachment; accepted durable settings fan out to all live attachments.

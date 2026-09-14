---
schema: "implement-rfc.card.v1"
id: "BPR-0759"
title: "Implement bounded native motion observation"
status: "Done"
rfcs: ["RFC-BPR-0031"]
depends_on: ["BPR-0758"]
metadata: {"pack":"bpr-dif-integration-v1","milestone":"runtime-foundation","acceptance":["B4","B9"],"milestone_plan":{"id":"bpr-dif-integration-v1","path":"docs/rfcs/plans/bpr-dif-integration-v1/milestone-plan.json"}}
acceptance: [{"criterion":"System unknown/failure resolves reduced; explicit Full remains available.","outcome":"pass","revision":"e337c1fa340630fb5b8085a7177a3422d98931f4","evidence":{"kind":"observation","detail":"Core policy and application profile-consumer tests pass; offscreen native source reports unsupported, whose System resolution is reduced. The composed fixture Appearance gesture reports hidden:full:full."}},{"criterion":"Change delivery and observer shutdown are focused-tested.","outcome":"pass","revision":"e337c1fa340630fb5b8085a7177a3422d98931f4","evidence":{"kind":"command","detail":"qt_motion_observer passes with injected initial/change/dedup, two subscribers, same-delivery unsubscribe fence, destroyed lifetime and handler release. App composition shares one observer across profile windows and unregisters on window destruction; application_appearance_theme_customization_contract and a direct fixture gesture pass."}},{"criterion":"Native support claims remain separate from injected tests.","outcome":"pass","revision":"e337c1fa340630fb5b8085a7177a3422d98931f4","evidence":{"kind":"observation","detail":"QT_QPA_PLATFORM=cocoa native probe observed a macOS AppKit full-or-reduced read; offscreen returned unsupported. Installed AppKit SDK exposes read and notification API. An actual OS preference toggle/change notification was not exercised and remains pending at later native verification; Windows/Linux are reported unsupported. No settings subprocess, polling or OS preference change occurred."}}]
---

# BPR-0759: Implement bounded native motion observation

## Outcome

Add/read the reliable per-platform reduction observation through an injected Qt edge. Unsupported is a first-class result; no settings subprocess or polling.

## Acceptance

- System unknown/failure resolves reduced; explicit Full is available.
- Change delivery and observer shutdown are focused-tested; native support claims remain separate from injected tests.
- Any attended OS preference change is restored and platform gaps are recorded.

## Evidence

Record focused command/gesture, direct surface, result, producing revision, and remaining limitation. Evidence is **not run** at pack authoring. Reuse still-valid prior results; never infer a native/art verdict from a fixture.

## Boundaries

No GitHub/GHES mutation, model run, credential access or shared-skill replacement is authorized by this card. Any required creative edit remains offline and separately recorded; ordinary builds and the product perform no image generation. Follow the milestone exit rather than running an exhaustive app-wide course for this slice.

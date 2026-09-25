---
schema: "implement-rfc.card.v1"
id: "BPR-1197"
title: "Restore copyable comparison explanations beside document tabs"
status: "Done"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1132"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"deterministic macOS integration failures at f6de40b6, 2026-09-25","reservation":"docs/issues/_reservations/BPR-1197--BPR-1197--cdb13ac29612.json"}
acceptance: [{"criterion":"AC1 The real missing-endpoint and comparison-limit public paths clear stale source and present the complete visible explanation, with an actual Copy gesture and full accessible text. A supported file recovers normally.","outcome":"pass","revision":"24cee6dfc85fec9c4d1d107bd6528ebf81b381df","evidence":{"kind":"command","detail":"Serial fail-before/pass-after for application_unavailable_diff_public_path and application_selected_file_safety_public_path. Final passes 16.86 s and 25.96 s. Both select the full explanation, deliver the platform Copy key through Qt, and assert the complete clipboard value and QAccessible description/visibility. Existing exact refusal, stale-source clearance, binary distinction and supported-file recovery assertions remain. Before/after full-window PNGs inspected."}},{"criterion":"AC2 The tab status preserves the complete tooltip and accessible description, allows legitimate single-line elision, and keeps the 28-pixel strip at narrow and wide windows and 100, 125 and 200 percent scale.","outcome":"pass","revision":"24cee6dfc85fec9c4d1d107bd6528ebf81b381df","evidence":{"kind":"command","detail":"qt_document_tabs_scale_100/125/200 pass on final source at 1280, 1100 and macOS 900-pixel floor. The tests assert full text/tooltip/QAccessible description, room for at least an ellipsis plus padding within the 28-pixel strip, complete literal wrapped explanation without clipping, and normal recovery. A zero-width status found at 1100 during the sweep was corrected; renders inspected."}},{"criterion":"AC3 Successful text comparisons retain their prior layout budget; focused Qt and composed checks, formatting, tidy and the documentation gate pass.","outcome":"pass","revision":"24cee6dfc85fec9c4d1d107bd6528ebf81b381df","evidence":{"kind":"command","detail":"Final affected set 7/7 in 124.32 s includes architecture_boundaries and all four UIforETW profiles at unchanged 872 folded / 838 open. Six unchanged window/focus-order scale passes are retained from the initial correction run. Build, six-file format check, six-TU tidy, documentation gate and 64 documentation tests pass. Existing naming/style tidy warnings are unchanged. Windows/Linux and native accessibility evidence remain owed on BPR-1135."}}]
---

# BPR-1197: Restore copyable comparison explanations beside document tabs

## Outcome

An unavailable comparison presents its complete explanation visibly, with
wrapping, selection, Copy and accessibility. The compact document-tab status
keeps its full tooltip and accessible description when its painted line elides.
Successful text comparisons retain the compact header and their reading area.

## Acceptance

- The real missing-endpoint and comparison-limit public paths clear stale source
  and present the complete visible explanation, with an actual Copy gesture and
  full accessible text. A supported file recovers normally.
- The tab status preserves the complete tooltip and accessible description,
  allows legitimate single-line elision, and keeps the 28-pixel strip at narrow
  and wide windows and 100, 125 and 200 percent scale.
- Successful text comparisons retain their prior layout budget; focused Qt and
  composed checks, formatting, tidy and the documentation gate pass.

## Boundaries

This follows terminal [[BPR-1132]] without reopening it. It owns only Qt status
presentation and affected tests. No application request/cache semantics change.
Work is isolated from the frozen m2 attended candidate and primary checkout.
Windows/Linux execution and native accessibility acceptance remain owed on
[[BPR-1135]]; macOS automated evidence does not supply those decisions.

## Starting evidence

The full macOS suite at `f6de40b633ea8b879d3643ad5a259169fd976975`
and serial reruns fail `application_unavailable_diff_public_path` at Copy support
and `application_selected_file_safety_public_path` at the hidden label's height.
The replacement tab status retains full text for its tooltip and accessibility,
but the old wrapping selectable label is hidden unconditionally. Inspect a
fresh missing-material render and retain fail-before/pass-after evidence.

## Implementation and diagnosis, 2026-09-25

The coordinator's amended assignment owns this follow-up in the isolated
`codex/bpr1132-status-refusal` worktree from `f6de40b6`. The reserved BPR-1197
is claimed; terminal BPR-1132 is unchanged. The primary checkout and frozen
m2 attended candidate are outside this writer's scope.

Both original failures reproduce serially. The retained before screenshot
shows empty source panes and an elided status beside the file tab. Tests
strengthened before the product correction also fail: the two public paths
and the 100-percent tab course cannot find a complete visible explanation.

The window now shows the existing wrapping, selectable explanation when its
comparison has no text content, including missing material, refused computation
and binary content. The label renders literal text. The compact tab status
retains its full tooltip and accessible description, and normal text recovery
removes the full explanation. This follows the existing typed content state;
no wording is parsed and no request, cache or provider semantics change.

The new width sweep also exposed a zero-width tab status at a 1100-pixel window
with all drawers open and a tab carrying its own pair. Its minimum size now
reserves the font's ellipsis plus the painter's horizontal padding. The full
explanation remains visible below that compact line at the 900-pixel floor.
The tests keep the fixed 28-pixel header and verify its elision bounds, complete
tooltip and accessibility description, plus full literal wrapped text without
clipping. Both real public paths select the explanation and deliver the
platform Copy key, then compare the clipboard with the complete text.

## macOS verification

Product revision: `24cee6dfc85fec9c4d1d107bd6528ebf81b381df`. macOS 26.6.2 arm64,
Qt 6.11.2, Debug, offscreen; all interactive courses ran serially.

| Check | Result |
| --- | --- |
| Original two public paths on `f6de40b6` product | Both fail, 56.44 s; original Copy and hidden-height failures reproduced |
| Strengthened tests before product correction | Both public paths and tab scale 100 fail, 42.72 s |
| Initial correction set | 9/12 pass; three new scale assertions expose zero-width compact status, subsequently fixed |
| Final affected set | 7/7 pass in 124.32 s: architecture, tab scales 100/125/200, both public paths, and UIforETW |
| Unchanged window and focus-order scales | All six pass in the initial correction set; retained after the compact status minimum-width fix |
| UIforETW final appearance profiles | Light, dark, high-contrast light/dark all retain 872 folded / 838 open at 1280×1000; tab strip 28 |
| Build, format and tidy | Pass; six changed translation units checked. Existing naming/style warnings remain unchanged |
| Documentation | Gate passes; all 64 documentation tests pass |

The two real public paths retain their refusal and recovery assertions. This
acceptance does not replace the original height assertion with one on an
elided single line: the full explanation must still fit visibly, and the
compact tab status has its own bounded elision and complete-text checks.

Exact commands, failures, final results, artifact hashes, inspection and
integration instructions are in the
[durable checkpoint](../../verification/BPR-1197-copyable-comparison-status-20260925.md).
Logs and PNGs are under
`/Users/jaeger/.codex/tmp/bpr1132-status-b8w8qmsa`, with an artifact manifest in
`verification.json`. The frozen m2 attended app/session remains unchanged.
Windows/Linux execution and native VoiceOver/Narrator remain pending on
[[BPR-1135]]; no native result is inferred from Qt accessibility objects.

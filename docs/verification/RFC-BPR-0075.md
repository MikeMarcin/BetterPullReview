# RFC-BPR-0075 verification

Mandate `title-row-publication-r1`; immutable RFC_BASE_SHA
`d9c93c2dbfe29c5fcd6e11b21df4be0a0aead5f1`. One milestone,
`title-row-publication-v1/settled-title-row`, covers all four criteria and upkeep.
The [live import](../rfcs/assets/title-row-publication-v1.LIVE_IMPORT.md) retains
original draft bytes and the authored grouping. The final candidate is
`0f039e775900153001b4c7cce04431a5d4278d86`; product correction `ca21f6f5f911b68a2dc608dd0d67f0bd5dc034e7` is unchanged by the final test-only
selected-file strengthening.

The required `frontend-design` skill was loaded from the installed Claude plugin
before product edits. The brief preserves the accepted native palette, font,
layout, icon and width yield; no new visual design was adopted.

## Result

Accepted publication and clear call the existing `apply_strip_width` path directly.
The prior guard consulted the identity action's stale hidden visibility and skipped
the pass that could show it. The correction measures current identity/context/surface
at current width and refreshes the existing text/action projection. Browser admission
remains core/host-owned.

| Criterion | Direct evidence | Result |
|---|---|---|
| AC01 | Real public workbench_window publication enters loading with two received-page observations and normal core commands before accepting content. Identity and adjacent browser button appear at unchanged 1920/1280/1100/900 widths, before resize/Home gestures. New assertion fails against unchanged baseline product; existing direct-acceptance control passes. | pass on `0f039e775` (macOS/offscreen) |
| AC02 | Same-PR title/context replacement, identical republication, new-authority PR replacement, removal of browser context and clear settle at unchanged width. Click dispatch names only the latest accepted markdown context; clear hides and empties identity and withdraws the browser action. | pass on `0f039e775` (macOS/offscreen) |
| AC03 | Preserved content keeps Home, Discuss and the actual selected Files index/identity plus position. A retained local cycle keeps its own identity without browser action; return shows the updated PR row immediately, and a new PR replaces the cycle normally. Existing composed personal-discovery and Markdown/browser paths pass on unchanged product source. | pass on `0f039e775` (macOS/offscreen) |
| AC04 | Registered 100/125/200 scale title-row courses pass, including exact order, accessibility, adjacent button, roomy/compact/macOS 900-point floor and pixel-by-pixel width yield sweep without overflow. Repeated same-content publication has identical geometry. Offscreen accepted-row captures generated at four widths per scale and inspected at roomy/floor sizes. | pass on `0f039e775` (macOS/offscreen) |

## Commands and controls

- [Final changed-window checks](../issues/_checkpoints/title-row-publication-v1/final-title-row.json): six `qt_workbench_{title_row,open_on_github}_scale_{100,125,200}` rows pass at the final candidate.
- [Integration pass](../issues/_checkpoints/title-row-publication-v1/candidate-verification.json): the six changed rows, three `qt_composed_markdown_{1_0,1_25,2_0}` rows, `application_personal_discovery_public_path`, `core_dependency_boundary` and `architecture_boundaries` pass. These twelve results are retained; the integration command also records three baseline failures below.
- [Failing-before control](../issues/_checkpoints/title-row-publication-v1/failing-before.json): the new public-window assertion fails against the original product at accepted identity visibility without corrective resize/Home navigation.
- Render courses pass at each registered scale ([100](../issues/_checkpoints/title-row-publication-v1/render-scale-100-retry.json), [125](../issues/_checkpoints/title-row-publication-v1/render-scale-125.json), [200](../issues/_checkpoints/title-row-publication-v1/render-scale-200.json)). [Render observations](../issues/_checkpoints/title-row-publication-v1/render-observation.json) retain capture digests and inspection limits; transient PNGs are consumed and removed.
- [Quality checks](../issues/_checkpoints/title-row-publication-v1/quality-checks.json): touched C++ passes clang-format 22.1.8 and diff whitespace checks. Source clang-tidy with the explicit macOS SDK and changed-line filter passes; this does not certify full-file or repository tidy. Initial tooling/setup failures and corrections are retained there.

## Baseline failures and limits

`qt_composed_github_scale_{100,125,200}` stops at the existing connection warning's
“at most 70 words” assertion. The [baseline product control](../issues/_checkpoints/title-row-publication-v1/composed-github-baseline.json)
reproduces the same failure with the changed product source restored to
`d9c93c2db`; all other product source and the composed-GitHub test are unchanged.
The candidate was restored byte for byte and focused binaries rebuilt. This failure
is outside title-row content publication; no trust-warning wording was changed.
The focused composed Markdown/browser and public Home paths pass independently.

Evidence is deterministic Qt offscreen rendering on macOS. It does not identify
the owner's installed binary, qualify native appearance or assert Windows/Linux
native observations. The accepted existing design, accessibility/shortcut contract
and browser admission are retained. No criterion in this RFC requires a new human,
native or independent verdict; no such acceptance is inferred.

## Documentation and upkeep

The existing workbench specification now records content-driven title-row settlement
and retained navigation. No new API, provider read, browser admission, authority,
persistence, threading/lifecycle, build/package policy or launch behavior is introduced:
the product diff changes only two Qt presentation invalidation sites. These categories
need no semantic document change. RFC/index/card and acceptance agree with the verified
candidate; the documentation gate runs for closeout. No remote mutations, release or
push occurred. Unrelated original dirty files and draft contents are preserved.

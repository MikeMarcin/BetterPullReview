# Additive reconciliation, not settled-RFC rewriting

## Contract precedence for this workstream

The live product's general architecture and safety rules still govern. New RFCs
0030/0031 govern their explicit deltas. Existing 0019/0020 requirements remain in
force where not superseded. The earlier animation prompt pack supplies choreography
and identity guidance; the generated PNGs supply actual candidate artwork, not proof
that every prompt constraint was met.

| Existing provision | Explicit new provision |
|---|---|
| 0019 exactly three initial clips, at most 48 content frames and 13.5 MiB complete strips | 0030 has exactly ten IDs, target 71 frames, bounded headroom to 76, complete pixels at most 24 MiB, unchanged 256-square runtime cells. |
| 0019 card in original identity vocabulary; downstream owner note | Prop-free neutral/base is mandatory. No runtime card erasure and no inseparable card variant in this catalog. |
| Prompt design asks for 512-square generative source frames | Actual sheets have smaller/fractional source grids. Native-resolution crops and reviewed 256-square canonical production frames are allowed without pretending they were generated at 512. Retain original/high-resolution edited intermediates outside the exact production inventory. |
| Older prose says every regenerated PNG file must have identical compressed bytes | Retain the later implemented canonical-pixel equality and separate byte-hash binding. Fix live descriptive docs as a conformance update; never edit historical review evidence. |
| 0020 only landing, no semantic cue mappings | 0031 retains Home decoration and adds constrained presentation cues only in an explicitly opened AI tools illustration slot. No avatar, code overlay, notification/toolbar mascot, review-author identity or mascot-generated conclusion. |
| 0020 one widget and application-wide preference in an earlier single-window model | One sprite widget per workbench, at most one active timer-bearing mascot in the active top-level presentation, shared process art cache, profile preference authority, independent window cue state. |
| 0020 no PR active means landing | Current Home can cover a retained review. Actual visible surface, not data nullness, determines eligibility. |
| 0020 waits for the original three-clip asset's formal terminal status | New runtime selection requires the **new revision's exact technical/provenance/art gates** under 0030. Do not force shipping the old card-bearing set first. Canonical RFC closeout remains separately governed by the live board. |
| Current build-boundary tool forbids all Dif runtime references | Selected Qt app graphs require the accepted catalog; core-only and ordinary build-tool isolation remain strict. |

## What is not superseded

Functional-core/host/Qt ownership, no autonomous review actions, privacy and provider
safety, metadata limits, offline immutable runtime resources, zero animation in
reduced/hidden/inactive states, truthful diagnostics, nonintrusive input/accessibility,
and digest-bound acceptance of altered art all remain.

## History and migration

Do not reopen old accepted decisions just to restate them. Record that the new source
revision is the consumer for this workstream; preserve old pack/RFC/verdict bytes as
history. Update mutable indexes, architecture descriptions, live asset README, tests,
and the import ledger to link to the new RFCs. Do not claim BPR-0291 approved new
sources, nor claim a new accepted asset rewrites that old verdict.

No backward-compatible runtime JSON reader is necessary: the app never reads the
manifest dynamically. Catalog/schema changes are compiled together. Keep any existing
legacy fixture/evidence support only where it is actually used, not a second shipping
asset tree. Formal closeout waits are reported separately from engineering readiness;
this pack does not authorize blanket deletion of prerequisite or platform gates.

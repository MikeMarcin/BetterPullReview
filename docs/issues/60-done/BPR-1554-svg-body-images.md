---
schema: "implement-rfc.card.v1"
id: "BPR-1554"
title: "Show SVG images in descriptions and comments through QtSvg"
status: "Done"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1549"]
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"inline-markdown-images-r1","governing_rfc":"RFC-BPR-0084","amendment":"RFC-BPR-0084 owner amendment 1 (2026-10-16)","criteria":["AM1-01"],"origin":"owner accepted Q1 in attended session 8a5e3c244f674294366fd8796da2897e, cursor 17","reservation":"docs/issues/_reservations/BPR-1554--BPR-1556--0108f19c75d6.json"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"06a06a2ea08242a7d57e2caed74afb094962e4a1","evidence":{"kind":"command","detail":"core_markdown_image svg_admission: signature after BOM/declaration/comment/doctype; fragment and embedded-image hrefs and url(#) admitted; http/file/relative/protocol-relative/data:text hrefs, external url(), @import, @font-face, SVG font, script, foreignObject, javascript:, entity and internal doctype subset blocked; depth 64, 20,000 elements and 1 MiB too_large with a within-bound negative control; unterminated element failed. qt_markdown_image_decoder: a gradient badge rasterizes at 20x10, Scale 2, 40x20 pixels, red at the centre; viewBox gives the size; external image, local file, external url(), script and entity refused; 20000-wide too_large; fragment reference admitted. Negative control: letting refused SVGs past the core fails 'an external image is refused'. fuzz_markdown selector 3 with admit_markdown_svg, seed 840085, 2,611,624 runs in 301 s, clean."}},{"criterion":"AC02","outcome":"pass","revision":"06a06a2ea08242a7d57e2caed74afb094962e4a1","evidence":{"kind":"command","detail":"application_markdown_images_public_path: the fixture's SVG badge loads at 90x20, Scale 2, no description placeholder; live course on MikeMarcin/BPR-Test#73: the camo-served shields.io badge loaded at 88x20 (rate remaining 4996 -> 4965 over the run)."}},{"criterion":"AC03","outcome":"pass","revision":"06a06a2ea08242a7d57e2caed74afb094962e4a1","evidence":{"kind":"command","detail":"cmake/BprDependencies.cmake requires Qt6::Svg for Qt builds; adapter links Qt6::Svg privately; tools/release.sh framework policy adds QtSvg.framework, package_runtime QT_LIBRARIES adds Svg, no SVG plugin admitted; packaging/dependencies.json QtSvg 6.11.2 (sha256 d594337f..., matching upstream .sha256); notices include QtSvg LICENSES; qt_records joins qtsvg's producer SBOM when QtSvg is deployed. package_runtime_contract, release_package_audit_contract, release_source_assets_contract, native_release_contract, dependency_modernization_contract pass."}}]
---

# BPR-1554: Show SVG images in descriptions and comments through QtSvg

Under [RFC-BPR-0084 owner amendment 1](../../rfcs/RFC-BPR-0084-inline-markdown-images.md#owner-amendment-1-2026-10-16-answers-to-q1-q3):
SVG body images, such as README badges, render through QtSvg's renderer after the core admits them as self-contained and bounded, rasterized statically at twice their size.

## Acceptance

- **AC01 (AM1-01):** Core and decoder tests show an admitted SVG rasterized at its size and twice its density, and each refused construct (external `href`, external `url()`, entity, script, `foreignObject`, `@import`, `@font-face`, over-bound size, elements or depth) ending in its state, with a negative control.
- **AC02:** The composed fixture course shows an SVG badge inline; the live course shows GitHub's camo-served SVG badge.
- **AC03:** The pinned Qt provider requires QtSvg; the package, release and capsule policies deploy the QtSvg library and no SVG plugin; notices and SBOM name QtSvg.

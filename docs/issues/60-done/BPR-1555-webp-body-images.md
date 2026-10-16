---
schema: "implement-rfc.card.v1"
id: "BPR-1555"
title: "Show WebP images in descriptions and comments through Qt's WebP plugin"
status: "Done"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1549"]
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"inline-markdown-images-r1","governing_rfc":"RFC-BPR-0084","amendment":"RFC-BPR-0084 owner amendment 1 (2026-10-16)","criteria":["AM1-02"],"origin":"owner accepted Q2 in attended session 8a5e3c244f674294366fd8796da2897e, cursor 17","reservation":"docs/issues/_reservations/BPR-1554--BPR-1556--0108f19c75d6.json"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"06a06a2ea08242a7d57e2caed74afb094962e4a1","evidence":{"kind":"command","detail":"qt_markdown_image_decoder: a still WebP decodes 5x3; an 8x6 lossless animated WebP keeps both frames with 120 ms and 30 ms delays, first frame opaque red; a VP8L header claiming 8000x8000 is too_large before decoding, with the unpatched still decoding as its negative control; another RIFF payload is unsupported; BMP and TIFF stay unsupported."}},{"criterion":"AC02","outcome":"pass","revision":"06a06a2ea08242a7d57e2caed74afb094962e4a1","evidence":{"kind":"command","detail":"application_markdown_images_public_path: the fixture's WebP photograph (240x120, image/webp) loads inline; markdown_images_fixture_contract regenerates the tree byte for byte and checks the RIFF/WEBP signature."}},{"criterion":"AC03","outcome":"pass","revision":"06a06a2ea08242a7d57e2caed74afb094962e4a1","evidence":{"kind":"command","detail":"BprQtProvider.cmake: BPR_QT_ADDON_MODULES qtimageformats; every arm (cache, BPR_QT_ROOT, each attempt) validates the add-ons; an essentials cache is amended in place with aqt --noarchives (observed on this host: qwebp and the qtimageformats SBOM appeared without a new base download); dependency_modernization_contract asserts the arms. package_runtime PLUGINS ship only qwebp (libqwebp.dylib, qwebp.dll, libqwebp.so); release.sh and the capsule admit libqwebp.dylib; dependencies.json QtImageFormats 6.11.2 LGPL-3.0-only AND BSD-3-Clause (sha256 cecd8900..., matching upstream); notices include its LICENSES and libwebp's COPYING, PATENTS and AUTHORS (package_runtime_contract asserts libwebp's licence travels)."}}]
---

# BPR-1555: Show WebP images in descriptions and comments through Qt's WebP plugin

Under [RFC-BPR-0084 owner amendment 1](../../rfcs/RFC-BPR-0084-inline-markdown-images.md#owner-amendment-1-2026-10-16-answers-to-q1-q3):
WebP body images decode through Qt's `qwebp` plugin from `qtimageformats` (libwebp, BSD-3-Clause; the owner's library approval) within the same bounds as GIF.

## Acceptance

- **AC01 (AM1-02):** Decoder tests show still and animated WebP within the bounds and an over-bound one refused, with a negative control.
- **AC02:** The composed fixture course shows a WebP image inline.
- **AC03:** The pinned Qt provider acquires `qtimageformats` (amending an older cache in place); the package, release and capsule policies ship only `qwebp` from it; dependencies, notices and SBOM name QtImageFormats and libwebp.

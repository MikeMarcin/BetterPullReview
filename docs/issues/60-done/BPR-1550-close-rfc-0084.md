---
schema: "implement-rfc.card.v1"
id: "BPR-1550"
title: "Verify and close RFC-BPR-0084"
status: "Done"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1549"]
platforms: ["mac"]
closeout_for: ["RFC-BPR-0084"]
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"image-acceptance"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"1f849cee42bb31f33d905d1ff77d504a29450504","evidence":{"kind":"command","detail":"tests/scripts/markdown_image_live_course.py --pull 73 on MikeMarcin/BPR-Test: 4 private-user-images and 6 camo images loaded (user-attachments, <img width=420>, a table of <img width=300>), the gone image failed; X-RateLimit-Remaining 4998 -> 4993. Rerun at the amendment: the camo-served SVG badge also loaded."}},{"criterion":"AC02","outcome":"pass","revision":"1f849cee42bb31f33d905d1ff77d504a29450504","evidence":{"kind":"observation","detail":"Owner accepted, no note, in fresh attended session 8a5e3c244f674294366fd8796da2897e (decision eb4887eb, fingerprint 9ca1d135..., handed back at cursor 17), on frozen root bpr1550-markdown-images-v001 with GitHub-vs-app renders of PR #73: 'Images in descriptions and comments show the way GitHub shows them.' The same session accepted Q1-Q3 (owner amendment 1, BPR-1554 to BPR-1556)."}},{"criterion":"AC03","outcome":"pass","revision":"1ffe0cfa88acaf34fde4100d061058265dff6736","evidence":{"kind":"command","detail":"Every RFC criterion and amendment criterion has recorded evidence in the closeout checklist and the cards; full dev ctest at 06a06a2ea merged with main (752 tests): 739 passed first time; the 13 others, all load-sensitive or environment rows, passed when rerun serially (application_large_review_launch and _active_work on a second single rerun), except documentation_structure, whose NEXT_ID was behind main until the merge at 1ffe0cfa8; after the merge, the 43 composition, image, documentation, packaging, settings, document-tabs and label-recovery rows pass and verify_documentation reports DOCUMENTATION_OK."}}]
---

# BPR-1550: Verify and close RFC-BPR-0084

Live course on `MikeMarcin/BPR-Test`, frozen attended build, the owner's
verdict and closeout ([RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md)).

## Acceptance

- **AC01:** The live course renders a PR whose description holds a
  `user-attachments` image and an `<img width>` tag, within the observed
  rate-limit margin.
- **AC02:** The owner answers, in a fresh short attended session with a frozen
  build and GitHub comparison renders: "Images in descriptions and comments
  show the way GitHub shows them." (Accept/Reject).
- **AC03:** Every RFC criterion has recorded evidence; documentation gate
  passes.

RFC base: `94699fd51c2644c3ebb38743f623927d179859a1`.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** Through the shipped composition, the PR description and conversation posts of a fixture show Markdown, HTML, linked, list, table, quote, `width`/`height`/percentage and `<picture>` images inline as defined in "What renders"; comparison renders against GitHub's page for the same body show matching placement and size. | Pass. Evidence: application_markdown_images_public_path (composed fixture markdown-images-v1: Markdown, `<img>` with width/height/percentage, linked, list, table and `<picture>` images inline in the description and both comments) at 04090b526; live course and GitHub page renders of BPR-Test#73 at 1f849cee4. Owner comparison pending (AC07). |
| **AC02:** A loading image shows its reserved box, then the body reflows once keeping scroll position and selection; an animated GIF animates; images stay within the reader's width and crisp at 1.0x, 1.25x and 2.0x; resizing, re-rendering and an unchanged refresh make no request once cached; a late result for another body, revision or window is never drawn. | Pass. Evidence: qt_markdown_images_1_0/1_25/2_0 (reserved box, one reflow with scroll compensation, selection kept, GIF animates while visible, width bound and crisp at each scale, stale projection ignored); application_markdown_image_host (cache hit and repeated request make no request, superseded and other-scope results never drawn). |
| **AC03:** Blocked schemes, `data:`, local and relative-escaping references, the per-body count, byte, pixel, frame, deadline and redirect bounds, a mislabeled or non-PNG/JPEG/GIF payload, SVG and WebP each end in the defined state with the placeholder, and no request is made for a blocked reference; core and host tests show each with a negative control. | Pass. Evidence: core_markdown_image (schemes, data:, local and escaping relative references, per-body count, byte/pixel/frame bounds, redirects, each with a negative control), qt_markdown_image_decoder (mislabeled, SVG, WebP, BMP unsupported), native_https_image_fixture (deadline, size, fourth hop), fuzz_markdown images selector 2,034,317 runs clean. |
| **AC04:** Loopback contract tests show: resolution uses only the profile's API authority; GitHub.com image fetches carry no credential; a GHES credential reaches only its credential origins; a redirect to another host drops it for good; a disallowed hop fails closed; and no URL, signed address or credential reaches Qt or the diagnostic log. | Pass. Evidence: native_https_image_fixture (11 loopback scenarios: GitHub.com never sends a credential, GHES only to its host and media host, foreign redirect drops it for good, disallowed hop fails closed; negative control), native_https_image_contract, github_markdown_image_resolution (profile API authority only); projections carry only the source as written. |
| **AC05:** Alt text is the accessible text, tooltip and fallback; Tab reaches images that open something; Enter and click open them through link admission; every failure keeps the placeholder with its reason; copy and source view preserve the original body. | Pass. Evidence: qt_markdown_images_* (alt as accessible text, tooltip and fallback; Tab reaches linked images and sets the accessible description; Enter and click go through link dispatch; reasons on placeholders; copy and source keep the original). |
| **AC06:** The memory and disk caches respect their bounds and isolation; a restarted application shows a cached image without a request; profile purge removes the disk cache. | Pass. Evidence: sqlite_markdown_image_disk_cache (128 MiB LRU, host/account isolation, 7-day expiry, restart, owner-private, no key text; three negative controls), application_markdown_image_host restart, application_markdown_images_public_path composed restart (only the never-cached SVG fetched; fails with the disk cache unplugged), sqlite_release_maintenance_files purge. |
| **AC07:** A live course on `MikeMarcin/BPR-Test` renders a PR whose description holds a `user-attachments` image and an `<img width>` tag, within the observed rate-limit margin; the owner accepts in an attended session that "images in descriptions and comments show the way GitHub shows them". | Pass. Live course pass (BPR-1550 AC01); the owner accepted in session 8a5e3c244f674294366fd8796da2897e (cursor 17). |
| **AC08:** Architecture, Markdown, threat-model and fixture documentation describe this policy; format, tidy, architecture-boundary, documentation and resealed composition-contract gates pass. | Pass. Evidence: native-markdown.md, system-architecture.md, security-threat-model.md, storage-and-diagnostics.md, fixture-catalog.md; clang-format 22 and clang-tidy clean on touched files; architecture_boundaries and test_network_isolation_manifest pass; application_composition_contract resealed and passing; verify_documentation DOCUMENTATION_OK. |
| **AM1-01..03** (owner amendment 1) | Pass. [[BPR-1554]] (SVG), [[BPR-1555]] (WebP), [[BPR-1556]] (preference). |

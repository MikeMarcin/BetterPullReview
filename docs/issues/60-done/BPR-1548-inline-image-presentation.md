---
schema: "implement-rfc.card.v1"
id: "BPR-1548"
title: "Show images inline in the Markdown reader"
status: "Done"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1547"]
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"inline-image-presentation"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"0ddac86fc6bfcc614b79071076d3fb26302ccbf2","evidence":{"kind":"command","detail":"ctest qt_markdown_images_1_0/1_25/2_0 (reserved box, single reflow with scroll compensation, width/height/percentage sizing within the text and table-cell width, crisp at device pixel ratio, GIF frames advance only while visible, linked image anchors, Tab sets the accessible description, copy writes Markdown back, every failure keeps the placeholder with its reason, stale projection ignored) and application_markdown_images_public_path (composed fixture markdown-images-v1: nine description images through the shipped composition, attachments/camo/linked/animated/<picture> load, SVG unsupported with placeholder, <img> in code stays text, comments load, a gone image fails to its placeholder, reopening answers from cache). Live: tests/scripts/markdown_image_live_course.py on MikeMarcin/BPR-Test#73 loaded 4 user-attachments and 4 camo images, SVG badge unsupported, missing image failed (rate remaining 4993)."}},{"criterion":"AC02","outcome":"pass","revision":"0ddac86fc6bfcc614b79071076d3fb26302ccbf2","evidence":{"kind":"command","detail":"packaging/package_runtime.py deploys imageformats qgif and qjpeg on macOS, Windows and Linux; tools/release.sh verify_plugin_policy and scripts/designated_live_macos_capsule.py admit exactly those two added plugins; package_runtime_contract, release_package_audit_contract, designated_live_macos_capsule_contract and native_release_contract pass. A signed macOS bundle was not staged (release staging belongs to publish-release)."}},{"criterion":"AC03","outcome":"pass","revision":"0ddac86fc6bfcc614b79071076d3fb26302ccbf2","evidence":{"kind":"command","detail":"docs/architecture/native-markdown.md (Images), system-architecture.md (Inline Markdown image allocation), security-threat-model.md (body-image exception), docs/verification/fixture-catalog.md (markdown-images-v1), src/adapters/qt/AGENTS.md and src/adapters/github/AGENTS.md; application_composition_contract resealed and passing; verify_documentation passes."}}]
---

# BPR-1548: Show images inline in the Markdown reader

The shipped reader shows images inline with loading, reflow, sizing, HiDPI,
animation, links, accessibility and fallback, composed through the real
application with packaged decoders
([RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md)).

## Acceptance

- **AC01:** Qt tests and a composed fixture course show RFC AC01, AC02 and
  AC05 at 1.0x, 1.25x and 2.0x.
- **AC02:** The package ships `qgif` and `qjpeg`; the release package
  verifier agrees.
- **AC03:** Markdown, architecture, threat-model and fixture documentation
  describe the policy; composition contracts are resealed.

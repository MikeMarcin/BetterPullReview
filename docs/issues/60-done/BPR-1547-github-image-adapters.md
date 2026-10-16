---
schema: "implement-rfc.card.v1"
id: "BPR-1547"
title: "GitHub image resolution and fetch adapters and the image decoder"
status: "Done"
rfcs: ["RFC-BPR-0084"]
depends_on: ["BPR-1546"]
metadata: {"mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"},"milestone":"github-image-adapters"}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"f657a502d8d18e1c3cff37bf121de1d120a7d5c7","evidence":{"kind":"command","detail":"ctest native_https_image_fixture (tests/scripts/verify_native_https_images.py: loopback CONNECT proxy, disposable CA, 11 scenarios): GitHub.com camo and user-attachments->S3 signed redirect fetched with no Authorization on any hop; foreign redirect blocked with no CONNECT; over-size too_large; 404 failed; fourth redirect failed; stall hits the 1 s deadline; GHES attachment hop and media hop carry exactly Bearer <random token>, storage hop and the later return to the GHES host carry none; media binding on GHES sends none; no cookie/referer, identity encoding, token never in child output. Negative control: sending the credential on every hop fails ghes-credential:requests. native_https_image_contract: media binding refuses GitHub reads, forged first hop/plain HTTP/unearned credential mark/over-bound bytes refused before the wire, GitHub.com account binding runs no image fetch, stopped fetch sends nothing. github_markdown_image_resolution: profile's API authority only (GraphQL /graphql, /api/graphql on GHES), repository and pull request identities enforced."}},{"criterion":"AC02","outcome":"pass","revision":"f657a502d8d18e1c3cff37bf121de1d120a7d5c7","evidence":{"kind":"command","detail":"ctest qt_markdown_image_decoder: PNG/JPEG decode to premultiplied packed ARGB; hand-assembled 2-frame GIF keeps both frames with 20 ms and 100 ms delays; 20000 px side too_large before decode; JPEG bytes behind a PNG signature fail in the PNG reader only; SVG, WebP, BMP, empty unsupported; truncated PNG failed. core_markdown_image covers the 40 MP/16384 px/400 frame/256 MiB bounds."}},{"criterion":"AC03","outcome":"pass","revision":"f657a502d8d18e1c3cff37bf121de1d120a7d5c7","evidence":{"kind":"command","detail":"gh_cli_transport compiles resolve_body_images through the closed gh api graphql shape for github.com and GHES with the canonical body on stdin; github_read_contract, github_anonymous_read_contract (anonymous refusal), native_https_anonymous_read_contract, github_paged_read, github_provider and test_network_isolation_manifest pass (28/28). Fixture replay of the resolution read is exercised by the composed course in BPR-1548. The signed served addresses never reach the diagnostic log (no log call on the image path)."}}]
---

# BPR-1547: GitHub image resolution and fetch adapters and the image decoder

GitHub `bodyHTML` resolution reads, native HTTPS image fetches under the
credential and redirect policy, and the restricted Qt Gui decoder implement
the host ports ([RFC-BPR-0084](../../rfcs/RFC-BPR-0084-inline-markdown-images.md)).

## Acceptance

- **AC01:** Loopback contract tests: resolution uses only the profile's API
  authority; GitHub.com fetches carry no credential; a GHES credential reaches
  only its credential origins; a foreign redirect drops it for good; a
  disallowed hop, a fourth hop, a non-200 status, a non-image type, an
  over-size body and the deadline fail closed.
- **AC02:** Decoder tests: only PNG, JPEG and GIF by signature; SVG, WebP and
  mislabeled payloads are unsupported; pixel, side, frame and decoded-byte
  bounds hold.
- **AC03:** The configured-CLI resolution shape and fixture replay are
  covered; nothing secret is logged.

---
id: "RFC-BPR-0084"
title: "Show images in descriptions and comments the way GitHub does"
status: "In Progress"
area: "BPR"
depends_on: ["RFC-BPR-0036"]
metadata: {"rfc_base_sha":"94699fd51c2644c3ebb38743f623927d179859a1","mandate_revision":"inline-markdown-images-r1","milestone_plan":{"id":"inline-markdown-images-v1","path":"docs/rfcs/plans/inline-markdown-images-v1/milestone-plan.json"}}
---

# Show images in descriptions and comments the way GitHub does

## Execution status

Mandate `inline-markdown-images-r1` (owner, 2026-10-16: "we should show images
the same way GitHub does") adopts RFC-BPR-0084 unchanged apart from this
section and its links. Immutable RFC_BASE_SHA: `94699fd51c2644c3ebb38743f623927d179859a1`. The
[live import](assets/inline-markdown-images-v1.LIVE_IMPORT.md) retains the
draft's bytes and the
[milestone plan](plans/inline-markdown-images-v1/milestone-plan.json) groups the
cards:

| Card | Scope |
|---|---|
| [[BPR-1545]] | Core image policy (AC03, AC04 policy side) |
| [[BPR-1546]] | Host lane, ports, memory cache, projection (AC02, AC03) |
| [[BPR-1547]] | GitHub resolution and fetch adapters, decoder (AC03, AC04) |
| [[BPR-1548]] | Qt presentation, composition, packaging (AC01, AC02, AC05) |
| [[BPR-1549]] | On-disk cache in the profile (AC06) |
| [[BPR-1550]] | Live course, attended owner review and closeout (AC07, AC08) |
| [[BPR-1554]] | SVG through QtSvg ([owner amendment 1](#owner-amendment-1-2026-10-16-answers-to-q1-q3), AM1-01) |
| [[BPR-1555]] | WebP through Qt's WebP plugin (AM1-02) |
| [[BPR-1556]] | "Images in descriptions and comments" preference (AM1-03) |

The image parts of RFC-BPR-0036 ("Links, images and remote content" and the
image clause of its AC-03) are superseded by this RFC for image references
only. A concurrent RFC-BPR-0036 amendment ([[BPR-1543]], [[BPR-1544]]) owns the
broader HTML subset and leaves `<img>` and `<picture>` here.

## Problem and grounded current behavior

On 2026-10-16 the owner decided that "we should show images the same way
GitHub does" in Markdown bodies: the PR description, review summaries and
every comment `markdown_body_view` renders.

Today `replace_images` in `src/adapters/qt/src/markdown_body_view.cpp` turns
every Markdown image into the text `[Image: alt — open image link]`, and
`markdown_document::loadResource` answers every resource request with an
empty byte array. Raw HTML is imported with `MarkdownNoHTML`, so an HTML
`<img>` tag shows as literal text. That follows
[RFC-BPR-0036](RFC-BPR-0036-native-markdown-rendering.md) ("Links, images and
remote content" and its AC-03): no automatic remote image fetching, and inline
retrieval, private attachment authentication and caching "require a later
scoped extension with host-owned effects and size limits". This RFC is that
extension. It records the owner's override of "no automatic fetch" for image
references only; every other RFC-BPR-0036 rule (no scripts, no embedded
browser, no local-file reads, links only by explicit gesture, source and copy
fidelity, bounds) stays.

GitHub's own rendering was observed on 2026-10-16 through `POST /markdown` and
GraphQL `bodyHTML`:

- An external image becomes a `camo.githubusercontent.com` proxy address with
  the original in `data-canonical-src`, including `http:` sources. GitHub's
  camo proxy fetches it, so the image host never sees the reader's address.
- A `github.com/user-attachments/assets/<uuid>` attachment becomes a
  `private-user-images.githubusercontent.com/.../<id>-<uuid>.<ext>?jwt=...`
  address signed for the authenticated reader, valid for five minutes. This
  is how a private repository's attachment is authorized; the API token is not
  sent to the image host. The bare `user-attachments` address of a public
  repository answers `302` to a signed S3 address without authentication.
- GitHub-hosted origins (`raw.githubusercontent.com`,
  `user-images.githubusercontent.com`, `avatars.githubusercontent.com`,
  `github.com/.../raw/...`) are served as written.
- A `data:` source loses its `src`; `width`, `height` (including `50%`) and
  `alt` survive; `style`, event handlers and other attributes are dropped.
  `<picture>` with `prefers-color-scheme` sources survives. An animated GIF is
  marked `data-animated-image`. SVG is served by camo as `image/svg+xml`.

## Proposed behavior and boundaries

### What renders

The reader shows each image of a rendered body inline where GitHub would:
Markdown `![alt](src "title")`, reference-style images, and HTML
`<img src alt width height>` in prose, lists, quotes and table cells. HTML
`<a href>` wrapping exactly one `<img>` is a linked image. `<picture>` whose
`<source>` elements select only by `prefers-color-scheme` shows the source
matching the reader's light or dark palette, else its `<img>`. Only `src`,
`alt`, `width`, `height` and, for `<source>`, `media` and a single-candidate
`srcset` are read; every other attribute is ignored. An `<img>` inside code
remains literal code. Other raw HTML keeps whatever presentation RFC-BPR-0036
and its amendments define; this RFC adds no general HTML importer and needs
none.

Sizing follows GitHub: an image's natural size is its pixel size in logical
pixels; `width` and `height` (pixels or a percentage of the reader's width)
override it, the missing dimension following the aspect ratio; no image is
wider than the reader's text width, and narrowing scales it down with its
aspect ratio. Images draw at the screen's device pixel ratio without
blurring at 1.0x, 1.25x and 2.0x. An animated GIF animates with its frame
delays, as GitHub does; one beyond the decode bounds below shows its first
frame.

Clicking an image, or pressing Enter on it after Tab reaches it, goes through
the existing link admission: a linked image opens its link, an unlinked one
opens its own original source address, as GitHub wraps every image in a link
to itself. Alt text is the image's accessible text, its tooltip with the
destination, and the text of its fallback.

Each image has one state. Before its pixels arrive it is **loading**: a quiet
box in the palette's muted background, sized from `width` and `height` when
both are known, otherwise one text line tall showing its alt text. When the
pixels arrive the body reflows once, keeping the reader's scroll position and
selection. Every other state (failed, too large, blocked, unsupported format,
over the per-body count) falls back to the existing accessible placeholder
`[Image: alt — open image link]`, whose tooltip and accessible description say
why it is not shown. Nothing in a body is blanked or truncated by an image
failure.

### Who decides, performs and presents

The core decides. It admits each untrusted source string from the reader for
the current accepted body context (the same window, session, identity,
snapshot and publication revision that links use), classifies it, bounds the
count, matches it to GitHub's served address, admits each fetch and redirect
hop, decides whether a credential may ride on it, computes the displayed size
from the decoded size, hints and available width, and owns the state
transitions and stale-completion rejection by typed identity.

The application host performs. It asks GitHub, through the profile's own
account transport, for the rendered HTML of the bodies that hold admitted
images (one bounded GraphQL `nodes(ids:)` query selecting `bodyHTML`, at most
25 nodes per query), runs image fetches on a bounded lane with cancellation,
runs decoding off the decision and GUI lanes, owns the caches, and publishes a
provider-free projection: body identity, reference ordinal, state, displayed
pixel frames and frame delays. No URL, signed address, header or credential
crosses into Qt.

Qt presents. The reader extracts image references and size hints from its own
imported document, sends their source text through a dispatch like link
dispatch, draws the projection through a custom text object (no `QUrl`, no
document resource loading, `loadResource` unchanged), handles reflow, HiDPI,
animation, focus and accessibility. It performs no fetch, decode or cache
effect.

The GitHub adapter performs the resolution read and the image fetches through
native HTTPS (`native_https_transport.cpp` stays the only GitHub source with
`QNetworkAccessManager`). A decode adapter implements the host's decode port
with Qt Gui's image readers. No new library is linked into the core;
`QImageReader` and Qt's own `qgif` and `qjpeg` plugins from the pinned Qt
provider do the decoding.

### Fetch policy

Every fetch uses the served address GitHub's rendering gives for that
reference, never a guess:

- A reference GitHub does not render as an image (stripped `src`, code, a
  scheme GitHub drops) is **blocked**. Source schemes other than `http` and
  `https`, `data:`, `file:`, network-path and parent-escaping references are
  blocked before any request. A repository-relative reference is resolved
  only through GitHub's rendering.
- On GitHub.com a served address is admitted only over HTTPS to
  `camo.githubusercontent.com`, `private-user-images.githubusercontent.com`,
  `user-images.githubusercontent.com`, `raw.githubusercontent.com`,
  `avatars.githubusercontent.com`, `objects.githubusercontent.com` or
  `github.com`; a redirect may additionally reach
  `github-production-user-asset-*.s3.amazonaws.com` and
  `github-production-repository-*.s3.amazonaws.com`. External hosts are
  reached only through camo. No GitHub.com image fetch carries a credential:
  GitHub's signed addresses carry the reader's authorization.
- On GHES a served address must be HTTPS. Only the profile's exact hostname
  and its `media.` subdomain-isolation host are credential origins: a fetch to
  one of them carries the profile's credential exactly as an API read would.
  Any other HTTPS host (GHES without an image proxy serves external images as
  written) is fetched without a credential.
- Every redirect hop is re-admitted by the same policy. A credential rides a
  hop only while every hop so far stayed on a credential origin; once dropped
  it is never reattached. Query-bearing signed hops are allowed for image
  fetches only, never for API reads.
- Requests send no cookies, no `Referer`, a fixed `User-Agent` and
  `Accept: image/png,image/jpeg,image/gif`, ask for identity encoding, and
  admit only status 200 with an `image/*` content type. The decoder trusts
  the content's signature, not its label.

The configured-CLI profile performs the resolution read through its closed
`gh api graphql` shape and the GitHub.com image fetches through credential-free
native HTTPS. A GHES credential origin in a CLI profile is fetched without a
credential and falls back on refusal. Public preview has no GraphQL, so its
images keep the placeholder until a later extension.

### Bounds

| Bound | Default |
|---|---|
| Image references considered per body | 32; later ones keep the placeholder |
| Bodies per resolution query / `bodyHTML` bytes per body | 25 / 1 MiB |
| Concurrent image fetches per profile | 4 |
| Queued image requests per window | 128 |
| Encoded bytes per image | 10 MiB |
| Redirect hops / total deadline per fetch | 3 / 20 s |
| Pixels per frame / largest side | 40,000,000 / 16,384 |
| Animation frames / decoded bytes per image | 400 / 256 MiB, else first frame |
| Decoded in-memory cache per profile | 256 MiB, least recently used |
| Encoded on-disk cache per profile | 128 MiB, least recently used |

Formats are PNG, JPEG and GIF, recognized by signature, decoded with format
auto-detection off. SVG, WebP and anything else are **unsupported** and keep
the placeholder (see open questions). Owner amendment 1 adds SVG and WebP.

### Caching and requests

The cache key is the profile's host and account (or none for public content)
plus the canonical source reference (the attachment UUID for
`user-attachments`, otherwise the source address), never a signed served
address. Once an image is cached, re-rendering, resizing, switching source
view, scrolling, theme changes and an unchanged refresh make no request.
The on-disk cache lives inside the profile as owner-private files, holds only
encoded bytes that decoded successfully, is isolated per host and account,
expires an entry after seven days without use, and is removed by profile
purge. A signed served address is never persisted or logged.

A changed or replaced body, a closed window, sign-out or an account change
cancels that context's queued work; a late completion for another body,
revision or window is rejected and never drawn.

### Privacy

Accepted by the owner's parity decision: on GitHub.com the fetches go to
GitHub's own image hosts, exactly as a browser on github.com would, and camo
keeps external image hosts from seeing the reader's network address. On GHES
without an image proxy, external image hosts see the reader's address and the
time the body was read, as they would in a browser. Unlike a browser, no
cookie or referrer is sent.

### Open owner questions (not blocking)

- **Q1 SVG.** Qt's pinned provider includes QtSvg, which renders SVG Tiny 1.2
  without scripts. Shipping it adds the QtSvg library and its `qsvg` plugin
  to the package. Accept to rasterize SVG with QtSvg (external references
  refused), or Reject to keep SVG as a placeholder.
- **Q2 WebP.** WebP needs Qt's `qtimageformats` module (with libwebp, BSD) in
  the pinned provider and package. Accept to add it, or Reject.
- **Q3 Setting.** Accept to add a "Load images in descriptions and comments"
  preference, on by default; Reject to load always.

Until answered, the defaults above apply. The owner accepted all three on
2026-10-16; see owner amendment 1.

### Owner amendment 1 (2026-10-16): answers to Q1-Q3

Recorded from attended session `8a5e3c244f674294366fd8796da2897e` (cursor 17):
the owner accepted "Images in descriptions and comments show the way GitHub
shows them" and each open question, with no notes. Accepting Q2 is the owner's
approval of libwebp (BSD-3-Clause) inside Qt's `qtimageformats`.

- **SVG (Q1).** An SVG payload, recognized by its root element, is rasterized
  once, statically, with QtSvg's renderer (untrusted-source mode, animations
  off), never through a Qt image plugin. The core admits it first: at most
  1 MiB, 20,000 elements and nesting depth 64; every `href` names a fragment of
  the document or an embedded `data:image/png|jpeg|gif|webp|svg+xml` payload;
  every CSS `url()` names a fragment; no entity declaration, internal doctype
  subset, script, `foreignObject`, `@import`, `@font-face` or SVG font. So
  nothing outside the document is ever loaded: no file, URL or external font
  or image. A refused SVG is `blocked`, an oversized one `too_large`. The
  rasterized size is the SVG's own (`width`/`height`, else its `viewBox`,
  else 300 x 150 as in a browser) within the pixel and side bounds, drawn at
  twice that size so it stays sharp at high density. The package adds the
  QtSvg library and no SVG plugin.
- **WebP (Q2).** WebP, recognized by its `RIFF....WEBP` signature, decodes
  through Qt's `qwebp` plugin from `qtimageformats` with the same byte, pixel,
  side, frame and decoded-byte bounds as GIF; an animated WebP animates. The
  pinned Qt provider adds the `qtimageformats` module; the package ships only
  `qwebp` from it.
- **Preference (Q3).** Settings > Review behaviour has "Descriptions and
  comments: Images — Load / Don't load", Load by default, stored in the
  profile's workbench preferences and applied live in every window. Don't
  load shows every image as its placeholder and sends no resolution or fetch;
  turning it back on shows the images again.

Packaging, release and capsule plugin policies, third-party notices and the
SBOM name QtSvg, `qtimageformats` and its bundled libwebp.

- **AM1-01:** Core and decoder tests show an admitted SVG rasterized at its
  size and twice its density, and each refused construct (`http`/`file`
  `href`, external `url()`, entity, script, `foreignObject`, `@import`,
  `@font-face`, over-bound size, elements or depth) ending in its state with a
  negative control; a composed fixture shows an SVG badge inline.
- **AM1-02:** Decoder tests show still and animated WebP within the bounds and
  an over-bound one refused; the composed fixture shows a WebP image; package,
  release and capsule policies and notices name the added files.
- **AM1-03:** Storage, Qt and composed tests show the preference persisted
  (schema migration keeps Load), applied live, and off making no request.

## Acceptance criteria

- **AC01:** Through the shipped composition, the PR description and
  conversation posts of a fixture show Markdown, HTML, linked, list, table,
  quote, `width`/`height`/percentage and `<picture>` images inline as defined
  in "What renders"; comparison renders against GitHub's page for the same
  body show matching placement and size.
- **AC02:** A loading image shows its reserved box, then the body reflows once
  keeping scroll position and selection; an animated GIF animates; images stay
  within the reader's width and crisp at 1.0x, 1.25x and 2.0x; resizing,
  re-rendering and an unchanged refresh make no request once cached; a late
  result for another body, revision or window is never drawn.
- **AC03:** Blocked schemes, `data:`, local and relative-escaping references,
  the per-body count, byte, pixel, frame, deadline and redirect bounds, a
  mislabeled or non-PNG/JPEG/GIF payload, SVG and WebP each end in the
  defined state with the placeholder, and no request is made for a blocked
  reference; core and host tests show each with a negative control.
- **AC04:** Loopback contract tests show: resolution uses only the profile's
  API authority; GitHub.com image fetches carry no credential; a GHES
  credential reaches only its credential origins; a redirect to another host
  drops it for good; a disallowed hop fails closed; and no URL, signed
  address or credential reaches Qt or the diagnostic log.
- **AC05:** Alt text is the accessible text, tooltip and fallback; Tab reaches
  images that open something; Enter and click open them through link
  admission; every failure keeps the placeholder with its reason; copy and
  source view preserve the original body.
- **AC06:** The memory and disk caches respect their bounds and isolation; a
  restarted application shows a cached image without a request; profile
  purge removes the disk cache.
- **AC07:** A live course on `MikeMarcin/BPR-Test` renders a PR whose
  description holds a `user-attachments` image and an `<img width>` tag,
  within the observed rate-limit margin; the owner accepts in an attended
  session that "images in descriptions and comments show the way GitHub shows
  them".
- **AC08:** Architecture, Markdown, threat-model and fixture documentation
  describe this policy; format, tidy, architecture-boundary, documentation and
  resealed composition-contract gates pass.

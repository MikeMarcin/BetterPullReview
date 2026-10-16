# Inline Markdown images live import

Baseline RFC_BASE_SHA: `94699fd51c2644c3ebb38743f623927d179859a1`. Mandate: `inline-markdown-images-r1`
(owner, 2026-10-16: "we should show images the same way GitHub does").

| Source | Live destination |
|---|---|
| RFC-DRAFT-0022-inline-markdown-images.md | [RFC-BPR-0084](../RFC-BPR-0084-inline-markdown-images.md) |
| DRAFT-0001 to DRAFT-0006 | [[BPR-1545]] to [[BPR-1550]] |
| Authored six-milestone plan | [Adopted plan](../plans/inline-markdown-images-v1/milestone-plan.json) |

The [original draft](inline-markdown-images-v1/RFC-DRAFT-0022-inline-markdown-images.source.txt),
[authored plan](inline-markdown-images-v1/milestone-plan.source.json) and
[ID map](inline-markdown-images-v1/id-map.source.json) retain exact bytes.
Q1 (SVG through QtSvg), Q2 (WebP through qtimageformats) and Q3 (a load-images
preference) are open owner questions with recorded defaults; none blocks
execution. A concurrent RFC-BPR-0036 amendment ([[BPR-1543]], [[BPR-1544]])
owns the broader HTML subset and leaves `<img>` and `<picture>` here.

---
schema: "implement-rfc.card.v1"
id: "BPR-0916"
title: "Implement native Markdown rendering for review content"
status: "Backlog"
rfcs: ["RFC-BPR-0036"]
depends_on: []
milestone: "Post-Pack v2"
---

# Implement native Markdown rendering for review content

Owner feedback after the first loaded PR: “We need an RFC to support markdown
rendering.” This card registers future implementation; no runtime change is
requested by this documentation slice.

Follow [RFC-BPR-0036](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md).
Before source work, inventory overview and public-conversation body paths,
select the supported dialect and native renderer, define measured processing
bounds, and split bounded implementation slices as needed. Preserve original
Markdown for source/copy/publication and keep all resource/navigation effects
out of widgets. No embedded browser or automatic remote image loading.

## Acceptance

Implement AC-01 through AC-06 and record direct composed evidence on [[BPR-0917]].
Coordinate long-description geometry with [[BPR-0918]] without blocking that
investigation on renderer implementation. Use synthetic bodies covering long
text, nested structures, wide tables/code and hostile resource references.

## Status

Not started; proposal only. Await a later implementation request.

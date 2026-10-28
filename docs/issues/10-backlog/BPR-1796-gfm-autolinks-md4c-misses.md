---
schema: "implement-rfc.card.v1"
id: "BPR-1796"
title: "Autolink what GitHub autolinks that md4c does not"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1796--BPR-1796--687f8fcd2519.json","kind":"bug","found_by":"BPR-1793 cmark-gfm comparison, 2026-10-27","related_rfcs":["RFC-BPR-0036"]}
acceptance: []
---

# BPR-1796: Autolink what GitHub autolinks that md4c does not

Found by [BPR-1793](../60-done/BPR-1793-evaluate-cmark-gfm-for-the-markdown-reader.md).
Qt's md4c autolinks fewer forms than GFM's autolink extension: a URL with a
non-ASCII path (`http://google.com/å`), `mailto:` and `xmpp:` links, an
address right after a colon or slash, a URL inside quotes, a host with an
underscore, and it ends a URL differently at trailing parentheses, an
entity-like ending or a `<`. `qt_markdown_cmark_spec` lists them under
`md4c-autolinks` (extensions example 19, spec 625 to 627, extensions 20).

## Acceptance

- AC1. The reader autolinks as GFM's extension does: a preparation pass
  makes the links (md4c's permissive autolinks off), outside code and
  existing links, with GFM's trailing-punctuation and parenthesis rules;
  each address is still admitted by core before it opens.
- AC2. The `md4c-autolinks` entries leave `known-differences.json` because
  they match.

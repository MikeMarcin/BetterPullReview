---
schema: "implement-rfc.card.v1"
id: "BPR-1796"
title: "Autolink what GitHub autolinks that md4c does not"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1796--BPR-1796--687f8fcd2519.json","kind":"bug","found_by":"BPR-1793 cmark-gfm comparison, 2026-10-27","related_rfcs":["RFC-BPR-0036"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "d7bb8e88ea87343a16c9549d88ed7e966fce98f2", "evidence": {"kind": "command", "detail": "qt_markdown_github_1_0, _1_25, _2_0 check_autolinks: a non-ASCII path, balanced parentheses, mailto: and xmpp: (with resource), a bare email, a quoted URL and an entity-like end link as GFM does, trailing punctuation left out; javascript:, code, an existing link's text and an underscored host stay unlinked. Activation still goes through the existing admission. All 25 Markdown-rendering courses pass."}}, {"criterion": "AC2", "outcome": "pass", "revision": "d7bb8e88ea87343a16c9549d88ed7e966fce98f2", "evidence": {"kind": "command", "detail": "qt_markdown_cmark_spec: the md4c-autolinks entries (spec 625 to 627, extensions 19 and 20) left known-differences.json; extensions 19 now differs only by percent-encoding, extensions 20 is a does-not-crash example; 607 of 726 match, 119 listed."}}]
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

## Built

`mark_autolinks` in `markdown_github.cpp` follows cmark-gfm's `autolink.c`:
`www.` after a line start, a space or one of `*_~(`; `http://`, `https://`
and `ftp://` after any non-letter; email addresses, with `mailto:` or
`xmpp:` (and an xmpp resource) when the scheme stands after a
non-alphanumeric character. Domains take ASCII letters, digits, `-`, `_` and
non-ASCII characters that are not punctuation, with no `_` in the last two
segments. Links end before `<` and give up trailing `?!.,:*_~'"`, an
entity-like `&name;` and an unmatched `)`. Each match becomes an explicit
`[text](destination)` with its characters escaped; code spans, existing
links (inline, reference, and a `[label]` naming a definition), footnote
references, HTML tags, `<…>` autolinks, inline math and definition lines
are skipped. md4c's permissive autolinks are off.

## Not done

- A destination keeps non-ASCII and reserved characters as written rather
  than percent-encoding them (`link-percent-encoding` in the list).

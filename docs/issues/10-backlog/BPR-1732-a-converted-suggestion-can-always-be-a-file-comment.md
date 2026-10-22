---
schema: "implement-rfc.card.v1"
id: "BPR-1732"
title: "A converted suggestion can always become a file comment"
status: "Backlog"
rfcs: []
depends_on: ["BPR-1716"]
priority: "P3"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1726--BPR-1733--4ee9bc97752e.json","origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2"}
---

# BPR-1732: A converted suggestion can always become a file comment

## Origin

RFC-BPR-0090's Q3 (owner, 2026-10-21): "Mark it and give the option to convert
it to a file comment say on the finalize review page or on the comment
itself?" Convert rewrites a suggestion GitHub cannot apply as text at the same
lines. Where the line is one GitHub resolves on no commit, that text is
BPR-1706's file comment. Where GitHub resolves it on a filtered diff's earlier
commit, the text is published there and GitHub shows it outdated, because a
draft carries no durable "file comment" subject that would make it a file
comment instead.

## Proposal

Give a draft a durable file-comment subject, set only by the reviewer's
Convert, so the host publishes it as RFC-BPR-0022's file comment with a
permalink to its lines wherever it is. This is BPR-1716's AC2 generalised
to ranges and earlier commits; it touches the draft record, the SQLite codec
and the GitHub file-comment path, so it is its own card.

## Acceptance criteria

- AC1: After Convert, the comment is published as a file comment with a
  permalink to its lines on every placement, and survives a restart.

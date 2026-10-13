---
schema: "implement-rfc.card.v1"
id: "BPR-1439"
title: "Share GitHub HTTP helpers and format with Qt and fmt"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
metadata: {"reservation":"docs/issues/_reservations/BPR-1439--BPR-1439--31a62109ca4f.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit (item 10: replace with Qt, use fmt not std::format)"}
---

# Share GitHub HTTP helpers and format with Qt and fmt

The 2026-10-13 audit found small HTTP and formatting helpers copied across the
GitHub adapter and the rest of the tree:
- five `Link` header parsers, with three levels of strictness;
- five percent-encoders, three of them using locale-dependent `std::isalnum`;
- five JSON `Content-Type` checks, the loosest of which accepts
  `application/jsonx`;
- a hand-written base64 encoder and RFC 3339 formatter;
- about 30 hex tables.

Two latent faults turned up as well. A header check's `"\r\n\0"` set stops at
the NUL, so an embedded NUL passes. The device-login verification URL check
accepts userinfo and any port. The owner chose Qt for these helpers, and fmt
rather than `std::format` for formatting. fmt is an owner-approved core
library.

## Acceptance

- AC1: One `Link` header parser in the GitHub adapter serves the gh response,
  native transport, paged read, mutation and publication readers. Each caller
  keeps its own URL policy. The publication reader no longer misses an
  unquoted `rel=next` or a multi-relation value.
- AC2: Percent-encoding uses `QByteArray::toPercentEncoding`, keeping `/`
  where a branch or path keeps it today. Existing outputs are unchanged, and
  no encoder depends on the locale.
- AC3: One JSON media-type predicate serves the five checks: `application/json`
  with at most a `charset=utf-8` parameter, case-insensitive and bounded. The
  release-update download keeps its own media-type set.
- AC4: The publication blob uses `QByteArray::toBase64`, and the RFC 3339
  publication date is formatted with pinned fmt, which is acquired like the
  other verified archives and listed in the notices. Outputs are unchanged.
- AC5: One core hex helper replaces the plain lowercase hex copies in core and
  in Qt-free targets. Qt targets use `QByteArray::toHex`. Integer, pkt-line
  length, uppercase-escape and JSON-escape renderings stay as they are.
- AC6: A header name or value with an embedded NUL is refused. The device-login
  verification URL is refused when it carries userinfo, an explicit port or a
  fragment. Each fault has a test.

## Evidence

Pending.

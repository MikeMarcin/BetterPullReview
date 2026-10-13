---
schema: "implement-rfc.card.v1"
id: "BPR-1433"
title: "Decode JSON with glaze behind one shared structural check"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1434"]
priority: "P1"
metadata: {"reservation":"docs/issues/_reservations/BPR-1432--BPR-1438--a3c0ec887bde.json","origin":"owner request 2026-10-13 after the hand-rolled infrastructure audit"}
---

# Decode JSON with glaze behind one shared structural check

The 2026-10-13 audit counted eight hand-written JSON implementations (about
2,900 lines). Four GitHub structural pre-checks had drifted apart (some
refused `1e999`, one had no string limit, one reported an over-long string as
malformed), and the MCP helper's reader accepted `.5`, `01` and raw control
characters. The owner chose glaze, now an approved core library. glaze decodes
but cannot refuse duplicate keys, so one first-party structural check remains.

## Acceptance

- AC1: `core::json_preflight` is the one structural check (one value,
  well-formed strings and numbers below 10^308, no duplicate or escaped key,
  per-caller ceilings with string and other ceilings as structural limits); the
  three GitHub copies are gone and the GitHub shared header wraps it.
- AC2: The MCP context helper decodes requests and writes strings with glaze
  after the shared check, so non-RFC 8259 input is refused; the designated live
  observer reads its exact fields, including 64-bit identifiers, with glaze.
- AC3: glaze is part of the Qt-free core graph, and the two deliberate
  first-party parsers (the canonical review-cycle JSON and the byte-range agent
  stream parser) are documented with their reasons.

## Evidence

Pending.

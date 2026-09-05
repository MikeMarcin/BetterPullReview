---
provisional_id: PACK-BPR-0022-04
live_id: allocate-on-import
title: Implement the canonical immutable code-permalink representation
status: Backlog
rfc: RFC-BPR-0022
depends_on: [PACK-BPR-0022-02, PACK-BPR-0022-03]
---

# Implement the canonical immutable code-permalink representation

Implement bounded compile and parse contracts for the same-host, same-repository, full-commit line permalink and the visible fallback body grammar.

## Acceptance

- Compiler emits canonical web-origin URLs, one-based line fragments, and `?plain=1` only for Markdown source.
- Path segments are encoded without allowing authority changes, userinfo, alternate ports, encoded separators, dot segments, arbitrary query, or fragment injection.
- Parser accepts exactly one leading canonical link, one blank line, and non-empty byte-preserved prose.
- Authority, commit, path, blob, and line are validated before an anchor is produced.
- Invalid, foreign, ambiguous, oversized, or malformed input returns no anchor and performs no mutation.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.

---
id: BPR-0073
title: Implement deterministic diff pairing and intraline ranges
status: Backlog
rfc: RFC-BPR-0004
depends_on: [BPR-0072]
---

# Implement deterministic diff pairing and intraline ranges

Implement deterministic text decoding/line splitting, primary diff
canonicalization, monotonic secondary line pairing, internal replacement
anchors, and valid original-text intraline ranges. Preserve final-newline and
line-ending facts and make whitespace policy part of the exact option digest.

Weak pairings remain explicit and moved blocks remain deletion plus insertion
in the primary path with separate relation metadata.

## Required direct evidence

Cover identical, insert/delete, 4:7 and 7:4 replacement, internal anchors,
repeated similar lines, tabs/whitespace modes, LF/CRLF/final newline, Unicode,
empty files, rename identity, binary/unavailable, and moved-block fixtures.
Tie breaking and stable IDs must repeat under replay and generated cases.

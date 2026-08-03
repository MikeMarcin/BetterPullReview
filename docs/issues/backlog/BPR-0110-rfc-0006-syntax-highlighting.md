---
id: BPR-0110
title: Implement complete-blob syntax classification
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0109]
---

# Implement complete-blob syntax classification

Implement the custom low-level highlighter that produces semantic spans and line-end state.

Highlight complete old and new blobs independently so a hunk beginning inside a multiline comment or string receives the same classification as the complete blob — this is the acceptance criterion that a hunk-local highlighter would silently fail. Cover C++, CMake, Markdown, JSON, Python, and plain-text fixtures selecting their expected definitions.

Switching theme or color must not re-run lexical parsing; prove it by counting parses across a theme change.

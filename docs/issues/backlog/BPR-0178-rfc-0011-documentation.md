---
id: BPR-0178
title: "Extend the AI agent boundary document"
status: Backlog
rfc: RFC-BPR-0011
depends_on: [BPR-0171]
---

# Extend the AI agent boundary document

`docs/architecture/ai-agent-boundary.md` already states the allocation, the
protocols, host policy, workspace modes, the supplement's contents, and session
semantics. It was written before any of this existed.

Extend it rather than writing a second document beside it: two documents would
leave a reader to work out which is current, and the answer would be "both,
partly".

## Required direct evidence

- What implementation settled and prose could not: the shape that makes the
  credential rule true rather than stated, the argv rule and the reason for it,
  and what each claim was checked against.
- Every claim in it is one a reader can check against a named test.
- `verify_documentation.py` and the issue-graph contract pass.

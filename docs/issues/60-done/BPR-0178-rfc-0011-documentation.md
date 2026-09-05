---
schema: "implement-rfc.card.v1"
id: "BPR-0178"
title: "Extend the AI agent boundary document"
status: "Done"
rfcs: ["RFC-BPR-0011"]
depends_on: ["BPR-0171"]
---

# Extend the AI agent boundary document

`docs/architecture/ai-agent-boundary.md` already states the allocation, the
protocols, host policy, workspace modes, the supplement's contents, and session
semantics. It was written before any of this existed.

Extend it rather than writing a second document beside it: two documents would
leave a reader to work out which is current, and the answer would be "both,
partly".

## What was added

A `What implementation settled` section, appended to the existing document
rather than placed beside it. It records what building this decided and prose
could not:

- **The credential rule as a shape.** Which types have no field one could
  occupy, and that the guard reads the type rather than a construction site —
  including why the structured-binding form replaced a `sizeof` sum.
- **Why the environment is an allowlist**: it is built by iterating the
  allowlist, never by enumerating the parent, so an unnamed variable cannot be
  reached. And why it is checked twice — the request can hold the boundary
  while the call loses it, which is a one-word mistake only the canary sees.
- **The argv rule and its reason**: `ps` on POSIX, the process list on
  Windows, and the structural separation of `Arguments` from `Input`.
- **Process ownership**, including that the launcher is deliberately outside
  the Qt adapter and that the scan refused the first attempt to put it there.
- **What is not claimed**: that this product cannot audit what a full agent
  independently accessed, and that nothing composes the launch surface yet.

Every claim names the test a reader can check it against — seventeen rows,
each naming a `ctest` name that exists.

## Required direct evidence

- What implementation settled and prose could not: the shape that makes the
  credential rule true rather than stated, the argv rule and the reason for it,
  and what each claim was checked against.
- Every claim in it is one a reader can check against a named test.
- `verify_documentation.py` and the issue-graph contract pass.

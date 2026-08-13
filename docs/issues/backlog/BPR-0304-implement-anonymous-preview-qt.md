---
id: BPR-0304
title: Implement explicit public-preview Qt presentation
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0303, BPR-0317]
---

# Implement explicit public-preview Qt presentation

## Required final state

- Qt names the mode public preview and never describes it as authenticated or
  equivalent to the review workbench.
- Viewer, resolution, viewed, actionability, mutation, triage, queue-role, and
  epoch associations are absent/disabled with accessible explanations.
- Measured rate limits, 404 ambiguity, GHES refusal, refresh availability, and
  switch-to-sign-in are presented exactly as BPR-0299 approved.
- Three-scale, focus, accessibility, incremental projection, cut-wire, and
  forbidden-action tests exercise the real adapter without provider/vault calls.
- A selected public file reaches the real rendered diff panes with exact path,
  base/head identity, content, and scroll/focus evidence; missing viewer identity
  does not silently remove the RFC's promised diff-reading surface.

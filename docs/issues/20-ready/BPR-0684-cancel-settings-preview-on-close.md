---
schema: "implement-rfc.card.v1"
id: "BPR-0684"
title: "Cancel Settings preview and pending import on close"
status: "Ready"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0682"]
---

# Cancel Settings preview and pending import on close

## Scope

The Settings close event currently returns focus without cancelling a palette
preview. Its in-flight import can also deliver a new preview after the window
closes. Add a close notification and route it through the existing appearance
intent boundary; cancel only preview/import work, preserving durable writes,
exports and syntax jobs. Keep accepted Settings panels alive for reopening.

Owned paths: Settings shell/workbench, appearance panel/host/composition and
focused Qt/host/public theme tests. Routing: standard | Reason: default implementation.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

## Acceptance

AC-03/04/09/10: Escape and native close restore the committed palette, retire
pending import identity and reject its late completion. Reopening retains stable
controls and accepted settings. An accepted durable write and unrelated syntax
work complete normally. Public composed theme course exercises actual close.

## Evidence

Discovered by source review during BPR-0682 integration preparation. Not run.

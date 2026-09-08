---
schema: "implement-rfc.card.v1"
id: "BPR-0684"
title: "Cancel Settings preview and pending import on close"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0682"]
acceptance: [{"criterion":"AC-03/04/09","outcome":"pass","revision":"3e11134e plus this finish","evidence":{"kind":"observation","detail":"qt_settings_window passed native close and Escape: one cancellation intent, stable controls on reopen. The actual application theme customization course passed 39.16s with Settings close replacing manual Cancel, preserving diff identity, position, lexical work and accepted palette."}},{"criterion":"AC-10","outcome":"pass","revision":"3e11134e plus this finish","evidence":{"kind":"observation","detail":"application_appearance_host passed staged late-import rejection, new import after reopen, accepted durable write and syntax completion surviving preview cancellation. Only theme-import request records retire; exports/writes are untouched."}},{"criterion":"Quality","outcome":"pass","revision":"3e11134e plus this finish","evidence":{"kind":"observation","detail":"Five selected TUs passed canonical tidy; reviewed composition digests and hostile-bypass test passed; documentation and diff checks passed. Attended conflict scan empty."}}]
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

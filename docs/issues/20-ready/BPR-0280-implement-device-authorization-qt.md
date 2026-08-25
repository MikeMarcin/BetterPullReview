---
id: BPR-0280
title: Implement device-authorization Qt presentation and browser handoff
status: Ready
rfc: RFC-BPR-0016
depends_on: [BPR-0277, BPR-0279]
---

# Implement device-authorization Qt presentation and browser handoff

## Required final state

- The enrollment UI keeps manual token entry first-class and offers a distinct
  device path with accessible user-code, verified address, expiry, copy, open,
  cancel, terminal, and retry presentation.
- Qt dispatches typed gestures and applies incremental projection only; it sees
  no token, refresh token, device code, client identifier, endpoint, or poll
  interval and performs no provider or vault effect.
- A narrow host-owned system-browser capability validates authority before open.
  Browser failure is nonfatal; no embedded browser, cookies, loopback listener,
  subprocess broker, or direct notifier-style bypass exists.
- Three-scale, focus, accessibility, cut-wire, off-authority, and failing-opener
  tests prove the real path and preserve manual enrollment.

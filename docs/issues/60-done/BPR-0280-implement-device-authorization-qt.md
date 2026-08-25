---
id: BPR-0280
title: Implement device-authorization Qt presentation and browser handoff
status: Done
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

## Implemented and closed — 2026-08-25

Three read-only scout lanes oriented the slice; the integrator wrote every
line and reproduced every command.

**The browser capability is host-owned because Qt is not allowed to hold an
address.** `VerifyArchitectureBoundaries.cmake` refuses `QUrl` and a literal
scheme anywhere under `src/adapters/qt/`, which is the rule that decided the
design rather than an obstacle routed around: `device_authorization_browser_port`
is declared beside the other four ports in `src/application/`, takes a
`std::string_view` and returns a `bool`, and receives no credential, device
code, client identifier, or endpoint. Its production implementation belongs
to composition and is not in this slice — the Qt adapter ships no opener and
cannot acquire one. `device_authorization_browser_handoff` carries the port
pointer and the compiled `Authority`, so a null pointer is the honest
representation of a build with no browser rather than a fabricated failure,
and a non-GitHub.com profile cannot silently inherit GitHub.com's authority.

**Opening a browser is not a core transition.** The RFC says a failed handoff
is not a failed authorization, so routing it through the core would have been
wrong by construction: `open_verification_address()` is a host method beside
`bind_refresh_authority`, it never calls `core::transition`, and the flow's
stage is provably unchanged across every outcome. What presentation learns
arrives as a fifth projection field, `Handoff`, published through the same
port by a `deliver()` split out of `publish()`; a stage change resets it, so
a stale "could not open" cannot outlive the flow it described.
`address_matches_verification_authority` is exact-host-over-HTTPS and nothing
else — no subdomain, no userinfo, no port, ASCII case folded on the host
only — and it runs *before* the capability is consulted, so a build with no
browser still refuses an off-authority address and the two are never
confused.

**Qt presents and dispatches.** `qt::device_authorization_panel` renders the
code, the address, the remaining validity, and the handoff result; every
field is a read-only `QLineEdit` so a keyboard reviewer can select and a
screen reader can read, and none is echo-masked because a device code is
meant to be read aloud. It applies incrementally — an unchanged projection
rebuilds nothing and announces nothing — and emits one polite, combined,
deduplicated accessible status per real change. Begin, cancel, and open leave
through `device_authorization_gesture_dispatch`; copy is Qt-local and copies
the code alone, never the address. Retry is a re-dispatched
`begin_device_authorization_intent` rather than a new core intent, and the
button says "Get a new code" so a retry does not read as a fresh start.

Two claims were narrowed rather than overstated. The Qt course proves the
device path *does not displace* manual token entry — no embedded prompt, no
credential editor, every field read-only — because whether the token modal
still enrolls a profile is a composition fact that [[BPR-0282]]'s public-path
course owns, not something this adapter can reach. And two UI strings were
reworded off `Authorization` to clear the Qt secret-token boundary scan; the
gate was satisfied, never relaxed.

Evidence at `dev`: `qt_device_authorization_scale_100|125|200` — the exact
selectors the RFC-BPR-0016 final command ledger already allocates — green at
1.0/1.25/2.0 with real device-pixel-ratio assertions and a nonempty raster
at each; `application_device_authorization_host` extended by nine browser
courses and green; 40/40 across the whole `qt_*` and `application_device*`
range; `check_boundaries`, `coding-format-check`, and
`verify_documentation.py` clean; `cmake --build --preset app-release` exit 0
with the panel compiled into the Release Qt adapter, because no focused
selector can see that configuration and [[BPR-0279]]'s review was caught by
exactly that gap.

Seven falsification probes, each reverted: a suffix-matching authority check
failed "a subdomain is not the authority"; a length-only check failed the
case-folding row; consulting the browser before validating failed "no browser
was opened for an address off the compiled authority"; escalating a failed
open to a core failure failed the handoff row; clearing the code on a failed
open failed "a failed open leaves the code on screen"; an assertive
announcement failed "politely"; and removing the equality short-circuit
failed "an unchanged projection rebuilds nothing".

The done move promotes exactly [[BPR-0281]], whose other dependency
[[BPR-0279]] is already done.

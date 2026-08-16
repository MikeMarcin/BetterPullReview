---
id: BPR-0313
title: Demonstrate deliberate credential-free native HTTPS
status: Ready
rfc: RFC-BPR-0017
depends_on: [BPR-0297]
---

# Demonstrate deliberate credential-free native HTTPS

## Required final state

- A `BUILD_TESTING`-only, non-product loopback course constructs an explicit
  typed credential-free request and proves zero vault lookup, credential
  discovery, authorization header, subprocess, and non-loopback outbound
  network.
- System trust, proxy, redirect, cancellation, timeout, and response limits use
  the existing native-HTTPS mechanics and remain fail-closed.
- Missing, malformed, or revoked authenticated material never falls back to the
  credential-free state; positive and cut-wire canaries prove both directions.
- The demonstration factory/symbol is absent from tests-disabled Release and
  does not create an anonymous product profile or partial public mode.
- This card creates `scripts/verify_rfc0017_test_inventory.py` with an exact,
  count-checked CTest JSON inventory for its demonstration selector and a
  platform-aware tests-disabled Release symbol/string absence check; later
  RFC-BPR-0017 cards extend the same script atomically with their selectors.

---
schema: "implement-rfc.card.v1"
id: "BPR-0313"
title: "Demonstrate deliberate credential-free native HTTPS"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0297"]
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

## Completed evidence

Implementation commit `b9bd446`; review corrections `aa4a514`.

### What landed

- `credential_free_request` is a typed binding carrying only a session context.
  It has no material field at all, so there is no empty or wiped credential for
  a later change to mistake for a usable one, and
  `make_native_https_credential_free_transport_for_test` takes no credential
  parameter, so the state can only be asked for by name.
- The binding, its factory, its `implementation` constructor, its
  `credential_matches` branch, and the branch that omits the header all sit
  inside `#if defined(BPR_NATIVE_HTTPS_TESTING)`, which
  `src/adapters/github/CMakeLists.txt` applies only when `BUILD_TESTING` is on.
  A tests-disabled Release build therefore has no omitting branch at all, which
  is why RFC-BPR-0017's finding about the product path stays literally true.
- `rfc0017_credential_free_native_https` runs the demonstration against the same
  loopback CONNECT and TLS fixture and the same configuration record every other
  native-HTTPS scenario uses. The child is handed an empty credential frame; the
  fixture treats any authorization header as a contract violation; two requests
  prove a deliberate absence rather than a spent credential; and a third,
  pull-request-scoped request is refused before the wire, which the fixture's
  exact two-request count is what proves.
- `scripts/verify_rfc0017_test_inventory.py` counts the selector from CTest's own
  JSON, checks the registered command carries `--suite rfc0017`, and scans a
  tests-disabled Release binary for the demonstration's symbols and strings. It
  anchors on the sibling `native_https_transport_fixture` so it cannot pass
  vacuously in the core-only graph, and records
  `make_designated_review_public_observer` as a known pre-existing product
  credential-free symbol.

### Independent review and what it changed

The review returned no blocking finding but one real regression and several
overclaims, all repaired in `aa4a514`:

- `credential_bytes()` discriminated on whether an alternative had a `Material`
  field, turning a former compile error into a silent empty credential for any
  future alternative that renamed or omitted it. It now discriminates on the
  type.
- The credential-free branch of `credential_matches` correlated on session
  identity alone, which would have served a pull-request-scoped read for an
  arbitrary account. It now refuses that shape, and a canary proves it.
- The revoked-material assertion was a verbatim duplicate of the wiped one, and
  the account factory had no invalid-material coverage at all. Replaced with
  wiped and malformed cases against the account factory, which refuses wiped
  material with `invalid_account_credential` rather than
  `invalid_credential_material` — the assertion now says what actually happens.
- One forbidden string in the inventory script was a CTest selector name, which
  lives in `CTestTestfile.cmake` and can never appear in a binary. Both
  forbidden strings now fire against the tests-enabled binary.
- `can_acquire_smart`'s `!credential_free()` term is inert: `credential_matches`
  already refuses by requiring an account credential. The term is kept as
  readable defense in depth and its comment now says so, rather than being cited
  as the mechanism. No test distinguishes it from its absence, and none can.

### Cut-wire checks

Each of these was run and reverted, so none of the passing results above is
vacuous:

- Changing `if (!credential_free())` to `if (true)` in `make_request` makes
  `rfc0017_credential_free_native_https` fail with
  `credential-free-proxy-contract`.
- Running the absence scan against the tests-enabled binary fails on both
  forbidden symbols and both forbidden strings; against the shipped Release
  application it passes.
- Changing the expected selector count from 1 to 2 fails the inventory check.
- Changing the required command argument makes the inventory check fail with
  `command omits`.
- The isolation manifest initially rejected the new test for taking loopback
  ports undeclared, which is the gate working; the ports are now declared in
  `MACOS_LOOPBACK_AUTHORITIES`.

### Gates

`ctest --preset dev` 186/186 and `ctest --preset core-only` 86/86, both twice
across the implementation and the corrections. Dev, core-only, and app-release
builds clean under `-Werror`. `coding-format-check`, `coding-tidy`,
`verify_documentation.py` (`DOCUMENTATION_OK`), and `git diff --check` pass.

### What this does not establish

The card's first bullet says "proves". Two of its five sub-claims are proven by
a command that would fail if false: the absent authorization header, by a
fixture that fails when one appears; and the absence of non-loopback traffic, by
the sandbox profile that denies it at the kernel rather than asserting it. The
other three — zero vault lookup, zero credential discovery, and no subprocess —
are true by construction, since the native HTTPS path contains no vault or
process call and the process is handed nothing, but no command here would fail
if one were added. They are inherited properties, and the documentation now says
so rather than claiming otherwise.

Likewise, credential-free redirect, cancellation, timeout, and response-limit
fail-closed behavior is exercised through shared code rather than
re-demonstrated; `RFC0017_SCENARIOS` holds one scenario. Extending it is
available to a later card if that inheritance stops being acceptable.

No service fact was resolved, no network authority was used or granted, no
anonymous product profile or partial public mode exists, and no production
factory changed. RFC-BPR-0017 remains `Deferred`.

---
id: BPR-0446
title: The native HTTPS wire fixture has no Windows registration
status: In Progress
rfc: RFC-BPR-0002
depends_on: []
---

# The native HTTPS wire fixture has no Windows registration

`native_https_transport_fixture` is the real-wire TLS matrix: a Python origin
with openssl-generated certificates drives the transport test executable
through thirty-six scenarios — trusted TLS, untrusted certificate, system
proxy, same/cross/query/GraphQL redirects, cancellation, timeouts, response
and header limits, rate-limit metadata, and the HTTP failure family. The
RFC-BPR-0002 verification ledger's Windows closeout paragraph requires
exactly that evidence target-native: "native HTTPS, TLS, proxy, redirect …
a macOS or Linux success cannot substitute for that run" — and it matters
doubly on Windows, where Qt's TLS backend is Schannel rather than the
OpenSSL/SecureTransport stacks the existing registrations exercise.

The gap is registration, not code. The test executable
(`test_better_pull_review_native_https_transport_tests`) builds and passes
its hermetic `--contract` mode on Windows (matrix-8 #128), and it parses
every `--fixture` scenario on every platform. Only the CTest registration in
`tests/github_contract/CMakeLists.txt` sits inside `if(APPLE OR
CMAKE_SYSTEM_NAME STREQUAL "Linux")`, apparently for the `find_program(…
openssl REQUIRED)` it needs. The driver
`tests/scripts/verify_native_https_transport.py` is dependency-free Python
(socket/ssl/subprocess) with no POSIX-only calls, and this host has OpenSSL
3.5.7 at `C:/Program Files/Git/mingw64/bin/openssl.exe`.

## Required final state

- The fixture registration extends to Windows with a discoverable openssl
  (a hint path or requirement note where PATH does not carry one), keeping
  the APPLE/Linux registrations byte-identical in behavior.
- `native_https_transport_fixture` passes target-native on the Windows host
  through the isolation launcher's loopback grants, or every failing
  scenario is attributed to a measured Windows cause on this card.
- The RFC-BPR-0002 ledger's Windows native-HTTPS row cites the passing run.

## Non-goals

- No transport or scenario changes; the same executable and driver run.
- Not RFC-BPR-0017's separately registered credential-free selector.

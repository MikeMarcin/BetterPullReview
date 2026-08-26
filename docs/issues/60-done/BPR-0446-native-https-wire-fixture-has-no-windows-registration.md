---
id: BPR-0446
title: The native HTTPS wire fixture has no Windows registration
status: Done
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

## Run on the designated host — 2026-08-26

The registration landed at `b18b9336`: a `WIN32` block with the Git for
Windows openssl hint (`find_program` resolved
`C:/Program Files/Git/mingw64/bin/openssl.exe` into the cache), uncontained
for the measured dos-volume-name cause, pinned in
`WINDOWS_UNCONTAINED_TESTS`, with the APPLE/Linux registration untouched.

**`native_https_transport_fixture` passed target-native in 9.35 seconds** on
the freshly built tree at that commit — all thirty-six scenarios of the
default suite through Qt's Schannel TLS backend, the run the ledger's
Windows closeout paragraph said no macOS or Linux success could substitute
for. The `BPR_NATIVE_HTTPS_TESTING` trusted-CA injection
(`addCaCertificate` on the fixture authority) holds under Schannel's
verification exactly as under OpenSSL and SecureTransport.

`test_network_isolation_manifest` then passed at `f8799ca9`, after
[[BPR-0460]] pinned the five upstream `WINDOWS_UNCONTAINED` registrations
the same fresh-configure run exposed — drift this card's diff did not
create and whose repair it therefore did not absorb.

Two measurement artifacts are recorded so nobody re-reads them as
evidence. The day's first fixture run printed
`native-https-transport-fixture:fail:success-child-exit` in 0.25 seconds:
today's driver against the stale 2026-08-23 executable, after a delegated
build lane's `cmd /c` invocation was silently reduced to a no-op by the
Bash tool's MSYS path conversion and its exit 0 was misread as a build.
That run is void, not a failing scenario. The same lane then deleted
`build/windows-dev` outright and reconfigured it fresh — recorded on
[[BPR-0036]], whose reputation-settled tree that was. One incidental
positive: the fixture's own freshly linked image drew no Smart App Control
refusal at all.

Implemented and closed by the integrator.

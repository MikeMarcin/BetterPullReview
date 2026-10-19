# Release security qualification policy

RFC-BPR-0014 M9 applies this policy to an exact source revision and the exact
delivered package bytes. Passing a build, a signature command, or the mechanical
checks below is not independent release acceptance. The existing
[release workflow](../development/release-packages.md) and `publish-release`
skill remain the only macOS release path. Offline preparation cannot authorize
signing, notarization, trust enrollment or publication.

## Evidence and stop conditions

Each record names source, executable, package, SBOM and relevant producer
identities, command and exit status, tool version, input scope, and unresolved
findings. Keep raw reports locally; commit redacted summaries and hashes, not
private crash reports, credentials, source bodies or signing inputs. Re-run
affected checks after a source/dependency/toolchain/package change. Unchanged
evidence may be reused only with its original scope and identity.

- Independently verify Authenticode for Windows; Developer ID, hardened runtime,
  notarization/staple and Gatekeeper for macOS; and package/repository signatures
  for the declared Linux distributions. Check every checksum against the same
  delivered artifact. Ad-hoc seals and unsigned installers are development
  evidence, never substitutes. Missing authority/native hardware stays pending.
- `verify_release_package.py` checks the selected target graph, first-party
  composition, binary/dependency allow-list, forbidden fixture seams, and exact
  SPDX file inventory. Stripped first-party PE images require their byte-identical
  producer plus full public/static linker map, bound to image name, timestamp,
  image base, entry point and symbol addresses. Only anchor names need native
  demangling; all decorated names are scanned. A map is trusted build evidence,
  not a cryptographic signature or independent proof of its own completeness.
  Upstream DLLs require exact Qt SPDX or selected Microsoft CRT producer bytes
  and exports; private upstream symbols are not claimed observable. Qt's Windows
  SPDX precedes signing: when raw hashes differ, reconstruct the pre-sign whole
  file in memory by clearing only the checksum/certificate directory and removing
  the trailing certificate. Every upstream hash must then match, and native
  Authenticode must report Valid for The QT Company Oy. Any other difference
  fails. This does not alter or resign the DLL. Microsoft's
  [PE signature format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
  specifies those excluded signature fields.
- Package audits reject an embedded corresponding-source archive. They bind
  its separate versioned release name, exact URL, producer revision and SHA-256
  through the installed provenance/notices/source offer and the SPDX. The
  macOS publication preflight checks those exact references inside the app ZIP
  against the separate source ZIP before remote operations. Checksums and the
  upload set include the source asset; native MSI/DEB/RPM production emits it
  beside the binary artifacts too.
- `verify_release_security.py` additionally validates retained notice texts, Qt transitive
  package identities, source offers, actual compiler commands and emitted image
  protections. Legal/owner review must resolve first-party distribution terms,
  mixed syntax-definition licenses, Qt/third-party obligations, toolchain terms,
  and practical corresponding build/relink material. The separate source asset
  receives equivalent access for every binary recipient at no extra charge,
  including a distribution-channel fallback after private repository access
  changes and a copy alongside corporate/offline redistribution. Retention is
  indefinite until the distribution owner documents that all applicable
  source-offer/license obligations have ended, and never ends while matching
  binaries remain distributed. Keep the asset and version tag immutable.
  Offline validation cannot certify those ongoing access/retention arrangements.
  Text presence alone does
  not discharge these obligations. See [Qt licensing](https://doc.qt.io/qt-6.11/licensing.html)
  and [LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations).
- First-party and selected SQLite Release compile commands require strong stack
  protection, frame pointers and `_FORTIFY_SOURCE=3` on Clang/GCC, or `/sdl`,
  `/guard:cf` and enabled `/GS` on MSVC. Check emitted Mach-O executable PIE,
  no executable stack or writable/executable segments; ELF PIE, full RELRO/NOW
  and non-executable stack; PE high-entropy ASLR/DEP and non-writable code. PE CFG
  header presence is recorded separately; it alone does not prove all guarded
  call sites. Prebuilt Qt and external KSyntaxHighlighting/LibArchive compile
  hardening need separate producer qualification; a first-party flag cannot
  vouch for them. Platform signing/entitlement policy remains an additional gate.
- Focused clang-tidy uses the exact compilation database and warnings as errors.
  Correct changed-surface findings; retain unrelated/pre-existing failures and
  their evidence rather than silently excluding them. Run the relevant core,
  host, containment, diagnostic and public-composition regression tests.
- Build the two `tests/fuzz/release_*_fuzz.cpp` harnesses with Clang libFuzzer,
  ASan and UBSan, no sanitizer recovery and frame pointers. The maintenance
  harness links `src/core/src/release_maintenance.cpp`; the diagnostic harness
  links `src/application/src/release_diagnostic_snapshot.cpp` and
  `src/core/src/review_submission_unknown.cpp` and
  `src/core/src/release_maintenance.cpp` (the diagnostic export option validator),
  with core/application include
  roots. Its digest uses the production SHA-256 composition from
  `src/core/CMakeLists.txt`: `sha256_hacl.cpp`, the selected platform backend
  (`sha256_commoncrypto.cpp` on macOS), and the pinned HACL* SHA-2 dependency
  with the include/link inputs declared by `cmake/BprHaclSha2Provider.cmake`.
  Preserve those inputs when compiling the harness directly; one backend file
  alone does not provide the public hash implementation or its fallback.
  Record which dependency code the sanitizers instrument. Run each with
  at least 100,000 iterations, seed 140962, five-second input timeout, maximum
  input sizes 256 and 16384 respectively. Retain compiler/version, flags,
  sanitizer failures, final statistics and reproducer hashes. Any crash,
  sanitizer finding or invariant failure blocks. This is bounded selected-path
  evidence, not whole-program or exhaustive fuzz coverage. Native host/vault
  lifecycle and third-party parsing qualification are separate obligations.
- Run Gitleaks against the exact committed RFC delta and candidate source tree,
  with redacted output. Any real secret blocks and requires owner-led revocation;
  do not print it or automatically contact the provider. Reviewed synthetic test
  matches require exact, narrow findings dispositions, never a broad allow-list.
- Run Grype on each final artifact's SPDX, retaining the scanner version/digest,
  full matching report, database URL/checksum/schema/build time and package
  coverage. Require a valid database built within 72 hours of the scan. Missing
  component identifiers, unknown versions, unrepresented static dependencies or
  unavailable feeds mean incomplete coverage, not zero vulnerabilities.
  Critical/High matches block until fixed or an independent named security
  owner accepts a source-backed, exact-version applicability decision with an
  expiry/recheck condition. Lower-severity findings also require disposition.
  Never infer non-applicability only from an idle launch or an unused filename.

## Local IPC is not a network service

Under mandate r3 the obsolete absolute “no local server” wording is replaced
with the intended no-network-server boundary. The declared listener exceptions
are exactly two, both bounded binary AF_UNIX rendezvous points owned by the
profile holder. The profile activation transport, in an owner-private real
directory, admits only the same OS user and the exact profile, validates a
fixed-size versioned frame, and hands a typed activation to the existing
application/core policy. The owner command endpoint (RFC-BPR-0045, extended by
RFC-BPR-0058) is a separate per-user rendezvous bound only while the exact-root
lease is held, an owner-private named pipe on Windows; it advertises a versioned
owner binding (protocol 3), admits only the same OS user, bounds every frame to
5 MiB, and carries a closed set of local draft, hook and assist commands and the
finite `bpr cycle` command registry, which the owner runs against its own draft
and store. Neither serves HTTP, remote content, an arbitrary command, a
credential or a provider endpoint, and neither carries publication authority. No TCP/UDP listener, loopback web server, browser/
Node runtime, arbitrary plugin loader or undeclared network destination is
allowed. This does not exempt normal outbound host capabilities from the
[existing network/process inventory](release-updater.md).

Inspect named process-owned AF_UNIX endpoints as well as TCP/UDP. Check endpoint
and parent owner/mode, malformed/oversized/version/profile refusal, actual second
launch handoff, owner lifetime and shutdown. Peer-credential and race refusals
also need the existing adapter tests; the same-user course cannot simulate a
foreign UID. Windows has no implemented profile activation transport, and
returning an empty Unix inventory there does not certify all Windows named
pipes. An unknown listener or inspection failure blocks, rather than becoming
an empty successful inventory.

`tests/end_to_end/release_security_course.py` exercises the tests-disabled macOS
app under a qualification guard and disposable child home. Its optional SIGABRT
course inspects only a new report matching its own process. Synthetic environment
canaries absent from an OS report are narrower evidence than a crash with real
resident synthetic review/token material. The latter, ordinary-user vault/root
permissions, diagnostics preview/export with source/comment/token canaries,
recovery, retention and removals remain required on the supported native matrix.
No automatic crash upload or credential enrollment is authorized by this course.

## M9 initial findings, 2026-09-21

Grype 0.119.0, database v6.1.9 built `2026-09-21T06:39:24Z`, returned exit 2
against M8's exact macOS SBOM `7d543637ab167827b0f69efb89a701daa894d9872407c01cf87a8e90ee341d40`:
21 matches, including Critical zlib `CVE-2022-37434`, High PCRE2
`CVE-2026-89157`/`CVE-2026-89161` and a High Bitnami SQLite match. Qt's retained
producer metadata identifies bundled PCRE2 10.47; [PCRE2 10.48's release notes](https://github.com/PCRE2Project/pcre2/releases/tag/pcre2-10.48)
document corresponding security fixes. No suppression or upgrade was performed
in that initial scan round. The zlib system/SDK-versus-shipped distinction and Bitnami
SQLite version-range matches need exact-artifact adjudication; the scan is not
proof that every reported vulnerability is exploitable. It is nevertheless a
failed release gate, and lower-severity findings and scan coverage remain open.

The subsequent owner-selected Qt 6.11.2 remediation has its own
[exact artifacts, unfiltered scans and primary-source applicability evidence](../verification/rfc0014-m9-dependency-remediation.json).
Its fresh results are 32 macOS and 30 Windows matches, both exit 2. The libpng
finding is removed; twelve additional matches per platform concern Qt's Tika
MIME-XML attribution. Proposed component/version/call-path distinctions do not
replace the independent named security-owner decision required above. Old
21/19 reports and licensing decisions remain bound to their historical bytes.

The [RFC ledger](../verification/RFC-BPR-0014.md) retains final candidate identities,
commands, tested scope and remaining independent/native/security-owner decisions.

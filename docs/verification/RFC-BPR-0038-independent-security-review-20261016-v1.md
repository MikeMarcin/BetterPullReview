# RFC-BPR-0038 independent vulnerability review — 2026-10-16, v1

The technical review covers **all 64 scanner rows and both outside-feed PCRE2
entries**. It supports 53 narrow applicability proposals, requires remediation
for three Windows native-zlib High matches, and leaves eight scanner rows
pending for insufficient evidence. The outside-feed DFA assessment is supported;
the JIT-stack assessment remains pending. **Both historical release gates still
fail.**

The [versioned decision record](RFC-BPR-0038-independent-security-review-20261016-v1.json)
contains each original row ID, exact artifact and SPDX package identity,
version, advisory, decision, rationale, primary references, evidence, and recheck
condition. It also contains the reproducible allocation probe and its scope.
The [input packet](RFC-BPR-0038-independent-security-review-packet-20261016.md),
[queue](RFC-BPR-0038-independent-security-review-queue-20261016.json), proposals,
raw reports, and source summaries retain their original bytes.

Reviewer: **Codex independent technical reviewer**, chat
`01a14646-bfaa-7fd3-bc0b-a152e9ea9900`, separate from the proposal author.
Review date: **2026-10-16**. Accountable independent security owner:
**not designated**. Michael Marcin's distribution-owner assignment does not
assign that separate role. An `accept` below accepts only the stated technical
analysis; no security-owner acceptance or release authorization is recorded.

## Findings and unresolved decisions

### R1 — Requesting all JIT modes does not establish the proposed workaround

Rows **MAC-001, MAC-007, WIN-001, WIN-007** remain pending for
`CVE-2026-89156`. Qt's `optimizePattern()` requests all three JIT modes but
ignores the result. PCRE2 10.47 installs modes successively and returns on a
later error without removing the earlier mode. The
[upstream workaround](https://github.com/PCRE2Project/pcre2/security/advisories/GHSA-2p8c-ff85-vh9x)
requires the relevant matching modes to be compiled, not merely requested.

A bounded allocation-failure probe against the exact retained PCRE2 sources
returned JIT error **-48** while complete-mode matching still returned **1**;
both partial modes returned **-45**. The failure occurred on the sixth
JIT allocation. Exact Mac x86_64 and Windows QtCore disassembly independently
confirms the ignored return value. See `E-JIT-PROBE` and `E-PCRE-BINARY` in the
decision record, and the source at
[Qt's call](https://github.com/qt/qtbase/blob/ef55f427f2c8b410d34f8a7681020a3000cf6866/src/corelib/text/qregularexpression.cpp#L1001)
and [PCRE2's sequential compilation](https://github.com/qt/qtbase/blob/ef55f427f2c8b410d34f8a7681020a3000cf6866/src/3rdparty/pcre2/src/pcre2_jit_compile.c#L14277).

This is a counterexample to the proposed invariant, **not an application
exploit reproduction**. The probe uses a harmless valid subject and a separate
JIT-enabled source build. Upstream leaves 16-bit susceptibility unconfirmed;
the exact Mac arm64 Qt slice has JIT disabled. Resolve the x86_64 applicability
with the failure case included, or qualify fixed PCRE2 bytes. A successful
ordinary match does not resolve this question.

### R2 — The outside-feed JIT-stack question needs an actual disposition

The affected mechanism in
[GHSA-r9hj-j2rw-4q3m](https://github.com/PCRE2Project/pcre2/security/advisories/GHSA-r9hj-j2rw-4q3m)
is present in Qt's x86_64 path: it assigns a stack callback, creates a growable
32–512 KiB stack after a stack-limit result, then retries. The advisory covers
16-bit code units. Mac arm64's compiled no-JIT stub excludes that slice.

There are meaningful restrictions: the first-party expressions found are
fixed, source search uses RE2, and KSyntaxHighlighting disables standard search
paths and escapes dynamic captures. This review did **not** establish a
complete pattern-origin and complexity bound for every reachable Qt/KSyntax
consumer. It therefore does not claim an attacker can exploit this app, or
accept a blanket non-applicability conclusion. The forward remediation target
is [PCRE2 10.49](https://github.com/PCRE2Project/pcre2/releases/tag/pcre2-10.49).
This remains a separate coverage entry; no scanner row was invented.

### Existing Windows native-zlib High blocker is confirmed

**WIN-013, WIN-026, WIN-027** retain `remediate` decisions. The exact Qt source
contains zlib 1.3.2's uncorrected failed-write branch, and its target includes
`gzwrite.c`. It lacks the state resets in the
[upstream fix](https://github.com/madler/zlib/commit/df84af25dc1942490e1d1c899a07619152a46148).
No corresponding consumer or PE import/export was found in the inspected
scope; this does not prove stripped internal code absent. A source patch or a
different build cannot remediate these MSI bytes. Require a fixed, qualified
package or the release policy's explicit independent applicability route.

### External runtime evidence remains incomplete

**MAC-025–027**: the header attribution mismatch is supported, but it cannot
certify a dynamically loaded D-Bus implementation. A standalone loader probe
found none of Qt's candidate library names on the current Mac. That is not an
active application load trace or a result for all selected native environments.

**MAC-032**: neither the Apple ABI version nor the current host observation
establishes inclusion of the CRC-combine fix across macOS 13+. No shipped
Mach-O slice imports the affected API family; the current system library lacks
`crc32_combine_gen64`. The remaining alias/internal-consumer and vendor-build
mapping questions are stated explicitly in `A-APPLE-27171`.

By contrast, **MAC-031** has a supported vendor-baseline rationale: both app
and QtCore slices declare macOS 13.0 as their minimum, and
[Apple identifies CVE-2022-37434 as fixed in Ventura 13](https://support.apple.com/en-us/102853).
That technical conclusion still awaits attributable security-owner acceptance.

## Complete scanner-row coverage

`Accept` means the narrow technical proposal is supported. `Pending` means
`insufficient-evidence`; it is not a waiver. Every grouped ID below has its
own complete row in the JSON record.

| Advisory or component assessment | Mac rows | Windows rows | Technical decision |
|---|---|---|---|
| PCRE2 CVE-2026-89156 | 001, 007 | 001, 007 | Pending: partial JIT compilation |
| PCRE2 CVE-2026-89157 | 002, 008 | 002, 008 | Accept: architecture/API prerequisites absent |
| PCRE2 CVE-2026-89158 | 003, 009 | 003, 009 | Accept: process/code-unit prerequisites absent |
| PCRE2 CVE-2026-89160 | 004, 010 | 004, 010 | Accept: required compile flag absent |
| PCRE2 CVE-2026-89161 | 005, 011 | 005, 011 | Accept: copied-subject reuse absent |
| PCRE2 CVE-2026-89162 | 006, 012 | 006, 012 | Accept: no traced serialization consumer |
| Tika CVE-2016-6809 | 013 | 014 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2018-11761 | 014 | 015 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2018-11762 | 015 | 016 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2018-11796 | 016 | 017 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2018-1335 | 017 | 018 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2018-1338 | 018 | 019 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2018-1339 | 019 | 020 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2021-28657 | 020 | 021 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2022-25169 | 021 | 022 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2022-30126 | 022 | 023 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2022-30973 | 023 | 024 | Accept: XML resource / Java handler mismatch |
| Tika CVE-2022-33879 | 024 | 025 | Accept: XML resource / Java handler mismatch |
| D-Bus CVE-2022-42010 | 025 | — | Pending: external implementation |
| D-Bus CVE-2022-42011 | 026 | — | Pending: external implementation |
| D-Bus CVE-2022-42012 | 027 | — | Pending: external implementation |
| SQLite BIT-sqlite-2024-0232 | 028 | 028 | Accept: exact 3.53.4 beyond upstream fix |
| SQLite BIT-sqlite-2025-29088 | 029 | 029 | Accept: exact 3.53.4 beyond upstream fix |
| SQLite BIT-sqlite-2025-6965 | 030 | 030 | Accept: exact 3.53.4 beyond upstream fix; raw High retained |
| Apple zlib CVE-2022-37434 | 031 | — | Accept: vendor-fixed supported baseline |
| Apple zlib CVE-2026-27171 | 032 | — | Pending: Apple patch/call-path mapping |
| Ruby-zlib GHSA-g857-hhfv-j68w | 033 | 031 | Accept: native-library / Ruby-gem mismatch |
| Native zlib CVE-2026-85091 | — | 013, 026, 027 | Remediate; all three High rows retained |

The six SQLite rows have individually checked upstream fixed versions and raw
Bitnami constraints. Exact source archives and embedded SQLite `SOURCE_ID`
values agree. The Tika rows were checked separately against their handler
descriptions, including the CNA's `CVE-2022-30126` identifier where Apache's
summary table has a different number. The Ruby finding concerns the Ruby
wrapper; the separate first-party native zlib provider compiles only its six
raw-inflate/checksum files. Full primary references are attached to each row.

## Evidence and verification

- Rehashed all 14 retained release assets and both raw reports; verified the
  frozen summary identities and the exact 64-row queue/raw bijection.
- Independently reran both exact report-binding verifiers: **exit 2**, with
  **33 Mac / 31 Windows** matches. No suppression or changed database was used.
- Verified all **44 Mac SPDX files** and all **24 MSI SPDX payload files**.
  Direct `cabextract` against the MSI failed on fragmented OLE storage; those
  outputs were excluded. Extracting the complete OLE cabinet stream first
  succeeded. The MSI evidence therefore does not depend only on the runtime ZIP.
- Inspected **28 Mach-O slices and 17 AMD64 PE images**, and the hash-bound Qt,
  SQLite, zlib, KSyntaxHighlighting, LibArchive and exact first-party sources.
- The focused partial-JIT probe compiled and passed its intended counterexample
  assertion. The record includes its source and reproduction instructions.
- Standalone runtime probes used macOS **26.6.2 (25G83)**. x86_64 results are
  **Rosetta observations**, not native Intel qualification. No application
  window, browser, user profile, bus connection or remote mutation was needed.

Record-consistency checks, documentation links and conflict-marker checks
passed. All **67 documentation unit tests passed**. The full documentation
gate failed on existing issue-counter drift: this checkout's `BPR-1558`
rewinds the shared historical high water `BPR-1566`. The worktree also lacks
its local managed skill deployment; the checks used the existing project's
skill scripts through `PYTHONPATH`. Neither condition was repaired in this
review. Exact outcomes are in the JSON's `validation` field.

Raw scanner failures, absent native execution, incomplete runtime mapping and
unassigned security-owner acceptance remain visible.

## Recheck and acceptance boundary

Recheck before the next release, on any source, package, dependency, build
option, architecture, supported OS or advisory-feed change, or by
**2026-11-15**, whichever comes first. The exact frozen historical scope is
Mac 0.2.1 from `c233c768327cd6c0cb6807860625ec12a9aeac14` and Windows 0.2.2
from `23447f3a9bacf9a080a93df18b1fb2f54f6ebd3d`.

The named independent security owner must explicitly accept policy-relevant
applicability decisions. This review supplies technical analysis for that
decision and does not supply the missing authority. **BPR-0962 remains Blocked,
AC3 remains failed, and no RFC-BPR-0038 acceptance criterion is closed.**

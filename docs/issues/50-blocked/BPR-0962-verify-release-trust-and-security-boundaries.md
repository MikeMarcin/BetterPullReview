---
schema: "implement-rfc.card.v1"
id: "BPR-0962"
title: "Verify release trust and security boundaries"
status: "Blocked"
rfcs: ["RFC-BPR-0038"]
depends_on: ["BPR-0957"]
milestone: "M9 Security and boundary verification"
metadata: {"milestone_plan":{"id":"rfc-bpr-0014-release-readiness","path":"docs/rfcs/plans/rfc-bpr-0014-release-readiness/milestone-plan.json"}}
blocked_by: "release-security-inputs"
---

# Verify release trust and security boundaries

## Security-audit port and what still blocks, 2026-10-22

The unmerged 2026-10-16 audit (`codex/rfc0038-security-audit-20261016`, which
contains `codex/rfc0038-qualification`) is now on main. That covers its scan
verifier and controls, its row-review method, the owner's Qt known-issues
decision and its 0.2.x evidence, kept as history. The
[ledger](../../verification/RFC-BPR-0038.md#security-audit-port-and-current-release-scan-recheck-2026-10-22)
lists what was taken. An offline rerun of the verifier against the published
v0.3.1 Mac, Windows and Ubuntu packages and the v0.3.2 Mac prerelease exits 2
on 21, 19, 21 and 21 matches. Those are the same rows v0.3.1's reviewers saw.
No criterion is accepted. This card stays **Blocked** on
`release-security-inputs`, for these reasons:

- **AC3 fails.**
  - Every current scan exits 2.
  - v0.3.1's factual dispositions expire 2026-11-19. No accountable named
    security owner has accepted them, as the release policy requires for
    Critical and High rows.
  - Windows bundled zlib `CVE-2026-85091` (High, three rows) is unpatched in
    Qt 6.12.0. It rests on a closed-caller disposition only.
  - PCRE2 `GHSA-r9hj-j2rw-4q3m` / `CVE-2026-103111`, which the feed omits,
    has no Qt 6.12.0 disposition (BPR-1742).
  - Coverage is incomplete, with 52, 44 and 61 flagged package rows.
  - Complete sanitizer and fuzz coverage, secret scanning and crash canaries
    with real resident material have not been run.
- **AC2 is pending.** Active-operation network, process, loader, socket and
  IPC inspection across the native matrix has not been done. Nor have the
  foreign-user IPC refusals or the Windows and Linux ordinary-user runtime and
  vault courses.
- **AC1 is pending.** v0.3.1 has downloaded-byte trust checks and the owner's
  material acceptance in its publication record, but nobody has assessed
  that evidence against this criterion. Each later artifact, v0.3.2 included,
  needs its own. Recipient access and retention are ongoing obligations.
- **Native environments and decisions are missing.** There is no native Intel
  Mac, clean Apple Silicon account, ordinary-user Windows 11 desktop or
  Ubuntu 26.04 WSL2/WSLg course. The M8/M9 native, vault and crash gaps
  remain. One owner decision is pending: whether the Qt deferral extends to
  Qt 6.12.0 (AW1). The owner confirmed the 2026-10-16 matrix amendment on
  2026-10-23 UTC (BPR-1743): Windows 11 and Ubuntu 26.04 WSL2 on JaegAW are
  the tested environments, and others are unproven but not disallowed.

Bounded work from the immutable RFC-BPR-0014 authored issue plan. The live
RFC and its owner amendments remain authoritative. Reuse inherited evidence
only for unchanged inputs; this card is unaccepted until its direct criteria pass.

## Owner amendment r4 — successor qualification

This unchanged-criteria standalone follow-up now belongs to
[RFC-BPR-0038](../../rfcs/RFC-BPR-0038-native-package-release-security-qualification.md).
RFC-0014 closes only its implemented foundation. No pending criterion, native
observation, trust/security gate or human decision is accepted by this transfer.
Paths, blocked state and exact historical evidence remain intact. The
[successor ledger](../../verification/RFC-BPR-0038.md) records the latest Qt 6.11.2
candidates, completed PE mechanics, raw 32/30 Grype failures and unanswered
Windows licensing-v002 decision; earlier records below remain historical.

## R4 current-check follow-up — network-isolation manifest

At unchanged source `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`, the
14-test closeout selection passes 13 but fails `test_network_isolation_manifest`.
The checkout-wide scan counts an unrelated ignored nested worktree's test
registrations. Separately, the actual registration in
`tests/github_contract/CMakeLists.txt` gives `release_update_native_https`
one loopback port, while `MACOS_LOOPBACK_AUTHORITIES` in
`scripts/verify_test_network_isolation.py` has no corresponding entry and
expects zero. This mismatch remains when inspecting tracked source separately;
that diagnostic run also refuses mismatched absolute source/build command paths
and is not a passing full manifest gate.

AC2/AC3 follow-up: reconcile the exact closed-fixture authority with the pinned
manifest, retain hostile authority tests, then run the full manifest on a clean
matching source/build pair without weakening isolation. Do not remove another
owner's nested checkout or count this existing failure as a pass. No source or
test change was made by r4. The receipts are summarized in the r4 ledger.

## Acceptance

- AC1: Independently verify Windows signatures, macOS signatures/notarization and Linux package signatures against each exact delivered artifact; reconcile checksums, SBOM, transitive dependency provenance, notices and source-offer obligations.
- AC2: Inspect packaged dependencies, sockets, process launches, plugin/library resolution and network destinations; verify only declared first-party composition owns effects, core remains Qt/provider-free, and no WebEngine/browser/Node/server/arbitrary plugin loader is introduced.
- AC3: Define and pass the release hardening, static analysis, sanitizer, fuzz, secret/vulnerability-scan and diagnostic/crash-canary policy; retain exact failures, tool/database versions and artifact identities. Missing native environments or signing authority remain pending owner/operator obligations.

## Verification and execution record

M9 owner `/root/rfc0014_m9`, mandate r3, coordinator-implementer rev2, on main.
Claim `8d75ff697d8aae83a54a6396fa7a71a519ec56ba`; immutable RFC base remains
`d0a8a6dda005af12b3818234f4878a9f48a44146`. M7 is the accepted prerequisite;
M8's blocked native work and exact human decisions are preserved.

The [M9 ledger](../../verification/RFC-BPR-0014.md#milestone-9-security-and-boundary-verification),
[policy](../../security/release-policy.md), [scan dispositions](../../verification/rfc0014-m9-security-evidence.json)
and [handoff](../../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m9.json)
bind source, tools, artifacts, failures and remaining obligations. Producer
`9409ff618c0e8c567fe266195ec8228852b18485` supplies new offline macOS and unsigned
Windows packages; verifier `f72013b0d245c3ef5e18b65eba7f31f250838b24` additionally
binds complete first-party PE maps, Qt pre-sign SPDX to valid publisher signatures,
exact deployed inventory/source/notices and actual emitted hardening.

- AC1 pending: mechanical inventories pass for both candidates; independent
  final distribution trust and complete supported artifacts remain pending.
  The owner approved licensing/source/build/relink obligations only for the
  revised macOS licensing-v002 inventory at hand-back 470, as recorded below;
  revised Windows/Linux artifacts and their corresponding obligations remain open.
- AC2 pending across the matrix: tests-disabled macOS relocation, loader refusals,
  real MCP boundary, five hostile IPC frames and second-launch handoff pass.
  Windows extracted MSI maps/anchors and native inspectors pass. Actual Windows
  public runtime/vault/network course and declared Linux/Intel cases remain open.
- AC3 fails release policy: Grype 0.119.0/fresh v6.1.9 DB reports 21 macOS and
  19 Windows matches, including High PCRE2 and Windows zlib. No suppression or
  security-owner waiver exists. Focused tidy, 200,000 combined ASan/UBSan fuzz
  iterations and core/host/storage/public regressions pass. Exact secret findings
  are benign prose/identifiers/synthetic tests, with raw scanner failures retained.
  OS crash reports omit synthetic environment canaries; actual resident review/
  token crash material and full native vault/root/privacy qualification stay pending.

Neither authored exit is accepted. Required next actions are dependency
remediation or independently reviewed applicability, final authorized trust
verification, remaining artifact-specific licensing and the missing native
courses. The attended
licensing question is narrower than release acceptance. No tag, push, upload,
Developer ID signing or notarization was performed.

## M9 post-review source distribution correction

Fresh sole owner `/root/rfc0014_m9_postreview` resumes only M9 at
`6ee88ac8afdc30543c0c6ea42e2a596b7556c1c4`, mandate r3 unchanged. Session
`cbd48b91000b00f1e72e1ad62c7c384c` handed back at sequence 465. Rejection
`626562d7-65a5-4eb3-842b-30b2573a09e5` applies only to licensing v001,
fingerprint `2ec3475e1eed532ab1455783ae8553b823f6f7ffb005ee8b057ce1be5119c577`.
The requested correction moves corresponding source to a separate version-bound
release asset, retains exact references/notices and replacement instructions in
the application, and requires equivalent recipient access and retention. It
does not approve legal terms, security, trust or publication.

Commit `1c429749aff4c09c6dc88da930d3344cd2d33c36` updates the existing
macOS and native CPack drivers. Installed provenance binds the sidecar version,
platform, producer, exact URL and digest; notices and source offers name it.
Audits reject absent, substituted, stale or embedded archives. The existing
publication script verifies the actual app ZIP/SPDX/source relationship before
remote operations and requires eight assets including source and its checksum.
The access policy includes original-channel fallback, corporate/offline copies
and indefinite retention until all applicable obligations end. Qt replacement
and local resealing instructions are preserved.

Offline macOS preparation, all 34 tests in four focused CTest contracts, the
exact security audit, publication source preflight, checksum/ad-hoc seal checks,
and guarded tests-disabled public package course pass. All prior notices and
seven upstream source payloads are retained; installed footprint is 92,536 KiB
versus 159,472 KiB before the correction. Windows/MSI and Linux/DEB/RPM driver
contracts pass with fixtures; revised native artifacts remain pending.

The [ledger](../../verification/RFC-BPR-0014.md#m9-post-review-source-distribution-correction)
and handoff bind the new candidate and the separate source asset. Only licensing
v002 is prepared for the remaining owner/legal obligation. The old rejected
v001 and M8 candidates stay protected and unchanged. Grype's 21 Mac/19 Windows
failures and all trust/native/privacy obligations remain pending; this card and
both M9 exits remain unaccepted. No publication, remote release mutation,
Developer ID signing or notarization occurred. Next action: hand back the exact
licensing decision; the coordinator assigns a fresh M9 engineering round.

## M9 licensing-v002 decision finalization

Fresh sole implementer `/root/rfc0014_m9_finalize` resumed at clean
`63c786684ebb1c66b314341f6ee4b66a1d569225`, with mandate r3 and
coordinator-implementer rev2 unchanged. Session
`cbd48b91000b00f1e72e1ad62c7c384c` saved human accept
`42661017-3d13-43ba-8366-7ac3bd32c0db` at event 469 and handed back at 470.
It applies only to `rfc0014-m9-licensing-v002`, fingerprint
`da5616bda5bc5727e9eb8b8cce756357300a9d5102592ca34698ec48aa8dce80`,
producer `1c429749aff4c09c6dc88da930d3344cd2d33c36`.

The existing owner/legal criterion is satisfied for that exact macOS inventory:
the separate version-bound corresponding-source asset, installed exact
reference/digest and notices, equivalent recipient access, continued retention,
and preserved build/relink/replacement instructions are approved. The raw note
is empty; no additional supporting opinion or authority is inferred. This
decision is neither a vulnerability disposition nor security, independent trust,
native-matrix or release approval. The prospective source URL is still
unpublished; fulfilling recipient access and retention remains a distribution
obligation. No approval transfers to revised Windows/Linux artifacts.

The actual app ZIP/SPDX/source preflight and all reviewed artifact digests pass
again without changing candidate bytes. The
[finalization ledger](../../verification/RFC-BPR-0014.md#m9-licensing-v002-decision-finalization)
and handoff capture the exact decision and server retirement. Historical v001,
M8 and unrelated protections remain intact. BPR-0962 stays Blocked on
`release-security-inputs`; both M9 exits remain pending, including the unchanged
21 macOS / 19 Windows Grype failures and trust/native/privacy gaps. Independent
M10/BPR-0963 is next through accepted M7; this owner stops at M9.

## M9 dependency remediation resumption

Sole implementer `/root/rfc0014_m9_finalize` resumes AC3 engineering from clean
`08d129d0b04d2cac2649013b16b315cf7ccf929e`, mandate r3 and
coordinator-implementer rev2 unchanged. M10 and M11 are integrated; their
acceptance does not resolve M9. The scope is supported pinned dependency
remediation, exact offline package rebuild/rescan, and primary-source
applicability evidence for every existing finding. No suppression, security
waiver, native/trust acceptance or publication is authorized.

Execution plan: preserve all six attended protections and recorded decisions;
upgrade the official Qt provider/source pin together where a released fix is
available; exercise the package/source/public path and focused Qt-dependent
contracts; bind unfiltered scans and per-finding evidence to the new artifact;
then return this card to Blocked if independent decisions or unavailable
upstream fixes remain. SQLite is already at the latest published 3.53.4.
Historical packages and approvals remain immutable and do not transfer to
changed inventories. M10/M12 are out of scope.

The owner explicitly selected Qt 6.11.2 during this round. Its official source
pin is `5b2e00eccaf5a4d8c14134ffa0ea8dfd0a35ae1ffc7f8d87fa4305a1ed23cf22`.
Provider and packaging pins now agree, package input/SBOM selection follows the
declared version, and a regression prevents stale cached Qt discovery from mixing
old helpers with new modules. The first configure exposed that old-cache failure;
the corrected configure and complete development build pass. Four package/source/
security contracts and the documentation gate pass before candidate production.
Full relevant tests, offline package audits and fresh scans are still required.

### Qt 6.11.2 remediation result

The [exact remediation evidence](../../verification/rfc0014-m9-dependency-remediation.json)
binds macOS producer `92c4e1594b7a8836d69d79404d1d12a7ddf9bbcf` and native
Windows producer `67f5e51407a883e4469cee6008048a001f7c13b3`. Both exact
packages pass source/inventory/hardening mechanics; Windows also passes the
actual administratively extracted MSI audit with bound full PE maps. The
tests-disabled macOS Cocoa public course passes. Windows acquisition requires
the pinned upstream aqtinstall repository-layout fix; child-only PATH cleanup
and a shorter disposable binary directory resolved host-specific build failures.
All guarded live station files, including its dirty MASK lock, remain unchanged.

Fresh Grype 0.119.0 with valid v6.1.9 database built
`2026-09-21T06:39:24Z` returns exit 2 for **32 macOS** matches
(2 Critical, 8 High, 19 Medium, 3 Low) and **30 Windows** matches
(1 Critical, 11 High, 15 Medium, 3 Low). Qt's libpng 1.6.58 removes its old
finding; the new Tika MIME-XML attribution adds twelve matches per platform.
Every match has primary-source/component/call-path evidence for independent
review, including SQLite version-range mismatches, PCRE2 build/options and
zlib/DBus platform distinctions. These are proposed dispositions, not waivers.
Qt 6.11.2 still ships PCRE2 10.47 and Windows zlib 1.3.2; the relevant newer
upstream fixes are not contained in those official binary packages. No named
independent security-owner acceptance exists. Historical 21/19 failures remain.

The complete development run finishes 374/415 passing, with 41 raw failures
retained. Two release-owned verifier failures are corrected and pass focused
rechecks. Serial Qt verification is 75/84; all nine failed Qt cases reproduce
with Qt 6.10.3. Ten representative broader failures also reproduce there.
No full-suite pass or blanket baseline classification is claimed. Six release
contracts, the 27-test dependency contract, format, six-unit focused tidy and
documentation checks pass. Fresh redacted secret scans reproduce only the
previously adjudicated synthetic/prose/identifier findings without suppression.

New macOS licensing-v003 and Windows licensing-v002 packets preserve the
separate version-bound source asset, exact installed references/digests,
recipient-access/retention model and relinking instructions. Each needs its own
owner/legal decision; the prior v002 acceptance remains bound only to its old
macOS bytes. All six historical protections and decisions are preserved.
This card and both M9 exits remain Blocked on `release-security-inputs`:
independent vulnerability disposition/fixed producer, changed-inventory legal
decisions, final trust and full native/privacy/hardening evidence. M10/M11 are
already accepted; M12 has no eligibility while M8/M9 remain pending. No signing,
notarization, tag, push, upload or publication is authorized or performed.

## M9 licensing-v003 partial decision finalization

Sole implementer `/root/rfc0014_m9_finalize` resumes only decision finalization
at clean `b88c88b0654afea3bf44f83c6ddf8e0684a3b2b8`, mandate r3 and
coordinator-implementer rev2 unchanged. Session
`cbd48b91000b00f1e72e1ad62c7c384c` saved human accept
`a3a22f09-afe9-4386-8d2d-e784f1e0ee9e` at event 477 and handed back at 478.
It applies only to macOS `rfc0014-m9-licensing-v003`, fingerprint
`fd1f44666e40b376bcb426d8ddec6e6067cfcdfaa284d93b7b72e8c210d2236a`,
producer `92c4e1594b7a8836d69d79404d1d12a7ddf9bbcf`.

The exact owner/legal distribution and source/build/relink criterion is
accepted: separate version-bound source asset, installed reference/digest and
notices, equivalent recipient access, ongoing retention and replacement
instructions. The human note is empty; no supporting legal opinion is inferred.
The unpublished source URL and ongoing recipient obligations remain distinct
from an observed distribution. This is not security, vulnerability, independent
trust, native-matrix or release approval.

Windows `rfc0014-m9-windows-licensing-v002`, fingerprint
`e19c8c7713cd7b79f80ea5a21fe557690d71289677cbd6a61d07fdd749b00a74`,
producer `67f5e51407a883e4469cee6008048a001f7c13b3`, has **no decision**
and is explicitly pending at hand-back. No skipped or unanswered item is accepted
or rejected by inference. All eight protections and historical decisions remain.

All 13 locally available macOS/Windows inventory digests still match, and the
actual macOS ZIP/SPDX/source preflight passes without changing candidate bytes.
The [partial-decision ledger](../../verification/RFC-BPR-0014.md#m9-licensing-v003-partial-decision-finalization)
and handoff bind the decision and requested server retirement. BPR-0962 remains
Blocked on `release-security-inputs`; both M9 exits remain pending. The raw
Grype failures (32 macOS / 30 Windows), historical 21/19 failures and independent
trust/native/privacy/hardening gaps are unchanged. M10/M11 are accepted, but
there is no eligible successor; M12 still requires M8/M9 and canonical
predecessor acceptance. No signing, publication or remote release mutation.

## M2 security allocation

The [source allocation](../../architecture/release-readiness.md) binds all nine
canonical public checks and G1–G9. Verify exact final package/helper bytes,
static/transitive SBOM coverage, effective emitted hardening, loader policy,
vault/root behavior and native crash-artifact canaries independently of build/
sign success. Write and apply an explicit tool/database/findings release policy
for static analysis, sanitizer/fuzzer, secrets and vulnerabilities.

G9 requires explicit reconciliation: the existing profile-activation adapter
uses an owner-authenticated bounded AF_UNIX listener, while the old RFC runtime
criterion says no local server. M2 did not waive or rewrite that criterion.
Inspect owner/peer/frame/lifetime/no-network-service properties; a TCP-only scan
cannot omit this IPC. Preserve any required acceptance/mandate decision rather
than claiming the current implementation already passes. Existing updater/live/
Intel gates remain separate; publication is not authorized.

Owner amendment r2 directs reuse of the existing release script's macOS
signature/notarization/Gatekeeper and dependency checks, followed by independent
exact-artifact qualification. The project `publish-release` skill preserves a
separate explicit user request before tag/push/release creation. Neither its
existence nor BPR-0908's skipped-notarization run satisfies this card; retain
the full supported matrix and SBOM/license/privacy/hardening obligations.

## M9 Windows licensing-v002 decision finalization (main, 2026-09-24)

The carried-forward Windows question was answered in attended session
`cbd48b91000b00f1e72e1ad62c7c384c`. It was re-presented unchanged in packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md)
beside that day's other asks. The session was handed back at sequence 589.
The owner saved a human **accept**, decision
`afa8af40-56ea-4c19-aaed-4f48ebde1218`, at event 588 (2026-09-24T19:17:39Z),
with an empty note. It applies only to `rfc0014-m9-windows-licensing-v002`:

- fingerprint `e19c8c7713cd7b79f80ea5a21fe557690d71289677cbd6a61d07fdd749b00a74`;
- revision `producer-67f5e514-win-b34099e6-source-11babc5f`;
- producer `67f5e51407a883e4469cee6008048a001f7c13b3`;
- manifest [rfc0014-m9-windows-licensing-v002](../../verification/reviews/rfc0014-m9-windows-licensing-v002/manifest.json).

**Accepted.** The owner/legal criterion is accepted for this exact Qt 6.11.2
Windows inventory only. The question asked whether the distribution terms and
the corresponding-source, build and relink obligations were approved,
including equivalent recipient access and retention. That covers:

- the separate version-bound corresponding-source asset;
- the installed reference, digest and notices;
- equivalent recipient access, including offline and corporate copies and a
  fallback;
- indefinite retention while matching binaries are distributed;
- the build, Qt replacement and relinking instructions in `SOURCE-OFFERS.txt`.

The empty note supplies no further legal opinion or authority.

**The bytes are unchanged.** The protected payload
`build/releases/m9-qt6112-windows-67f5e514` still matches the reviewed digests
on this Mac:

- MSI `0174d9ce46537601ffca81295eed3bcea31c7fc5666e35613b8e4d6810178ef6`;
- SPDX `b34099e6a7e957dc64916a649c2c036ce16ad26dcf0e1c9507b05a7fa979494b`;
- corresponding source `11babc5ff5cc7865344835685223d4757bddfa433a4517d56fe9975f39436b3b`.

The candidate's protection is kept, as for the earlier licensing decisions.

**Not approved by this decision:** vulnerabilities, security, signatures,
native behaviour and publication. The prospective v0.1.3 source URL is still
unpublished. Recipient access and retention stay distribution obligations,
distinct from any observed distribution.

With the macOS licensing-v003 accept, both current inventories now have their
owner/legal decision, and AC1's licensing half is decided for these exact
bytes. AC1's independent signature verification and the other criteria are
not. **The card stays Blocked on `release-security-inputs`**. These are still
open:

- the raw Grype failures (32 macOS / 30 Windows) and their security-owner
  disposition;
- independent trust verification;
- native, privacy and hardening evidence.

No signing, notarization, tag, push, upload or publication was performed or
authorized. The [successor ledger](../../verification/RFC-BPR-0038.md#human-review-and-protections)
records the same decision.

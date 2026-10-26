# RFC-BPR-0038 qualification ledger

## v0.3.4 publication and permanent Debug timing policy, 2026-10-26

[Release results](release-v034-results.md) record stable Latest v0.3.4 at
`be4fe79d8abbc8f438b25bbb76bbd16ab6d0a795`, all 28 anonymous downloaded
assets matching producer bytes and fresh native trust/source audits on all
three selected platforms. The results retain actual failures, recoveries,
configured skips, eight Ubuntu Debug performance omissions, the four-file
licensing/test source bridge, raw vulnerability failures and remaining gaps.
Native Intel Mac qualification was explicitly omitted by the owner.

The owner made Debug performance timings permanently nongating; the
[standing policy](../development/release-test-profiles.md#standing-debug-performance-policy)
and [decision](../issues/_checkpoints/RFC-BPR-0038/owner-debug-performance-policy-20261026.json)
have no per-release expiry. This changes timing acceptance only. RFC-BPR-0038,
its native installation/vault/crash/privacy and vulnerability gaps, and existing
blocked cards remain open. No historical FAIL is relabeled as PASS.


## Owner matrix confirmation, 2026-10-23

[BPR-1743](../issues/60-done/BPR-1743-confirm-or-withdraw-the-october-16-qualification-matrix-amendment.md)
asked whether the 2026-10-16 matrix amendment stands. The owner answered on
2026-10-23 UTC: "Windows 11 and ubuntu wsl2 are tested. If it works on other
things is unproven, but not disallowed." The
[confirmation](../issues/_checkpoints/RFC-BPR-0038/owner-matrix-confirmation-20261023.json)
sits beside the unchanged
[2026-10-16 record](../issues/_checkpoints/RFC-BPR-0038/owner-matrix-amendment-20261016.json).

The tested Windows and Linux environments are Windows 11 x64 and Ubuntu 26.04
x86_64 under WSL2/WSLg, both on JaegAW. macOS is unchanged. Windows 10, other
distributions and native Linux desktops are unproven but not disallowed:
nothing refuses them and nothing claims them. The RFC matrix, BPR-0960,
BPR-0961, BPR-0962, BPR-0973, the package guide and its maintainer reference,
the corporate security overview and README now say so. The RFC-0014 plan
records it as dated
[amendment r5](../rfcs/plans/rfc-bpr-0014-release-readiness/amendments/r5/amendment.json)
with its M8 exit text unchanged. Shipped release notes keep their wording.

This is wording and scope only. No environment is newly qualified. The M8/M9
native, vault, crash-privacy and vulnerability gaps stay open, BPR-0958 to
BPR-0962 stay Blocked, and all nine criteria remain open.

## Security-audit port and current-release scan recheck, 2026-10-22

Under the owner's 2026-10-22 direction to finish the unmerged work, a Claude
rfc-implementer ported the RFC-0038 security-audit branch
`codex/rfc0038-security-audit-20261016` (tip `b110288eb`). That branch already
contains every commit of `codex/rfc0038-qualification` (tip `5271d9c9b`). The
port grants no release trust, security-owner or legal approval, and it leaves
the M8/M9 native, vault and crash gaps open.

**Ported as current:**

- The scan verifier,
  [`verify_release_vulnerability_scan.py`](../../scripts/verify_release_vulnerability_scan.py),
  and its controls, registered as `release_vulnerability_scan_contract`. Six
  more controls were added: a narrower threshold, only-fixed, VEX, a foreign
  SPDX, a foreign revision or platform, and count drift.
- The [scan-binding course and row-review method](../security/release-policy.md#binding-a-scan-to-its-artifact).
- The [owner's Qt known-issues decision](../security/release-policy.md#owner-decision-on-known-upstream-qt-issues-2026-10-16),
  with its recorded Qt 6.11.2 scope.

**Kept byte-identical as historical 0.2.x evidence:** these files apply only to
Mac 0.2.1 (`c233c768`), Windows 0.2.2 (`23447f3a`) and Qt 6.11.2. The
follow-up record binds them by hash.

- [Dispositions](RFC-BPR-0038-vulnerability-dispositions-20261016.md).
- The [Mac 0.2.1](RFC-BPR-0038-macos-0.2.1-vulnerability-scan-20261016.json)
  and [Windows 0.2.2](RFC-BPR-0038-windows-0.2.2-vulnerability-scan-20261016.json)
  scan summaries.
- The [review packet](RFC-BPR-0038-independent-security-review-packet-20261016.md)
  and its [64-row queue](RFC-BPR-0038-independent-security-review-queue-20261016.json).
- The [independent review v1](RFC-BPR-0038-independent-security-review-20261016-v1.md)
  and its [record](RFC-BPR-0038-independent-security-review-20261016-v1.json):
  53 accept, 8 insufficient evidence, 3 remediate.
- The [remediation follow-up](RFC-BPR-0038-security-remediation-followup-20261016-v1.md)
  and its [record](RFC-BPR-0038-security-remediation-followup-20261016-v1.json).
- The [evidence reconciliation](RFC-BPR-0038-evidence-reconciliation-20261016.json).
- The [prerequisites](RFC-BPR-0038-prerequisites-20261016.md).
- Three checkpoints under `docs/issues/_checkpoints/RFC-BPR-0038/`. Two are
  owner decision records, one for the
  [Qt known issues](../issues/_checkpoints/RFC-BPR-0038/owner-qt-known-issues-20261016.json)
  and one for the [matrix](../issues/_checkpoints/RFC-BPR-0038/owner-matrix-amendment-20261016.json).
  The third is the [continuation](../issues/_checkpoints/RFC-BPR-0038/continuation.json).

None of these decisions transfers to 0.3.x bytes.

**Not applied:** the branch's edits to the RFC matrix, the RFC-0014 milestone
plan, BPR-0959, BPR-0960, BPR-0961 and BPR-0973, the package guide and the
security overview. They carried the 2026-10-16 owner matrix amendment, which
removed Windows 10 qualification and limited Linux claims to WSL2/WSLg. Main
never adopted that amendment. Main's RFC and README still say Windows 10/11.
BPR-1743 asks the owner to confirm or withdraw it.

**Dropped:** the branch's wizard definitions, milestone bindings, strategy
selection and Codex session state. They were superseded by the 0.3.x release
work.

### Current-release recheck

The rerun used Grype 0.120.1 offline (`GRYPE_DB_AUTO_UPDATE=false`) with the
local database: valid schema v6.1.10, built `2026-10-21T06:31:02Z`, about 41
hours before the scans on 2026-10-22. The verifier ran against each exact
artifact. Package and SPDX digests were checked against the published GitHub
asset digests first.

| Artifact (source) | Package SHA-256 | Matches / advisories | Verifier |
|---|---|---|---|
| [Mac 0.3.1 ZIP](RFC-BPR-0038-macos-0.3.1-vulnerability-scan-20261022.json) (`2ed7e5f9`) | `07de4909…3ffc` | 21 / 21: 2 Critical, 4 High, 14 Medium, 1 Low | exit 2 |
| [Windows 0.3.1 MSI](RFC-BPR-0038-windows-0.3.1-vulnerability-scan-20261022.json) (`2ed7e5f9`) | `ec3c3819…5064` | 19 / 17: 1 Critical, 7 High, 10 Medium, 1 Low | exit 2 |
| [Ubuntu 0.3.1 DEB](RFC-BPR-0038-ubuntu-0.3.1-vulnerability-scan-20261022.json) (`2ed7e5f9`) | `c302a973…2a2b` | 21 / 21: 2 Critical, 5 High, 13 Medium, 1 Low | exit 2 |
| [Mac 0.3.2 ZIP](RFC-BPR-0038-macos-0.3.2-vulnerability-scan-20261022.json) (`20ee6d72`, prerelease) | `d948d9de…a936` | 21 / 21, as Mac 0.3.1 | exit 2 |

Each 0.3.1 row set matches the reviewed
`.mask/sessions/release-v0.3.1/qt612-release031/security/grype-*-final-r2`
reports exactly: same package, version, advisory and severity, on the earlier
`2026-10-19` database. Mac 0.3.2 has the same 21 rows as Mac 0.3.1. The raw
reports are kept outside the repository in
`.mask/sessions/rfc0038-security-port-20261022/`. The summaries record their
SHA-256 values.

### Which 0.2.x conclusions still describe current rows

The identical package, version and advisory rows below recur in 0.3.x. The
0.2.x reasoning still describes them, as a proposal. The binding decisions for
0.3.1 are the release's own independent factual dispositions, which expire
2026-11-19.

- Tika MIME XML (`c2972478…`, 12 advisories, all three platforms): an XML
  resource, not Java Tika handlers.
- SQLite 3.53.4 (three Bitnami rows): the version is beyond each upstream fix.
- `GHSA-g857-hhfv-j68w` on `zlib` 1.3.2: concerns the Ruby gem, not native
  zlib.
- D-Bus header attribution 1.13.12 (Mac, Ubuntu): headers only. A libdbus
  loaded at run time is not qualified.
- Apple system zlib (Mac): the 0.2.x follow-up showed that `crc32_combine` and
  `crc32_combine_gen` without the `64` suffix reach the negative-length path
  on macOS 26.6.2. So the absence of the `*64` exports proves nothing. The
  current disposition rests on caller absence instead.
- Windows bundled zlib `CVE-2026-85091` (three High rows): still unpatched.
  The `gzwrite.c` in the pinned Qt 6.12.0 archive (`a951bd16…`) hashes
  `2ce78329…5b04`, byte-identical to Qt 6.11.2. 0.3.1 relies on a
  closed-caller disposition only.

Historical only:

- The PCRE2 10.47 rows (`CVE-2026-89156` to `CVE-2026-89162`). Qt 6.12.0
  bundles PCRE2 10.48, and the feed no longer matches them.
- The partial-JIT probe, which used 10.47 sources.

Still open:

- The outside-feed PCRE2 `GHSA-r9hj-j2rw-4q3m` / `CVE-2026-103111`, fixed in
  10.49. The Qt 6.12.0 attribution says 10.48, and its JIT stack-growth stub
  is unchanged. On Qt 6.11.2, v0.3.0's independent `dd6` adoption disposed
  of it as "reviewed product attacker-pattern exclusion, no arbitrary
  extension immunity". No record of v0.3.1, v0.3.2 or this repository on
  this Mac carries that to Qt 6.12.0. The v0.3.1 reviews covered only the
  raw rows, and the PCRE2 rows had gone.
  BPR-1742 carries it.

### What still blocks BPR-0962

The [card](../issues/50-blocked/BPR-0962-verify-release-trust-and-security-boundaries.md#security-audit-port-and-what-still-blocks-2026-10-22)
lists the blockers. Every current scan exits 2. No accountable named security
owner has accepted any disposition. Windows zlib and `GHSA-r9hj` remain. The
native, vault, crash, privacy, IPC and hardening courses have not been run.
Two owner decisions are pending.

## Public stable 0.3.1 published, 2026-10-20

[Stable v0.3.1](https://github.com/MikeMarcin/BetterPullReview/releases/tag/v0.3.1)
is public and Latest, with 28 assets, no draft/prerelease flag and annotated tag
`728e6e75ad4df34a45b7801551272106bde456d5` peeled to
`2ed7e5f98641d75678f861dad0edbe658b5f5d22`. The
[publication record](release-v031-publication.json) binds every final asset,
normal gate, material decision and post-download native check. The
[engineering record](release-v031-engineering.md#published-coordinated-031-2026-10-20)
retains failures, timings and the exact owner exception.

All three full native normal suites completed: 2,389 passes and three configured
Debug cache-course skips across 2,392 rows, with no missing/unexpected rows.
The registered optimized Release course passes on macOS and Windows. Ubuntu's
strict course remains FAIL, exit 8, at its unchanged 216 MiB RSS-growth bound.
The owner extended only the existing sampled/lifetime RSS exception; the
supplemental lifecycle diagnostic remains an always-failing observation,
and BPR-1653 remains open for causal repair after publication.

Every asset was downloaded anonymously and matched its final producer SHA-256.
Fresh downloaded-byte checks pass for macOS Developer ID/notarization/Gatekeeper
and DMG/ZIP equivalence; Windows MSI/four first-party images plus 11 Qt DLLs,
with fresh independent five-file native verification and exact 29-file extracted
MSI/runtime equality; and Ubuntu's six signatures, public-only key, tamper
rejection, checksums and source binding. All three exact source assets have the
same public access as binaries at no additional charge. Source, first-party
snapshot, digests and the annotated tag are retained read-only indefinitely;
recipient access, original-channel fallback and offline redistribution remain
ongoing obligations.

The owner accepted each platform's exact Qt6.12 distribution material in two
short attended sessions. Independent factual reviews cover all 21/19/21 raw
Grype matches; each scan remains exit 2, with 52/44/61 flagged coverage rows.
Expiry is 2026-11-19 or an earlier named input/advisory change. Windows' affected
zlib gzip-writer source remains present and unpatched; only the current closed
caller composition supports its bounded nonapplicability disposition.
No applicable-risk or unknown-coverage waiver is inferred.

RFC0038, BPR-0958–0962 and BPR-0973 remain open for the broader native/vault,
Intel/desktop/accessibility, upgrade/removal, crash/privacy, external-hardening
and complete vulnerability/fuzz qualifications. Publication does not close
those criteria. The prior source/tag/assets and historical evidence below
remain unchanged.

## Public repository access, 2026-10-19

The owner-authorized [public cutover](../security/publication-cutover-2026-10-19.md)
made the repository and retained releases public, enabled private vulnerability
reporting, and kept GitHub Actions disabled. Anonymous full downloads of all
99 retained assets, including every matching source archive, reproduce their
audited sizes and SHA-256 values. No artifact or tag changed. This adds public
access evidence; the exact native qualifications, dispositions and open gaps
below remain unchanged.

## Coordinated private stable 0.3.0 published, 2026-10-19

[Private stable v0.3.0](https://github.com/MikeMarcin/BetterPullReview/releases/tag/v0.3.0)
is Latest with 28 assets at immutable source/tag
`dd6d3cec6026f32dd50428238bbfe9aaa80926fe`. The
[release engineering ledger](release-v030-engineering.md#published-coordinated-030--final-dd6-acceptance-2026-10-19)
records exact artifact/download/native checks, source fulfillment and separate
audit-tool integration. BPR-1654–1656 final package/source-material criteria
are reconciled there; the historical failed artifacts remain unchanged.

Independent factual applicability decisions retain all 97 raw findings
(33/31/33) and coverage flags (44/36/56); expiry is 2026-11-18 or a named
input/advisory/feed change. No applicable-risk or unknown-coverage waiver is
invented. Current Ubuntu material has its own human acceptance; Mac/Windows
reuse only independently verified unchanged approved material. Equivalent
no-extra-charge source access and immutable indefinite retention remain ongoing.

The owner accepts only BPR-1653's original failed Ubuntu 216 MiB RSS predicate
as a v0.3.0 exception, with causal correction and an unchanged-course pass due
in a follow-up patch. The 64 MiB cache bound passes; BPR-1653 stays In Progress.
No full suite is rerun: 1963 execution-input hashes match dd6. Four authorized
macOS focused native courses pass, without claiming a fresh complete normal run.

This publication does not close RFC-BPR-0038 or its broader M8/M9 clean-account,
authentication/vault, Intel, Windows/Ubuntu desktop, accessibility, upgrade/
removal, resident-material crash/privacy, external-hardening or complete
vulnerability/fuzz qualification. BPR-0958–0962 and BPR-0973 remain open.
The historical sections below retain their original evidence and limits.

## Coordinated stable 0.3.0 source preparation, 2026-10-18

The owner selected full macOS, Windows x64 and Ubuntu 26.04 packages at stable
0.3.0. [BPR-1623](../issues/60-done/BPR-1623-prepare-coordinated-v030-release-source.md)
and the [engineering ledger](release-v030-engineering.md) bind source
preparation to entry `a426db419d393895020ab961509df46aba94ee46` under mandate
`release-v030-coordinated-r1`. The coordinator owns exact-source native normal
gates, signing, packaging and publication; preparation supplies no new native
or security qualification. Historical approvals and configured skips retain
their exact original scope. BPR-0958–0962 and BPR-0973 remain open, including
native/vault, crash/privacy, vulnerability and distribution obligations below.

## Unversioned Ubuntu readiness refresh, 2026-10-16

The owner selected readiness for the next coordinated release, with no version
selected. The [producer record](ubuntu-release-readiness-20261016.md) binds
Ubuntu 26.04 x86_64 WSL2 source `aa6294c50f95b51d95f7c7861d7430f89cdb2fdc`,
available native tools/Qt SPDX, matching public-key exports and successful
scoped GPG configuration validation without credential retrieval or signing.
The focused packaging refresh exposed a missing xdiff shared target in the tiny
CPack fixture; BPR-1561 restores it with existing assertions unchanged. Native
verification of the repair plus unchanged passing contracts gives 66 passing
cases and one explicit Windows-only skip. No full normal gate or final package
was produced. Clean desktop/Secret Service, exact final-artifact trust,
runtime/security and distribution obligations on BPR-0961/0962 remain open;
existing released bytes are unchanged.

## 0.2.0 macOS release: xdiff LGPL arrangement approved, 2026-10-13

The owner selected 0.2.0 (not 0.1.10, which was never released) for a
macOS-only private prerelease built from the release-source commit that
carries this entry, and approved the LGPL arrangement for Git's xdiff
([BPR-1485](../issues/60-done/BPR-1485-adopt-linked-xdiff-as-the-primary-line-matcher.md))
for this release, after being shown each element:

- xdiff ships as its own LGPL-2.1 shared library, `libbpr_xdiff.dylib` in
  `Contents/Frameworks`, dynamically linked as Qt is;
- its notice is in `THIRD-PARTY-NOTICES.txt`;
- `SOURCE-OFFERS.txt` states that it is a separately replaceable shared
  library;
- the corresponding-source asset carries the unmodified Git 2.56.0 archive
  plus BetterPullReview's build script and its two build headers.

This clears the pending xdiff clause in `docs/development/release-packages.md`
and `scripts/verify_release_security.py`. It is not approval of the remaining
first-party terms, mixed syntax-data licences, toolchain terms or other
build/relink obligations for these bytes, and it is not trust, vulnerability
or publication approval. The clean-account vault, native Intel/platform,
crash privacy and vulnerability gaps below remain open; Windows and Ubuntu are
not part of this release.

## Follow-up release gates, 2026-10-12

The direct owner mandate `release-v010-20261012-fix-and-publish-r4` says
"fix all the issues and go publish 0.1.10 prerelease afterwards" and selects
full macOS, Windows and Ubuntu 26.04. A fresh RFC implementer owns
[BPR-1404](../issues/30-in-progress/BPR-1404-verify-followup-release-gates.md),
with published v0.1.9 candidate
`6cc7a99e8754ffe482a90dc012d5afe98b3a9da8` immutable. The version bump and
publication are authorized after the known release issues are repaired and
verified; v0.1.9's timing exception does not apply to v0.1.10.

The repaired source `f01b79c5a1568a4f25353f34cf61dcdd6627201e` passes
all 14 affected uninstrumented Mac courses, including load gaps 91.358042 ms
and 91.199833 ms against the unchanged 100 ms requirement. Windows subsequently
built natively; its SSH logon-context failures and local-token focused passes
remain separate from the required full gate. Ubuntu's complete baseline is
661 pass, 11 fail and one configured skip. The known font/scrollbar and accepted
appearance-contract repairs await final native checks; deleted-reply discard
remains under diagnosis. [The exact gate ledger](BPR-1404-v010-release-gates.md)
records the generator repair, local ENOSPC failure and current security proposals.

The coordinator serializes Windows and WSL courses on their shared physical
host and owns packaging, signing and publication. Every wider RFC0038 gap
below remains explicit, including independent security-owner decisions;
publication authority does not supply missing acceptance evidence.

## v0.1.9 Mac release amendment, 2026-10-12

The owner narrows v0.1.9 to the current macOS build and explicitly accepts
its timing misses for this release: "timing misses be damned". Windows and
Ubuntu 26.04 packages and remaining repairs move to an unversioned follow-up
patch on [BPR-1404](../issues/30-in-progress/BPR-1404-verify-followup-release-gates.md).
[BPR-1403's gate ledger](BPR-1403-v019-release-gates.md) preserves the original
failed normal gate, exact fixture repairs, focused checks, configured Debug
cache-bytes skip and failed 100 ms PR-load observations. This owner release
exception does not turn failed timings into passing evidence or close any
native, vault, trust/licensing, crash/privacy or vulnerability qualification.
The coordinator owns the separate exact-source Mac signing/notarization and
publication path; RFC0038 and its wider blocked follow-ups remain open.

## Next-release Linux scope amendment, 2026-10-11

The owner selects Ubuntu 26.04 x86_64 DEB and per-user archive for the next
release, removing Ubuntu 24.04 and RHEL from the selected set. WSL2 is admitted
for ordinary binary production. The later instruction requests reusable future
readiness, probably with a Mac publisher and the signing PC's Windows/Ubuntu
producers, with no actual release now. [BPR-1400](../issues/60-done/BPR-1400-prepare-ubuntu-2604-release-route.md)
owns bounded preparation under `release-next-20261011-ubuntu2604-r2`.
This is an owner baseline/scope amendment, not evidence that any package,
clean desktop/vault, trust or security course passed. Current Ubuntu ABI ceiling
is glibc 2.43; actual GLIBCXX/system dependencies still require artifact evidence.
Optional RPM mechanics stay outside the selected set. The historical entries,
M8/M9 plans and review packets below remain unchanged. BPR-0961 and BPR-0962
stay Blocked; RFC0038 remains Proposed. The subsequent producer signing
prerequisites have separate evidence below; exact-candidate
native/licensing/trust/security/crash obligations and eventual publication
authority remain pending. No release version/build/publication is selected.

Preparation evidence from BPR-1400, producer-scripts candidate
`6b06c981419e15215fa9ffcd97de7585c611ea13`: the actual Ubuntu 26.04 WSL2
baseline CLI succeeds, and 42/42 focused runtime/native-manifest/source-binding
contracts pass in an isolated exact-commit snapshot with no skips. A tiny
native CMake configure includes the production install definition and verifies
DEB default and the new target; it builds no application. Windows checked cases
pass with eight explicit native/POSIX/Bash skips, all exercised successfully in
WSL. Documentation gate and 67 documentation tests pass. Initial bare WSL PATH
lacked CMake; the configured login shell resolves CMake 4.4.2, GCC 15.2 and
glibc 2.43, and the same candidate passes. Temporary source/test roots are
removed. The ignored run checkpoint retains actual commands and the failure.
These are script/fixture and configure observations, not produced release
binaries, WSLg rendering, clean desktop/vault/installer qualification, a new
vulnerability scan, signing or publication approval. BPR-1400's Done status
accepts only the bounded preparation slice; all wider release gaps above remain.

## Reusable signing preparation, 2026-10-11

Owner amendment `release-next-20261011-vault-verifier-r3`,
[BPR-1402](../issues/60-done/BPR-1402-reuse-release-signing-vault-and-native-verifier.md),
asks for reusable passphrase storage instead of entering it for each release.
Entry/baseline `2b56a8c30f3fbde66a3469dd0778121401ad9e0a`.
Implemented and tested producer candidate
`277c58f26` (full revision in the ignored candidate-check receipt).
One named generic Windows credential (local-machine persistence for the same
Windows account) is bound to the approved full fingerprint/keygrip and an
owned, private WSL release GPG home. No vault enumeration or secret-output
command exists. The helper validates before retrieval, sends the in-memory
passphrase only to the fixed preset command's stdin and checks unlock with
pinentry disabled. Wrong passphrases clear the cache; missing credentials or
wrong bindings fail without interactive fallback. The key remains encrypted;
the scoped agent retains its 600/7200-second cache bounds and external-cache
refusal. The owner completed direct enrollment through the coordinator's fresh
wizard. Two separate fresh-agent courses then exercised maintained WSL-to-Windows
unlock and pinentry-disabled signing with the approved key; public-only
verification passed and tampered content was rejected in both. The public
receipt is `.mask/sessions/release-next-20261011/gpg-vault-real-receipt.json`.
The initial visible-terminal attempts failed before secret entry because the
public config was absent in that native process's filesystem view. Materializing
the unchanged public binding in the actual producer context resolved it;
this failure remains in the coordinator's ignored checkpoint. Agent observation
of a file alone does not prove the producer can read it.

Focused local evidence: 27/28 native-release/vault tests pass on Windows with
one explicit Bash publisher skip; Ubuntu WSL passes the complementary Bash
case and skips only the actual Windows module-pollution row. The Windows row
proves a poison module actually loads under normal autoload, then that the
maintained verifier loads its own native manifest and never executes it.
Composed package fixtures prove failed unlock precedes output/build and every
detached signature uses the scoped home/pinentry-error path. The opt-in native
course creates its own disposable encrypted Ed25519 key and one synthetic
credential, proving Unicode round-trip, cold-agent unlock, Windows/WSL interop,
missing/wrong credential/passphrase refusal, nonprivate-home and wrong metadata
refusal, cleared
failure cache, public-only verification and tamper rejection. It removes the
exact synthetic credential, private GPG test root and its own agents. It reads
no approved credential and signs no release.

Separate coordinator prerequisite observations: the approved Linux Ed25519
key UID is `BetterPullReview Release Signing`, expiry 2028-10-11, fingerprint
`ED85BAA84A3B6D83FCD5B1F107CEC96F2688E4A8`; its prior local sign/public-only
verify/tamper smoke passed. The dedicated Windows Azure profile signer login
and profile-level role were observed. Exactly one approved disposable probe
was Azure-signed/timestamped for `Michael Marcin`; the existing final probe
SHA-256 `e1d9353dfcc085226813f63b6c595ad2a3bf817359bc83bea239f12439f0753a`
now passes the maintained SignTool and native Authenticode checks, signer
SHA-1 `9AE4E8F2FE30AB93CCF6BDC3378789FC2883F298` and timestamp signer
SHA-1 `BDA7BAB5B5B180CF0768C06C0F11C49ACC9690F5`. It was never re-signed.
No BPR release artifact has been signed by this preparation. Azure user login
may expire or require MFA; permanent unattended Azure credentials and remote
station logon access have not been established. Current skill/operator guides
reuse nonsecret producer configuration and keep credentials on the signing PC.

The ignored durable return is
`.mask/sessions/release-next-20261011/vault-handoff.json`. These observations
do not close RFC0038, BPR0958–0962 or BPR0973, replace independent final trust,
or erase native desktop/product-vault, privacy, licensing/source access,
security/vulnerability and future publication obligations. Historical M8/M9
records below retain their exact original candidates and failures.

## Scope and immutable evidence

Owner amendment r4, assignment `/root/rfc0014_owner_closeout`, moves unfinished
release qualification from RFC0014 into this successor. Entry/baseline:
`4f2c3bdb5855ba0fb509889cdbe08d7cf534acab`. No native, security, licensing or
trust observation is newly supplied by that transfer. The
[original evidence ledger](RFC-BPR-0014.md) and
[M8](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m8.json) /
[M9](../rfcs/plans/rfc-bpr-0014-release-readiness/handoff-m9.json) handoffs retain
all historical commands, raw failures, environment limits and exact decisions.
The shared plan's M8/M9 definitions/exits remain unchanged; neither is accepted.
BPR0958–0962 stay Blocked; BPR0973 is the successor closeout.

## Exact latest Qt 6.11.2 candidate set

Version 0.1.3. These are retained earlier producers, **not** packages built from
the r4 documentation-only closeout revision. Qt 6.11.2 remains selected in the
current source. Do not transfer prior approvals to changed bytes.

| Platform / asset | Producing revision | SHA-256 |
|---|---|---|
| macOS universal local ZIP | `92c4e1594b7a8836d69d79404d1d12a7ddf9bbcf` | `65cbeb97be3252c2b7e4346fe8033842671030c660744af41943eeecdffeb22c` |
| macOS SPDX | same | `b0b94e7fc1deabf16d0fe59af8ff5d19732cab859fa5887e3477f1258dd09b2c` |
| macOS separate corresponding-source ZIP | same | `2532ba6aaa2137deb709f201faa453214425e52cc3bec5389cbe898210663329` |
| macOS executable | same | `8ffd54bbd15993c9b7c11871410ff545f43c5831eaaf2bbdc4d022dc625586ab` |
| Windows x64 MSI | `67f5e51407a883e4469cee6008048a001f7c13b3` | `0174d9ce46537601ffca81295eed3bcea31c7fc5666e35613b8e4d6810178ef6` |
| Windows SPDX | same | `b34099e6a7e957dc64916a649c2c036ce16ad26dcf0e1c9507b05a7fa979494b` |
| Windows separate corresponding-source ZIP | same | `11babc5ff5cc7865344835685223d4757bddfa433a4517d56fe9975f39436b3b` |
| Windows executable | same | `068434f05f4231ba4f752b5958a26cb2dec6972b8e20b15a74073e59eab19c98` |

Retained local directories:
`build/releases/m9-qt6112-92c4e159` and
`build/releases/m9-qt6112-windows-67f5e514`.
The exact Windows native producer/expanded MSI remains at
`C:/Users/mikem/bpr0014-m9-qt6112-92c4e159`; no new remote inspection is claimed.
Mac inventory covers 42 files, 13 binaries, 63 Qt package records, seven source
archives and 228 hardened compile units. Windows covers 22 files, 16 binaries,
58 Qt package records, six source archives and 223 hardened units. Bound complete
PE maps/anchors and upstream Qt publisher verification passed. Mac seal is
ad-hoc only; MSI is NotSigned. None establishes final distribution trust.

The [curated dependency-remediation record](rfc0014-m9-dependency-remediation.json)
binds raw reports and proposed per-match applicability evidence; its SHA-256 is
`30ca502097e2eb6e0d7dda1da8076306e0cabbb7ce30fd6b04314b429caf5bce`.
Raw Grype 0.119.0, v6.1.9 DB built `2026-09-21T06:39:24Z`, exits 2:
Mac 32 (2 Critical / 8 High / 19 Medium / 3 Low), Windows 30
(1 Critical / 11 High / 15 Medium / 3 Low). Historical 21/19 failures remain
history, not the current inventory. Qt includes PCRE2 10.47 and Windows zlib
1.3.2; new Tika MIME-data attribution contributes twelve matches per platform.
Proposed source/version/call-path distinctions are **not** independent disposition.
No suppression or security waiver exists.

## Original requirement transfer and remaining work

| Original RFC0014 criterion | Accountable follow-up and current limit |
|---|---|
| 1 Clean native install/launch/auth/fixture/uninstall | BPR0958–0961; complete target matrix remains pending, including clean accounts and ordinary-user desktops. |
| 2 Independently verified trust | BPR0962; Developer ID/notarization/Gatekeeper, Authenticode and declared Linux package signatures remain pending. |
| 3 Runtime dependencies/process/network/IPC | BPR0962; retain Mac tests-disabled relocation, loader refusals, real MCP, hostile IPC and actual Windows/WSL inspector probes; full active-operation network/vault, foreign-user IPC, Windows/Linux/Intel runtime evidence remains pending. |
| 4 SBOM/notices/source/checksums | BPR0962; exact Mac/Windows mechanics passed. Windows v002 licensing is unanswered, Linux licensing unqualified, source URLs unpublished. Equivalent no-extra-charge recipient access and retention must actually be fulfilled. |
| 5 Release-policy hardening/analysis/fuzz/scans | BPR0962; 32/30 raw vulnerability failures, external dependency hardening, complete PE CFG, full sanitizer/fuzz coverage and broad test failures remain. No scan pass inferred from evidence collection. |
| 6 Vault/files/cache/retention/purge/uninstall | BPR0958–0961/BPR0962; implemented bounded controls and disposable fixture checks do not prove real-vault continuity/deletion or complete native residual courses. Shared updater/registry state deliberately survives profile purge; purge is not uninstall. |
| 7 Diagnostics/crashes | BPR0962; metadata-only preview/export and environment-canary checks passed on recorded surfaces; resident source/comment/token crash and complete native privacy qualification remains missing. |
| 8 Corporate packet | BPR0962/BPR0973; M10 and r4 document actual implementation/gaps; reconcile exact final qualified artifacts and operator claims before successor closeout. |
| 9 Packaged architecture | BPR0962; bounded core/source and exact available package evidence retained; full final delivered/runtime matrix remains unqualified. |

Native details: BPR0958 needs a disposable clean Apple Silicon account with a
working default Keychain and authorized authentication, complete revised-package
rendered course, full purge and residual inspection. BPR0959 needs native Intel.
BPR0960 needs a clean JaegAW Windows 11 non-elevated desktop/auth/high-DPI/
accessibility/Credential Manager/purge course; full PE mechanics are no longer
missing, but do not satisfy those requirements. BPR0961 needs the Ubuntu 26.04
WSL2/WSLg ordinary-user DEB/per-user archive course with Secret Service, rendered
interaction, purge and removal (owner amendments 2026-10-11 and 2026-10-16;
Ubuntu 24.04, RHEL 10 and a native Ubuntu desktop left the matrix). JaegAW
Windows 11 elevated SSH and Ubuntu 26.04 WSL shell runs were used and remain
valid evidence for their actual narrower operations, not substitutes.

The complete development run ended 374/415 passing (41 raw failures; two
release-owned corrections subsequently pass focused checks). Serial Qt passed
75/84; all nine failing Qt assertions reproduced with Qt 6.10.3. Ten broader
representatives also failed the old baseline; other failures were not all
classified. Existing BPR0940/0945/0967–0972 retain related test/layout/resource/
privacy/fuzz/scaling work. BPR0962 must reconcile relevant fixes and reverify the
release candidate; do not duplicate, silently waive or close those other cards.
Gitleaks 8.30.1 source/delta scans exited 1 with 16/2 documented benign synthetic,
prose and identifier findings; exact classifications are retained without global
suppression. Two 100,000-iteration ASan/UBSan fuzz courses are bounded maintenance
core/diagnostic evidence, not complete application or native coverage.

R4 current checks at `4f2c3bdb5855ba0fb509889cdbe08d7cf534acab` pass 13/14:
the remaining `test_network_isolation_manifest` fails on ignored nested
checkout registrations and an independently visible
`release_update_native_https` one-port versus pinned-zero mismatch.
BPR0962 AC2/AC3 explicitly owns reconciliation and clean matching-root rerun;
no allowlist or test was weakened. Tracked-source architecture passes separately.
Full old-Mac package/security re-audits refuse `package-build-source-drift`
because current `BprQtProvider.cmake` includes the later Windows installer pin.
Keep the original exact producer audits; do not label old artifacts as current
source qualification. Fresh artifact hashes, Mac source-asset preflight and
local ad-hoc seal pass without claiming independent trust.

## Human review and protections

Session `cbd48b91000b00f1e72e1ad62c7c384c`, hand-back cursor 478
(later hand-back 589 on 2026-09-24), host/server stopped. Existing eight protections and all candidate bytes remain.
No new decision or review server is created by r4.

- Original M8 source/map and maintenance accepts remain at producer
  `6ad716350c7f2e02a1b00370ea49cd5b77ce4c15`. Recovery accept remains
  `80481e80d60f739db4c7eaea5e2eb8716ace2b4d`, decision
  `f9c2ddba-40bd-4d4d-b8f8-295a7056db79`, events 459/460.
  These are three scoped questions, not complete native acceptance.
- Mac `rfc0014-m9-licensing-v003` accepted at event 477, hand-back 478,
  decision `a3a22f09-afe9-4386-8d2d-e784f1e0ee9e`,
  fingerprint `fd1f44666e40b376bcb426d8ddec6e6067cfcdfaa284d93b7b72e8c210d2236a`.
  Only the exact Qt 6.11.2 inventory/source/build/relink/distribution criterion
  is accepted. Empty raw note supplies no further legal/security opinion.
- Windows `rfc0014-m9-windows-licensing-v002` was unanswered at hand-back 478
  and is **accepted** on 2026-09-24: decision
  `afa8af40-56ea-4c19-aaed-4f48ebde1218`, event 588, hand-back 589, empty note,
  carried forward unchanged in
  [owner-decisions-20260924-v001](reviews/owner-decisions-20260924-v001/operator-packet.md).
  Fingerprint `e19c8c7713cd7b79f80ea5a21fe557690d71289677cbd6a61d07fdd749b00a74`,
  producer `67f5e51407a883e4469cee6008048a001f7c13b3`; MSI, SPDX and
  separate-source digests re-verified unchanged. Only the exact Qt 6.11.2
  Windows inventory/source/build/relink/distribution criterion is accepted,
  not security, vulnerability, signature, native or publication approval. Its
  manifest and protected payload are retained, and the session's server is
  stopped. [[BPR-0962]] records it.

## Continuation

This successor is Proposed and unclaimed. Fresh engineering first reads the
live five cards, this ledger, original M8/M9 handoffs and shared plan. Obtain the
specific missing input or disposition; claim only eligible bounded work.
Bind all future tests and decisions to actual candidate bytes. Reuse earlier
evidence only where inputs are unchanged. Do not sign, notarize or publish merely
because RFC0014 is Auto-Verified. No remote station lease or writer is handed off.

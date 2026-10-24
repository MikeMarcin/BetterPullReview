# RFC-BPR-0038 qualification prerequisites, 2026-10-16

This is the documentation-only handoff for the focused BPR-0958–0962 and
BPR-0973 qualification effort. It neither authorizes publication nor accepts a
security, legal, trust, native-lifecycle or privacy criterion.

## Security-review and distribution-owner update, later on 2026-10-16

The owner requested a prepared independent finding review for another agent
to check. The [review packet](RFC-BPR-0038-independent-security-review-packet-20261016.md)
and [64-row queue](RFC-BPR-0038-independent-security-review-queue-20261016.json)
bind the current historical Mac and Windows matches. No reviewer is named in
the queue and no finding has an accepted disposition. A separate agent's
analysis must identify its reviewer and the accountable independent security
owner before any policy-relevant applicability decision can be accepted.

Michael Marcin is the named **distribution decision owner**, assisted by his
agents, as the owner stated “me and my agents.” This assigns responsibility;
it does not resolve the present inventories' first-party terms, third-party
notices, corresponding-source access, replacement/relink instructions or
retention. Each current or future exact package still needs a recorded
distribution decision and fulfilled access. The September 24 Windows
licensing-v002 acceptance remains limited to producer `67f5e51407a883e4469cee6008048a001f7c13b3`.

## Owner matrix update, later on 2026-10-16

The active qualification targets are native Intel and Apple Silicon macOS
13+, **JaegAW Windows 11 x64**, and **JaegAW Ubuntu 26.04 x86_64 under
WSL2/WSLg**. The owner removed Windows 10 qualification and accepted WSL2/WSLg
as the Linux qualification environment, limiting Linux release claims to
WSL2/WSLg. A separate native Ubuntu desktop is no longer required by this RFC.
The table and older stop-state text below describe the earlier handoff and do
not expand this amended matrix. Windows 11 still needs an ordinary-user
desktop, and WSL2/WSLg still needs actual Secret Service, rendered, install,
vault, purge and removal evidence on exact artifacts. The owner will provide
native Intel Mac hardware. The new user-directed retry produced the linked
[scan evidence](RFC-BPR-0038-vulnerability-dispositions-20261016.md) without a
new service rejection; the earlier service restriction remains prior-task
history, not proof that later security work is permitted.

## Evidence already reconciled

The exact machine-readable record is
[RFC-BPR-0038-evidence-reconciliation-20261016.json](RFC-BPR-0038-evidence-reconciliation-20261016.json).
Its 14 retained products were rehashed against receipts.

| Evidence | Exact scope that may be reused | Scope that remains open |
|---|---|---|
| Mac 0.2.1, source `c233c768327cd6c0cb6807860625ec12a9aeac14` | Published downloaded bytes; Developer ID deep/strict signature; app and DMG notarization staples and Gatekeeper acceptance; recorded normal and optimized gates. | This historical artifact has 33 raw vulnerability matches and no independent security acceptance, native Intel run, clean-account vault course or resident-material crash/privacy course. |
| Windows 0.2.2, source `23447f3a9bacf9a080a93df18b1fb2f54f6ebd3d` | Published downloaded bytes; MSI and four source-built image signatures/timestamps; administrative extraction, package mechanics and exact PE maps; repaired focused Windows/WSL/cache evidence. | Extraction is not ordinary-user installation. The original resumed 705-test account is 703 pass, one failure and one configured skip. Clean Windows 10/11 install, vault, accessibility and removal remain open. |
| Windows final Grype report | Grype 0.120.1, valid v6.1.10 database built `2026-10-15T06:30:36Z`, raw SHA-256 `2b959ae13bf3be5074b40dbabf6ecabebffd87576db9c133f6aa67aaba330d9f`, exit 2. | All 31 matches / 23 advisories remain undisposed: Tika 12, SQLite 3, Ruby-zlib identity 1, native zlib 3 and PCRE2/SLJIT 12. PCRE2 10.49 is the recorded remediation target; no fix or applicability decision is accepted here. |
| BPR-0940/0945/0967–0972 | The Done-card acceptance records, revisions and card hashes embedded in the reconciliation JSON. | Their source, platform and fixture limits remain. They do not establish native package, vault, trust, security-owner or distribution acceptance. |
| BPR-1527 and BPR-1551 | BPR-1527 producer history `7815a5395`, final handoff `333cc904a`; BPR-1551 completion `be8bcd1045375777da9008f34dd434f0ede9810f`. | Future common-source integration and exact-package requalification remain separate. Existing published releases must not be rebound or republished. |
| Windows licensing-v002 | Historical acceptance on 2026-09-24, event 588, decision `afa8af40-56ea-4c19-aaed-4f48ebde1218`, producer `67f5e514`. | No approval transfers to changed bytes, a future common-source release, recipient-access/retention changes or Linux. |
| BPR-1400/1402/1404 | BPR-1400 and BPR-1402 are Done at their recorded scopes. Later Mac/Windows evidence supersedes BPR-1404's older published-slice narrative. | BPR-1404 is not silently completed; its remaining Ubuntu/full scope still lacks evidence. |

The desired future Mac, Windows and Ubuntu release still needs one common version
and source revision plus distinct evidence for every exact platform artifact.
Mac 0.2.1 and Windows 0.2.2 are historical releases from different sources, so
their receipts cannot satisfy that future freeze.

## Inputs required before native qualification can finish

- [ ] A native Intel Mac running macOS 13 or newer. A universal slice or Rosetta
  execution does not satisfy native Intel evidence.
- [ ] A clean Apple Silicon macOS 13+ ordinary-user account with an unlocked
  default Keychain and authority for the attended authentication step.
- [ ] A clean non-elevated ordinary-user Windows 11 x64 desktop on JaegAW with
  Credential Manager. JaegAW is authorized for this focused work; the earlier
  elevated SSH and administrative extraction remain narrower evidence.
- [ ] An ordinary-user Ubuntu 26.04 x86_64 WSL2/WSLg session on JaegAW with
  working Secret Service for the exact signed DEB and per-user archive. Ubuntu
  desktop outside WSL2/WSLg, Windows 10, Ubuntu 24.04 and RHEL are outside the
  selected qualification scope.
- [ ] One fresh, short attended session per human judgment for fixture-PR sign-in,
  high-DPI presentation, keyboard/focus, accessibility and uninstall behavior.
  Use only the designated BPR-Test repository and do not inherit old sessions.
- [ ] Exact per-platform lifecycle evidence for ordinary-user install, upgrade,
  restart, credential continuity/deletion, uninstall, purge and residual state.
- [ ] A separate named security reviewer and accountable independent security
  owner to inspect the raw findings and proposed explanations for each exact
  candidate, then record accept/reject decisions. The implementer cannot
  approve their own vulnerability rationale. The review packet and queue are
  ready for the owner's other agent.
- [x] Michael Marcin is assigned as the named distribution decision owner,
  assisted by his agents.
- [ ] Record and fulfill distribution decisions for each current exact
  inventory: notices, corresponding-source access, replacement/relink
  instructions and retention. The September 24 Windows decision is historical
  only; a new package needs a new decision and fulfilled access.
- [ ] Independent trust verification for every final Mac, Windows and Ubuntu
  artifact, bound to its digest, source, SBOM and corresponding-source asset.
- [ ] Unresolved runtime, privacy, hardening and dependency courses must run in
  this authorized scope and stop if a new service restriction appears.

## Stop state

The prior continuation produced limited local development observations before
the service restriction. They are recorded in the
[continuation checkpoint](../issues/_checkpoints/RFC-BPR-0038/continuation.json)
only to prevent accidental repetition. All uncommitted product, test and
dependency-preparation edits were removed from this branch. Existing dependency
pins, policies and Mac 0.2.1 / Windows 0.2.2 release assets remain unchanged.

BPR-0958–0962 remain unresolved and BPR-0973 cannot close the RFC. No criterion,
release, waiver, reviewer authority or legal acceptance is inferred from this
handoff.

The documentation unit suite passes 67/67 and `git diff --check` passes. The
repository-wide documentation gate stops on unrelated concurrent allocator
drift: `docs/issues/NEXT_ID` is `BPR-1558` while Git history has reached
`BPR-1561`. This task leaves that shared allocation state unchanged.

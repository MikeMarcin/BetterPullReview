# RFC-BPR-0038 independent finding review packet, 2026-10-16

This packet hands the existing BPR-0962 vulnerability work to a separate
reviewer. It contains **proposals**, not security acceptance. The historical
Mac 0.2.1 and Windows 0.2.2 scans still fail, and neither package represents a
future common-source Mac/Windows/Ubuntu release. No release asset or policy is
changed by this packet.

## Reviewer brief

Independently check every package/advisory pair in the
[64-row review queue](RFC-BPR-0038-independent-security-review-queue-20261016.json)
against the [proposed dispositions](RFC-BPR-0038-vulnerability-dispositions-20261016.md),
the exact [Mac](RFC-BPR-0038-macos-0.2.1-vulnerability-scan-20261016.json) and
[Windows](RFC-BPR-0038-windows-0.2.2-vulnerability-scan-20261016.json) summaries,
the raw reports, relevant source and binaries, and primary upstream advisories.
Do not inherit a conclusion merely because the proposal cites it. For each
row, record the exact-component assessment, primary evidence, an explicit
accept/reject/remediate/insufficient-evidence decision, and an expiry or
recheck trigger. Record the reviewer's identity, review date, and the named
accountable independent security owner. Leave a row pending when its evidence
does not support a decision. Keep raw scanner failures and all matches.

The current [release policy](../security/release-policy.md) requires Critical
and High matches to be fixed or to receive an independent, named security
owner's source-backed, exact-version applicability decision with an expiry or
recheck condition. Lower-severity findings also require disposition. A second
agent's technical check can supply independent analysis; security-owner
acceptance must still be explicit and attributable. This review cannot infer
application-wide safety from a scanner result or a narrow component mismatch.

## Frozen historical inputs

| Input | Mac 0.2.1 | Windows 0.2.2 |
|---|---|---|
| Platform and architecture | Darwin universal x86_64/arm64 | Windows x64 |
| Producer source | `c233c768327cd6c0cb6807860625ec12a9aeac14` | `23447f3a9bacf9a080a93df18b1fb2f54f6ebd3d` |
| Package SHA-256 | `8a635a4b8a99caaf5cb7db3aa2344772749e8bd8080272d63004a9f80c4ac4f8` | `b3e2a5420893604ebb6b30d7708cb2405d090a16e09dc459d049bf1155a50f85` |
| SPDX SHA-256 | `db51b084a3641c0e674b0ab5b9e689ba2d68ba0d79c89370157d9ffd47b21f38` | `088c1778af8dec5125ded29651100a901fb8fad49ec33e30e3be2f1d7a93d039` |
| Dependency inventory SHA-256 | `7dfc8b5dea3f4189fd7ef8ba9152e092fd3c3668a78dc39350237741be8a00a1` | `7dfc15f986e508226d2182311b0f5aaa8c60d46e744e429e6ca48399f58de472` |
| Raw Grype JSON SHA-256 | `8029ef64c1700e0f01cc9129b0f950b494654595279af5ebf3decea3fcd629a0` | `a4400641a562ef49673475ba9fa7cb688ed1e410eb3182e347c7b1d6663d3529` |
| Unsuppressed result | Exit 2; 33 matches / 27 advisory IDs | Exit 2; 31 matches / 23 advisory IDs |

Both scans used Grype **0.120.1** with valid database schema **v6.1.10**,
built `2026-10-16T06:29:51Z`, and `--fail-on negligible`. The exact database
URL, package and SPDX names, inventory count, scanner configuration and every
finding are in the machine-readable summaries. The [reconciliation
record](RFC-BPR-0038-evidence-reconciliation-20261016.json) binds the other
retained release assets. On this host, the raw reports are
`/private/tmp/bpr-rfc0038-security-audit-20261016/macos-0.2.1-grype.json` and
`/private/tmp/bpr-rfc0038-security-audit-20261016/windows-0.2.2-grype.json`.
Those task-local paths are conveniences, not durable review evidence; verify
their hashes before use. If they are unavailable, repeat the exact scan using
the retained asset identities and keep a new raw report. A changed database or
finding set is a new observation requiring reconciliation, not a reason to
alter the queue or suppress a match.

## Review questions that can falsify the proposals

| Match group | Rows | Checks required before any decision |
|---|---:|---|
| PCRE2 10.47 and SLJIT attribution | Mac 12; Windows 12 | Check both SPDX identities, compiled PCRE2 options, architecture, the exact Qt call paths and each upstream precondition. The six reported advisories must be assessed separately on each artifact. Also investigate unreported `CVE-2026-86145` and `GHSA-r9hj-j2rw-4q3m`; feed silence is not a disposition. The forward remediation target is PCRE2 **10.49**, never 10.48. |
| Tika MIME XML attribution | Mac 12; Windows 12 | Verify the exact attributed PURL/resource and whether any Java Tika handler code is present or reachable in the package. A data-only mismatch can resolve the component identity of a match; it cannot prove unrelated parsing paths safe. |
| SQLite 3.53.4 | Mac 3; Windows 3 | Confirm exact bundled source/version and each upstream fixed version. Check whether Grype's affected range is genuinely wrong for that advisory and package. Retain the High raw match. |
| Ruby-zlib `GHSA-g857-hhfv-j68w` | Mac 1; Windows 1 | Verify the SPDX package is native madler zlib 1.3.2 and the advisory concerns the Ruby `zlib` gem; do not conflate this with native zlib findings. |
| Native zlib `CVE-2026-85091` | Windows 3 | Verify all three exact 1.3.2 package/attribution entries and the upstream fix. The historical MSI predates that fix; a source-only patch or unrelated build is not package remediation. Keep the High blocker until exact rebuilt bytes, SPDX and native evidence exist, or the policy's independent applicability route is actually met. |
| D-Bus header attribution | Mac 3 | Distinguish shipped 1.13.12 headers from an implementation library; inspect the actual external runtime load/version before any claim about the CVEs. A header-only mismatch does not waive runtime risk. |
| Apple system zlib `WrapZLIB` | Mac 2 | Verify the two QtCore slices' dynamic load, supported OS patch baseline and call path separately. A Mach-O current-version value does not prove an OS security patch. `CVE-2026-27171` remains unresolved in the proposal. |

The queue has one row for each of the **33 Mac plus 31 Windows** raw matches.
It separately records the two PCRE2 advisories absent from these Grype reports;
those are coverage questions, not invented scanner matches. The initial queue
contains no reviewer, security owner, decision, rationale or expiry.

## Expected review record

Return a separate, versioned decision record without changing the frozen
source summaries. For every queue row, identify its ID and exact artifact,
package, version and advisory; give the decision and rationale; cite primary
sources and exact binary/source observations; record an expiry or a concrete
recheck trigger. State any unsupported claim as pending. Include the two
outside-feed PCRE2 assessments as separate entries. Record disagreements with
the proposals and the exact evidence needed to resolve them. The named
security owner must explicitly accept any policy-relevant applicability
decision; a reviewer's analysis alone is not that acceptance.

Future common-version Mac, Windows 11 and Ubuntu 26.04 WSL2/WSLg artifacts
need their own packages, SBOMs, inventories, scans, source revisions and
independent trust and security decisions. Historical decisions cannot be
copied to changed bytes. BPR-0962 remains **Blocked**, with AC3 failed and all
nine RFC-BPR-0038 criteria open.

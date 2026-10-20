# GitHub hosted-content secret audit — 2026-10-19

## Result

No new BPR credential was found in the audited release assets or accessible
hosted content. The repository remained **private**, confirmed again at
2026-10-19 23:42 UTC. No release, asset, ref, workflow, visibility setting or
other hosted content was changed. No push or GitHub Support ticket was needed
for the findings described below.

Subsequent owner-authorized cleanup is recorded separately in the
[publication-readiness report](publication-readiness-2026-10-19.md): Actions
was disabled, and four early releases lacking expected distribution material
were archived privately and deleted from GitHub. The 14-release/115-asset
inventory below remains the original audit scope, before that cleanup.

This is a bounded secret audit, not clearance to make BPR public. The owner
confirmed that neither Packages nor Projects is used for BPR, closing those
two inventory questions by owner confirmation. GitHub returned 404 for
secret-scanning alerts and repository security advisories;
those responses are not evidence of empty inventories. The earlier
[retired review-session credential investigation](../development/betterleaks-history-remediation.md)
still applies to Git history.

The [machine-readable receipt](../verification/github-content-audit-2026-10-19.json)
records all 115 asset IDs, names, byte counts and SHA-256 digests; the 29 remote
refs; scan counts; upstream comparisons; and every finding's file digest,
member location, rule and line/column. It contains no matched values or source
snippets. Counts describe scanner occurrences, including derived binary-string
views, rather than distinct credentials.

## Scope and acquisition

| Surface | Observed and audited |
|---|---|
| Releases | All 14 releases, `v0.1.0` through `v0.3.0`, including notes and asset metadata |
| Release assets | All 115 assets, **2,571,952,561 bytes**; every downloaded size and SHA-256 agrees with GitHub's asset metadata |
| Repository metadata | Description, URLs, settings and other returned repository JSON |
| Git metadata | Messages/headers of 9,025 commits reachable from the 15 remote branches and 14 tags, plus all 14 annotated tag objects; all required objects were already local |
| Issues and PRs | Zero issues, PRs, issue comments, review comments and commit comments, including closed items |
| Actions | One old completed run and its job metadata; its retrieved log ZIP is empty (22 bytes). Zero retained artifacts, caches and repository variables |
| Deployments and configuration | Zero deployments, environments and webhooks; Wiki, Pages and Discussions disabled; zero forks |
| Linked attachments | 29 distinct upload/image URLs found by inspecting 113,246 remote-reachable Git objects and returned API content. All trace to synthetic Markdown-image fixtures, tests or their generator; no real BPR upload identified |

Acquisition used authenticated, read-only GitHub API calls with pagination and
actual response-header rate-limit checks. No new Actions run was started.
Downloaded inputs and unpacked bytes stayed in the ignored, owner-private
`.cache/betterleaks/github-audit-20261019/` directory. Input copies can contain
unreviewed content; they are not reports and must not be committed or uploaded.

GitHub's repository API supplied a `temp_clone_token`. The scanner detected it
in the private acquisition input. It is an API-generated temporary access
credential, not user-authored hosted content. It was never used, printed or
committed; its value was removed from the input and content cache. Reports
retain only the field name and disposition. Final repository acquisition
redacted that field before writing anything to disk.

A second API inventory found the same refs, release contents and accessible
surfaces. The comparison disregarded asset download counters changed by this
audit. A fresh scan of the final metadata returned zero findings.

## Inspection and controls

The audit ran the installed, digest-verified **BetterLeaks 1.9.0** binary on
macOS 26.6.2 arm64, Python 3.14.7. It used an isolated configuration extending
the default rules and the BPR review-session URL rule, with no repository
fixture exceptions. The default path prefilter was disabled. Ambient config,
ignore files and inline allow comments had no authority in the isolated scan.
Credential validation was disabled. Only location/rule metadata was emitted
through the report template; match text, captures and source lines were absent.

Every scan and extraction ran under macOS Seatbelt network denial. Acquisition
was a separate explicit operation. Neither scanning nor committing downloaded
dependencies, fetched a repository or attempted to authenticate a discovered
credential. No scanner command or scanner-dependent test was added to CI or
normal test discovery.

DMGs were mounted read-only without opening an application. ZIP, tar,
gzip/xz, DEB/ar, CAB and MSI/OLE payloads were read recursively without running
installers. Members were stored by SHA-256, with an index retaining each asset
and member path. Links were inspected as text, not followed. macOS libarchive
and a locally cached, checksum-verified `olefile` 0.47 wheel supplied extraction;
no global package was installed. The receipt pins that wheel.

The scan covered **42,222 distinct inputs totalling 4,962,690,379 bytes** after
deduplication, including archive containers, extracted members, metadata,
ASCII binary strings and UTF-16LE/BE string views. String views retain byte
offsets. Disposable controls showed plain and NUL-wrapped ASCII canaries were
detected. Native scanning missed a UTF-16 canary, so explicit UTF-16 views were
added and both byte orders then passed the detection control. Native archive
recursion was disabled in favor of the separately inspected extraction graph;
text decoding used a depth limit of five.

A separate member-name inspection found no `.env`, credential/secret file,
SSH private-key filename, PEM/key file or PKCS#12/Java keystore outside the
identified public upstream sources. This supplements pattern detection; it
does not establish that arbitrary binary data cannot contain a key.

## Finding dispositions

The unrestricted audit produced **1,121 occurrences in 197 distinct inputs**:

| Disposition | Occurrences | Evidence |
|---|---:|---|
| Public upstream source | 1,027 | QtBase 192, Glaze 22, HACL* 696, LibArchive 4, Git 111, ICU 2 |
| Qt PEM-format markers | 84 | Adjacent `BEGIN`/`END` delimiters used by Qt's key parsers, with no encoded key body; matches occur in derived strings from macOS, Windows and Linux runtime libraries |
| Public signing fingerprints | 10 | `keyFingerprint` fields in the Linux 0.3.0 release/provenance JSON; values match issuer fingerprints parsed offline from the six detached signature assets |

The corresponding-source archives contain upstream TLS test keys, password and
credential-URL examples, cryptographic test vectors, and source syntax mistaken
for credential assignments. All 22 distinct bundled dependency archives match
the repository's pinned source checksums. For **each of the six components with
findings**, a fresh unauthenticated download from its recorded public upstream
URL was hashed independently and matched the entire shipped archive. The
receipt gives those URLs and hashes. This establishes the flagged material's
public upstream provenance; it is not a test of whether any credential works.

No fixture was deleted, source archive modified, rule disabled or new scanner
exception added. Keeping the original dependency sources preserves their
source-delivery and retention obligations. The existing local policy remains
unchanged by this audit.

An additional in-memory exact-value search found **zero copies of the retired
BPR review token** across all 42,222 audit inputs, checking ASCII and both
UTF-16 byte orders. This extends the earlier Git-object investigation to the
downloaded assets and accessible hosted metadata.

## Remaining limits and publication follow-up

- **Packages and Projects — resolved by owner confirmation:** the owner's
  follow-up, recorded at 2026-10-20 00:09 UTC, states: "Neither is used for
  BPR." Both wizard steps are complete from that explicit answer. Independent
  API inventory remains unavailable because the credential lacks
  `read:packages` and `read:project`; this is not an API-verified empty
  inventory. Global credentials and scopes were not changed.
- **Unavailable or historical hosted data:** 404 alert/advisory responses,
  empty retained logs, deleted assets/comments, unlinked uploads, GitHub
  caches, unreachable server objects and external clones are outside the
  inspected content. The attachment search cannot enumerate every upload
  ever associated with an account.
- **Two malformed upstream fixtures:** Qt's
  `tests/auto/gui/image/qimagereader/images/corrupt.svgz` is truncated;
  Git's `t/t5000/huge-and-future.tar` is not fully readable. Both were scanned
  raw, and the available decompressed SVG prefix was also scanned. Their
  complete enclosing archives match the public upstream downloads. No other
  extraction error was recorded.
- **Detection boundaries:** regex/entropy detectors and their built-in content
  heuristics are not a proof of absence. Scanning bytes and extracted strings
  does not decompile executables, OCR images, recover encrypted content or
  decode every application-specific resource or encoding. Extraction used a
  depth bound of 12 and a 1 GiB member bound; neither bound was reached.
- **Platform and release trust:** Windows and Linux payloads were inspected
  on macOS. Their installers were not executed or natively qualified. Digest
  comparison and signature-packet inspection are not a new signing/trust or
  release-readiness approval.

The only previously established BPR credential is the retired local review
token retained in two historical Git blobs. Its live risk was resolved by
session retirement; original history intentionally still fails the local
scanner. The separate cleanup rehearsal has not been applied to shared refs
or GitHub. If byte removal is chosen, regenerate the rehearsal after a
coordinated freeze and reconcile signed release provenance before any
explicitly authorized push.

[GitHub's removal guidance](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository)
limits Support assistance to sensitive exposure whose risk cannot be mitigated
by credential rotation. Public test material, parser delimiters and public
fingerprints do not establish such an exposure. No purge ticket was filed.
If a non-revocable exposure is later found, prepare a redacted case identifying
affected asset/object IDs and the completed remediation before contacting
Support. Keep BPR private until the outstanding publication checks are resolved.

## Local follow-up

The [local tooling guide](../development/betterleaks.md) documents setup,
staged/tracked/history commands, hook installation/removal and recovery.
The managed pre-commit hook remains active in the integration checkout.
The explicit nine-course scanner integration check previously passed with
network access denied; this audit added separate disposable extraction and
encoding controls, outside normal test discovery.

Final local checks passed: the documentation gate, receipt consistency,
staged scanning and all 9,971 current tracked files. All-ref history still
reports the same 18 redacted occurrences of the one retired review token in
the two known historical files. Workflows and existing CI test entrypoints
are unchanged.

The ignored audit directory retains acquisition/extraction helpers, private
inputs, the content graph and metadata-only receipts for local reproduction.
Do not treat its raw inputs as shareable evidence. The committed receipt is
the redacted record to review. Re-inventory and scan new or changed hosted
content before relying on this snapshot for publication.

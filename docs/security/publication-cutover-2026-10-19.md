# BPR public cutover — 2026-10-19

## Result

The owner authorized pushing `main`, making BPR public, enabling private
vulnerability reporting and verifying anonymous release/source access while
keeping GitHub Actions disabled. That cutover is complete.

- [BPR](https://github.com/MikeMarcin/BetterPullReview) is public. The reviewed
  `main` commit `293f315c430f27f264b198f6df431576f0702831` was pushed before the
  visibility change. The documentation follow-up containing this report updates
  the security policy and records the result without changing executable code.
- GitHub private vulnerability reporting is enabled. The anonymous
  [Security page](https://github.com/MikeMarcin/BetterPullReview/security)
  exposes the private report link. [SECURITY.md](../../SECURITY.md) points
  directly to that form; submitting requires a GitHub sign-in. No test report
  was sent.
- Repository Actions permissions still report `enabled: false`. Historical
  workflow `323328391` remains `disabled_manually`; the single old completed
  run is unchanged. No new run was started.
- All **10 release pages and 99 assets** are accessible anonymously, including
  **24 binary packages and all 12 corresponding-source archives**. Latest
  remains [v0.3.0](https://github.com/MikeMarcin/BetterPullReview/releases/tag/v0.3.0).
- GitHub recognises the committed root licence as MIT. Anonymous downloads of
  `LICENSE` and the security policy matched the committed files.

The [machine-readable receipt](../verification/publication-cutover-2026-10-19.json)
records the exact push, mutation responses, settings, anonymous URLs, byte
counts and hashes. Its timestamps use UTC on 2026-10-20; the title uses the
owner's local date. Public visibility was confirmed at 01:46 UTC, and the
asset verification completed at 01:57 UTC.

## Verification

Immediately before publication, the authenticated read-only inventory found
the same **29 remote refs**, **10 releases** and **99 retained assets** as the
[preparation audit](publication-readiness-2026-10-19.md). All required Git
objects were local, no new objects were found, and no workflow files existed
at those tips. Asset IDs, names, sizes, digests and update times matched; all
cached asset bytes were freshly rehashed against current GitHub digests.
Twenty refreshed metadata files passed pinned BetterLeaks 1.9.0 under macOS
network denial, without repository fixture exceptions or credential
validation. An exact ASCII/UTF-16 search also found no retired-token occurrence
in that metadata.

The push used one explicit `main` ref, without force, automatic tag following
or recursive submodule pushes. GitHub confirmed the exact remote commit before
the visibility change. Private reporting was enabled through the repository
API and read back as enabled. Actions permissions were checked before the
push, after the push, after changing visibility and after enabling reporting.

Anonymous checks used fresh Python HTTP openers with no credentials, cookie
jar, credential provider, netrc or proxy configuration. They verified the
repository API and page, exact main ref, release inventory, all release pages,
Latest, Security and policy pages, and committed licence/policy text. All 99
assets were downloaded in full and hashed: **2,254,693,496 bytes**, each matching
the audited size and SHA-256. Eight connections initially failed; those eight
downloads passed a second anonymous check using curl with configuration,
authentication, cookies and proxies excluded. No final download failure
remained. Temporary signed redirect queries and response bodies are absent
from the receipt.

The [previous source-reference verification](../verification/publication-preparation-2026-10-19.json)
covers installed provenance, `SOURCE-OFFERS.txt` and `THIRD-PARTY-NOTICES.txt`
in all 24 binary payloads. The now-public binary and source bytes are identical
to that evidence. No release asset or tag was replaced, renamed or rebuilt.
The four previously retired early releases remain withdrawn; their original
tags and owner-private retention archive remain intact.

## Bounds and retained obligations

The source, notices, tag identities and equivalent recipient access must remain
available under the [release policy](release-policy.md). Public access does
not discharge obligations to earlier private recipients. The original secret
audit and reviewed public upstream fixtures remain applicable by exact digest.
Git history still retains the one retired local review token; these findings
do not call for a history rewrite or a GitHub Support purge ticket.

The post-cutover API returned an empty repository advisory inventory. It
reports `main` as unprotected and no rulesets. Repository secret-scanning,
push-protection and validity-check settings report disabled; the secret-alert
API still returns 404, not evidence of zero alerts. These settings were
inspected without changing them. The local BetterLeaks hooks remain the
configured developer check; no scanner or test was added to hosted CI.

Verification ran on macOS arm64. It establishes anonymous access and exact
bytes, not a fresh native installation, trust or vulnerability qualification.
The [v0.3.0 ledger](../verification/RFC-BPR-0038.md), bounded vulnerability
dispositions, Ubuntu RSS exception and wider open M8/M9 work remain unchanged.

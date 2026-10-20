# BPR publication readiness — 2026-10-19

## Decision

Keep BPR private while completing the publication steps below. Retain the
existing Git history: the only established BPR credential is one retired
loopback review-session token. Its session has ended; no new BPR credential
was found in the release assets or accessible hosted content. Neither a
history rewrite nor a GitHub Support purge ticket is justified by these
findings. The [history investigation](../development/betterleaks-history-remediation.md)
and [hosted-content audit](github-content-audit-2026-10-19.md) retain the
evidence and detection limits.

The integration is on local `main` at
`c634a9338e4400aee212dc24a760dcc19bdf757c`, including the existing README work.
The pinned scanner is installed and its managed pre-commit hook is active in
the main checkout. No push or visibility change was made. Hosted cleanup below
was separately authorized by the owner's follow-up on 2026-10-19.

The [machine-readable receipt](../verification/publication-readiness-2026-10-19.json)
records API status codes, settings, removed release identities, retained tag
identities, archive hashes and verification. Its UTC timestamps fall on
2026-10-20; this report uses the owner's local date.

## Completed hosted cleanup

### GitHub Actions

The workflow file `.github/workflows/ci.yml` was already absent from all
15 remote branch tips and 14 tag tips inspected locally. GitHub nevertheless
listed historical workflow `323328391`, named `CI`, as active. It is now
`disabled_manually`, and repository Actions permissions have `enabled: false`.
GitHub's [workflow API](https://docs.github.com/en/rest/actions/workflows#disable-a-workflow)
provides disabling, not a workflow-record deletion endpoint. The historical
record can therefore remain visible; no executable workflow file remains at
the inspected tips. No Git history was rewritten to remove historical YAML.

The one old completed run, `30498222845`, is retained. Its metadata was audited
and its retrieved log ZIP was empty. No new run was started. No scanner install,
scan or scanner integration test was added to Actions or normal CI discovery.
Repository-level disabling prevents workflows from running while that setting
remains in effect; verify it again at publication. See
[GitHub's Actions settings documentation](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository).

### Retired early binary releases

The owner authorized deleting old releases that would complicate publication.
`v0.1.0`, `v0.1.1`, `v0.1.2` and `v0.1.3` each had only a macOS DMG, ZIP and
their two checksum files. They had no corresponding-source or SPDX release
asset. Their eight extracted DMG/ZIP payloads contain Qt frameworks, but no
member named `THIRD-PARTY-NOTICES.txt`, `SOURCE-OFFERS.txt`,
`source-provenance.json`, `LICENSE*`, `COPYING*` or containing `spdx`.
This establishes missing expected distribution material, not a determination
of every obligation applicable to those historical bytes.

Those four GitHub releases and their 16 assets have been deleted. Every tag
still resolves to its original object. The remaining **10 releases and 99
assets**, including Latest `v0.3.0`, retain their original IDs, names, sizes,
digests and release notes. The repository remains private.

Before deletion, all **317,259,065 asset bytes** were copied to the owner-private
retention directory:

```text
/Users/jaeger/Documents/Codex/releases/retired-v010-v013-20261019
```

Every asset was rehashed against the audited GitHub digest. The directory also
contains original notes/metadata, a manifest, and a verified self-contained
Git bundle of the four original tags and their reachable history. Files are
read-only and the archive directories are owner-only. Its ten metadata/text
files passed an additional secret scan with network access denied. The bundle
preserves first-party history; it is not a reconstructed corresponding-source
package for all shipped dependencies.

Withdrawal prevents these incomplete binary sets from becoming public through
the repository's releases. It does **not** discharge obligations to existing
recipients. Retain the originals indefinitely under the
[release policy](release-policy.md), provide required recipient material through
the original private distribution channel, and investigate an exact historical
source request against its own producer inputs. Do not substitute v0.3.0's
source archive or republish the old binaries as a repaired release. No source
asset was deleted; these four releases had none. Later source assets remain
unchanged and available with their matching binaries.

## First-party licence

The owner selected MIT after asking about MASK, ATT and AgentSkills. The local
MASK and AgentSkills `LICENSE` files both use MIT and the convention
`Copyright (c) 2026 <project> contributors`. A separate ATT licence could not
be located in the connected projects or inspected repository inventory;
its licence is not claimed verified.

BPR now follows the verified convention in its root [LICENSE](../../LICENSE):
`Copyright (c) 2026 BetterPullReview contributors`. The README limits this
grant to original BPR material and preserves third-party and adapted-material
notices. Future packages include the complete MIT notice and declare MIT for
the BPR package in SPDX. Aggregate `licenseConcluded` remains `NOASSERTION`;
dependency declarations are unchanged. The two source-helper bundles (xdiff
and ICU) pin and carry the same licence text with their BPR build material.
No existing release asset, SPDX or corresponding-source archive was rewritten.

Focused licence checks ran with network access denied: package runtime tests
passed 28 cases with one explicitly skipped Linux-native CPack check; all 21
source-asset tests passed. These exercise emitted notices/SPDX, exact licence
bytes in source helpers, digest refusal and publication preflight fixtures.
They do not build or qualify new native distribution artifacts.

## Remaining publication steps

| Step | Current evidence and required result |
|---|---|
| Prepare the public operating policy | Add a concise `SECURITY.md` with the intended private reporting route and supported-version scope. Update private-only README/release instructions and deliberately adapt the publisher's private-repository guard before relying on it for public releases. Preserve source binding, signing and exact-artifact checks. |
| Reconcile retained distribution material | Use the existing v0.3.0 and earlier exact-release records for the remaining binaries. Keep corresponding-source access and retention intact. The retired early releases do not need to be rebuilt merely to expose their Git tags, but past recipient obligations remain. Do not treat private package acceptance as blanket approval of new public distribution terms. |
| Freeze and refresh the audit | Just before publication, record the exact proposed `main`, remote branch/tag set, releases/assets and hosted inventories. Scan changed content with the pinned offline scanner; reuse unchanged asset evidence only after matching IDs and digests. Confirm that the known retired token is still the only unresolved historical match. Stop on any new unexplained finding. |
| Perform an explicitly authorized cutover | Push the reviewed local commits only when requested, confirm private visibility and Actions disabled, then change visibility only on the owner's explicit instruction. Verify anonymous repository, installer and exact corresponding-source access afterward. Inspect available security settings/alerts and the reporting route without sending a test vulnerability report. |

The initial GitHub inventory still reports no recognised licence because the
MIT adoption has not been pushed. Publication and licensing are separate;
making a repository public does not itself supply an open-source grant. See
[GitHub's licensing guide](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository).

The current main-branch API reports `protected: false`; ruleset inventory
returned 403 and is not established. Review branch/tag protection during the
cutover, particularly force pushes and deletion of release tags. Do not add
required hosted CI checks to a repository whose Actions execution is disabled.

GitHub private vulnerability reporting is a public-repository feature, so the
current private-repository 404 does not establish an empty advisory inventory.
Prepare the policy now; enable and verify the private reporting route when the
repository becomes public. See
[GitHub's reporting setup](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository).

GitHub automatically scans public repositories for supported secrets and may
notify credential providers. This platform behavior is separate from the local
offline scanner; do not enable optional credential-validity checks or promise
that GitHub's public scanning is offline. See
[GitHub's secret-scanning documentation](https://docs.github.com/en/code-security/concepts/secret-security/secret-scanning).

Treat the visibility change as publication of every accessible branch, tag,
release and retained public-facing record. Copies and public forks cannot be
recalled simply by making the repository private again. See
[GitHub's visibility guidance](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility).

## Evidence that remains bounded

The secret audit covered all 115 then-hosted assets before retirement, including
recursive archive extraction and ASCII/UTF-16 views. It found public upstream
test material, Qt parser delimiters and public signing fingerprints, with
independent provenance evidence. It did not establish absence of secrets in
every encoding, encrypted payload, unlinked upload, deleted record, server
cache or external clone. Packages and Projects are owner-confirmed unused;
their inventories were not independently API-verified.

Local BetterLeaks integration was tested against the real pinned 1.9.0 binary
on macOS arm64, including actual commits, partial staging, `git commit -a`,
missing/incompatible binaries, hook conflicts, suppression attempts, exact
fixture exceptions and negative controls. Nine local courses passed with
network denied. Windows and Linux asset bytes were inspected on macOS;
scanner tooling was not natively tested on those platforms. History scanning
still correctly fails on the 18 redacted occurrences of the retired review
token; no exception was added to make that history appear clean.

The [current release qualification ledger](../verification/RFC-BPR-0038.md)
remains authoritative for v0.3.0. Its bounded vulnerability dispositions,
Ubuntu RSS exception and open native/vault/crash/privacy/accessibility and
other M8/M9 work remain explicit. Making source public need not close every
product RFC, and must not imply that these product qualifications passed.
The [security overview](corporate-security-overview.md) now separates current
v0.3.0 evidence from historical unsigned and unpublished candidates.

## Local commands

From this checkout or the main checkout:

```sh
python3 scripts/betterleaks.py setup
python3 scripts/betterleaks.py staged
python3 scripts/betterleaks.py tracked
python3 scripts/betterleaks.py history
python3 scripts/betterleaks.py hook-status
```

Only `setup` downloads anything. Scans and commits do not fetch repositories,
download dependencies or validate credentials. See the
[tooling guide](../development/betterleaks.md) for opt-in installation/removal,
recovery and the explicit local integration-test command. The active managed
hooks preserve other hooks and repository/worktree configuration boundaries.

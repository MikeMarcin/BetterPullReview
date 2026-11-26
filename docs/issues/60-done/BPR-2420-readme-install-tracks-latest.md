---
schema: "implement-rfc.card.v1"
id: "BPR-2420"
title: "README installs from the latest stable release and names no version, and the documentation gate keeps it so"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"docs","defect":true,"mandate_revision":"v0-5-0-prerelease-targets-r1","revision":"bfbb6e2a5","origin":"owner, 2026-11-26: README Install still pointed new users to 0.4.2 after 0.5.3 shipped; \"Let's make sure it just points to the latest stable release always.\"","reservation":"docs/issues/_reservations/BPR-2420--BPR-2420--fa8e94106b7a.json","related":["BPR-1743","BPR-2215","BPR-2309"],"acceptance":["BPR-2420:latest","BPR-2420:stable","BPR-2420:inventory","BPR-2420:gate","BPR-2420:release-flow","BPR-2420:docs"]}
---

# BPR-2420: README installs from the latest stable release

## Why

README's Install section said "The latest release is **0.4.2**" and linked the
0.4.2 build 60 DMG, MSI and DEB directly, three stable releases after 0.5.0
shipped its fix for reviewing untrusted pull requests. 0.5.3 was GitHub's
Latest. The section had been edited by hand at each release
(`dc402c20c`, `5fc1a81b9`) and nothing noticed when an edit was missed. Owner,
2026-11-26: "Let's make sure it just points to the latest stable release
always."

## Change

`README.md`, Install:

- Links `https://github.com/MikeMarcin/BetterPullReview/releases/latest` and
  says the download table at the top of its notes links each installer (every
  release body opens with that table, per the publish-release notes standard).
- The table names each system's installer by its file-name pattern
  (`BetterPullReview-…-macos-universal.dmg`, `…-windows-x64.msi`,
  `…-linux-ubuntu-26.04-x64.deb`) instead of a pinned URL.
- Ubuntu's signature steps link `docs/development/release-packages.md#check-signatures`
  (the place 0.5.x release bodies link); the old link was 0.4.2's
  `#linux-signature-verification` release-body anchor, which no 0.5.x body has.
- The warning says releases are stable but qualification remains incomplete and
  links the package guide's Known limitations, instead of naming 0.4.2 and its
  `#verification-and-limitations` anchor.

`scripts/verify_documentation.py`, `verify_readme_install_tracks_latest`
(in `main`): refuses a README link to one exact release
(`…/BetterPullReview/releases/<anything but latest>`, anywhere in README), an
Install section without the `releases/latest` link, a missing Install section,
and any `X.Y.Z` / `vX.Y.Z` / `X.Y.Z-build.N` version in Install. Minimum OS and
tool versions there are `X.Y` and pass. `tests/scripts/verify_documentation_tests.py`
`ReadmeInstallTests`: six cases.

`.agents/skills/publish-release/SKILL.md` (Publish once): a stable release
needs no README edit; Latest excludes prereleases; the gate refuses a pinned
README; asset names carry `VERSION-build.N`, so README cannot use
`releases/latest/download/`.

## Acceptance

- **BPR-2420:latest** README's Install section reaches the installers through
  `releases/latest` and names no release version or pinned asset URL.
- **BPR-2420:stable** `releases/latest` resolves to a stable release: the 0.5.x
  stable releases are not prereleases, and the numbered `vX.Y.Z-build.N`
  candidates, which are, cannot become Latest.
- **BPR-2420:inventory** Every other user-facing place that names a release
  version is either historical by design or made version-independent.
- **BPR-2420:gate** The documentation gate fails on a README that pins a
  release or names a version in Install, and passes on the new README.
- **BPR-2420:release-flow** The publish-release skill says README needs no
  per-release edit and why direct `latest/download` links are not used.
- **BPR-2420:docs** `python3 scripts/verify_documentation.py --root .` passes.

## Verification

macOS arm64, Python 3.15.0. Evidence revision `bfbb6e2a5` (one commit on main
`806afd22d`).

| Criterion | Check | Result |
|---|---|---|
| BPR-2420:latest | README Install read after the change: one `releases/latest` link, patterns in the table, no `X.Y.Z` | pass |
| BPR-2420:stable | `gh release view 0.5.0/0.5.1/0.5.2/0.5.3 --json isPrerelease,isDraft`: all `false`/`false`; `gh release list`: every `v0.5.x-build.N` is Pre-release, 0.5.3 Latest; `gh api repos/MikeMarcin/BetterPullReview/releases/latest`: tag `0.5.3`, prerelease false, draft false (read-only, 2026-11-26) | pass |
| BPR-2420:inventory | `git grep` for `releases/(download\|tag)/`, `releases/latest` and `0.[345].N` over README, CONTRIBUTING, docs (excluding issues, verification, rfcs), packaging, `.agents/skills`: remaining hits are per-release notes `docs/releases/v*.md` (versioned by design), the release-notes reference's worked 0.4.1 example and versioned-URL rule (release bodies must pin), historical statements (README "removed in 0.5.0", `application-updates.md` 0.5.3 RC note, `release-packages.md` 0.4.0 tag history), the 0.5.0 promo script `docs/images/promo/betterpullreview-0.5.0-script.txt`, and third-party dependency URLs in `cmake/`. `docs/development/application-updates.md` already links `releases/latest`. No Homebrew, winget, appcast or website source is tracked in the repository | pass |
| BPR-2420:gate | `python3 tests/scripts/verify_documentation_tests.py`: 88 tests OK (six new). The new check run on `HEAD~1`'s README: `README.md:29 links one release (…/releases/download/0.4.2/BetterPullReview-0.4.2-build.60-macos-universal.dmg)`, exit 1 | pass |
| BPR-2420:release-flow | SKILL.md paragraph before "Verify URL, peeled tag commit…"; description unchanged, so the `.claude` pointer still matches | pass |
| BPR-2420:docs | `python3 scripts/verify_documentation.py --root .`: `DOCUMENTATION_OK` (worktree linked to the deployed implement-rfc skill; it has no MASK deployment of its own) | pass |

## Limits

- README no longer has one-click installer links. Asset names are
  `BetterPullReview-VERSION-build.N-<platform>.<ext>` on every 0.5.x release,
  so `releases/latest/download/<asset>` cannot reach them. One-click links need
  stable-name aliases uploaded with each stable release (for example
  `BetterPullReview-macos-universal.dmg`, `…-windows-x64.msi`,
  `…-linux-ubuntu-26.04-x64.deb` and the DEB's `.asc`), byte-identical to the
  canonical asset like the existing macOS legacy ZIP alias, and verified by
  `tools/publish-release.sh`, `packaging/verify_native_release.py` and the
  post-publication asset count. That touches the three release routes'
  asset-set verification and cannot be proven without a publication, so it was
  not done here; it is a follow-up for the owner to choose. If adopted, README
  rows could link `releases/latest/download/<alias>` and the gate already
  accepts that form.
- The Ubuntu signature link and Known limitations now follow `main`'s package
  guide rather than one release's copy; release bodies keep linking the
  tag-pinned guide.
- Not part of this card, noticed in passing: README says `gh` access on
  Windows and Linux writes unconfined, while `release-packages.md` says GitHub
  CLI support is macOS-only. Left as found.

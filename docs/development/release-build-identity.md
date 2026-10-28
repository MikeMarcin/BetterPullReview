# Release candidate build identity

Owner amendment, 2026-10-28; implementation tracked by [[BPR-1799]]. A build
number belongs to a BetterPullReview release candidate cohort, not a compiler
invocation or an ordinary local build. The marketing version remains the CMake
three-component version. Human text uses `0.4.1 (build 1)` as an example, not a
selection of the next release version.

## Existing contracts and compatibility

The existing build identity supplies Git commit/date/dirty state to About.
`CFBundleVersion` currently equals the marketing version. Windows PE uses
`major,minor,patch,0`; WiX uses the three-component marketing version. Native
manifests bind version and source but have no candidate cohort. BPR's shipped
updater reads GitHub releases directly; there is no Sparkle appcast path.
It derives the ZIP name from the base-version tag, rejects prereleases, ignores
SemVer metadata for precedence, and validates the staged bundle's short version
against that tag. Adding a tag suffix would break existing installed clients.

Keep stable tags `vMAJOR.MINOR.PATCH` and the bundle short version unchanged.
Canonical assets use `VERSION-build.N`; emit the legacy
`BetterPullReview-VERSION-macos-universal.zip` as an exact byte alias of the
numbered ZIP for already shipped updaters. The publisher/audit must compare
their digests and cohort rather than accept an unrelated second ZIP. A base
version can publish only one cohort: occupied tags/releases are never replaced.
The existing `v0.4.0` is occupied by immutable source
`144de719a8d972162f92ec04f98ba11293ae0a8f`. Choosing a new corrected semantic
version is a separate owner decision before publication.

The stable updater continues to order marketing versions, preserving its
persisted skipped-version contract. It checks a numbered staged bundle's
positive build identity where available; an older stable client can still
validate the base version and exact compatibility ZIP. No claim that SemVer
metadata changes precedence is introduced.

## Allocation and source binding

The publish-release preparation step owns allocation on clean main, after
product/configuration changes have been committed. Producers consume the
resulting cohort; they never allocate independently. A tracked product counter
and immutable numbered allocation receipts live under
`packaging/release-builds/`. A process-exclusive common-Git ledger reserves the
number durably before writing the tracked allocation, so an interrupted or
abandoned preparation cannot recycle it in another linked checkout. Historical
allocation paths on known refs set a high-water floor. The sole publisher must
reconcile/fetch its authoritative main before allocating on another clone;
offline competing allocators are not supported.

Each allocation binds the pre-allocation source commit/tree, marketing version
and a canonical JSON digest of the complete declared platform/preset/feature,
toolchain and dependency inputs. Local absolute paths and transient timestamps
are not candidate configuration. The allocation metadata is committed as its
own explicit slice. Freezing accepts only that commit over the recorded source
base with exactly its counter/receipt delta, then emits an external cohort
document containing the final source commit and allocation/input digest. This
avoids a self-referential commit hash. A retry at the same frozen source and
same declared inputs reuses the receipt; another source/configuration gets a
new number. Interrupted reservations are recovered by input identity, not by
guessing the next free integer.

All selected producers receive the same frozen cohort document. Configure,
packaging and publication verify its source/configuration binding. Changes to
tool-only release tooling require an explicit producer-versus-tool revision
bridge; they do not silently relabel compiled bytes. Ordinary configure/build
without a cohort document uses no release number and allocates nothing.

## Native mappings

| Surface | Numbered identity |
|---|---|
| Human About/copy/diagnostics | marketing version plus `build N`, source/configuration retained |
| macOS short version | `MAJOR.MINOR.PATCH` |
| macOS `CFBundleVersion` | positive decimal `N` |
| Windows PE numeric version | `MAJOR.MINOR.PATCH.N` |
| Windows MSI `ProductVersion` | `MAJOR.MINOR.N`; marketing version and cohort also bound in metadata |
| Debian package version | `MAJOR.MINOR.PATCH+build.N` |
| Canonical download/source asset | `VERSION-build.N` plus platform/variant |
| Stable tag / old-updater ZIP alias | existing base-version format |

Use an explicit common range `1..9999`, fail before allocation/build at
exhaustion, and never wrap. Major/minor must fit MSI's `0..255`; PE numeric
components must fit their native fields. MSI's fourth field is ignored, so
putting N there would not establish upgrade order. For the existing per-machine
`0.4.0` package, a next `0.4.x` candidate with positive N maps to `0.4.N`, which
is strictly newer; every later same-major/minor cohort increases N. Refuse a
marketing downgrade and verify the actual prior MSI version before publishing.
Preserve the existing UpgradeCode and BPR-1788 per-machine scope; this change
does not add a cross-scope migration.

Apple documents a one-to-three-integer build version; its archived reference
limits the first component to four digits. Microsoft limits MSI major/minor to
255 and build to 65535, and ignores a fourth component. Debian permits `+` in
the upstream version. These constraints motivate the bounded mappings above:
[Apple](https://developer.apple.com/documentation/bundleresources/information-property-list/cfbundleversion),
[Microsoft](https://learn.microsoft.com/windows/win32/msi/productversion),
[Debian](https://www.debian.org/doc/debian-policy/ch-controlfields.html#version).

## Verification and retained obligations

Exercise allocation, unchanged retry, changed-input allocation, concurrent
refusal, abandoned reservation recovery, history high water and source-delta
refusal in disposable Git repositories. Exercise the shortest CMake-to-About
and copied diagnostics composition with a numbered cohort and an ordinary
local build. Package/audit/publisher fixtures must reject mixed numbers,
configuration/source mismatches, missing aliases and changed alias bytes.
Native bundle/PE/MSI/DEB metadata must be inspected on their actual platforms.
This feature does not waive release qualification, source/legal, security,
signing, native trust or the owner-selected next semantic version.

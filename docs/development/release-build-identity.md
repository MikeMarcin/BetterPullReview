# Release candidate build identity

Owner amendment, 2026-10-28; implementation tracked by [[BPR-1799]]. A build
number belongs to a BetterPullReview release candidate cohort, not a compiler
invocation or an ordinary local build. The marketing version remains the CMake
three-component version. Human text uses `0.4.0 (build 1)` for the first numbered RC. The owner clarified
on 2026-10-29 UTC that several RCs may retain marketing version 0.4.0.
A patch bump is not a native prerequisite for another RC.

## Existing contracts and compatibility

The existing build identity supplies Git commit/date/dirty state to About.
`CFBundleVersion` currently equals the marketing version. Windows PE uses
`major,minor,patch,0`; WiX uses the three-component marketing version. Native
manifests bind version and source but have no candidate cohort. BPR's shipped
updater reads GitHub releases directly; there is no Sparkle appcast path.
It derives the ZIP name from the base-version tag, rejects prereleases, ignores
SemVer metadata for precedence, and validates the staged bundle's short version
against that tag. A build-tag stable release would break existing installed clients.

Keep stable tags `vMAJOR.MINOR.PATCH` and the bundle short version unchanged.
Canonical assets use `VERSION-build.N`; emit the legacy
`BetterPullReview-VERSION-macos-universal.zip` as an exact byte alias of the
numbered ZIP for already shipped updaters. The publisher/audit must compare
their digests and cohort rather than accept an unrelated second ZIP. A stable base-version tag can identify only one cohort; occupied tags/releases
are never replaced. Numbered RCs use distinct `vVERSION-build.N` tags and must
remain GitHub prereleases. Installed stable updaters already exclude prereleases,
so manually distributed RCs do not need their RC tag to pass stable staging.
Their corresponding-source URLs must name the actual RC tag. A build tag cannot
be promoted to stable; choosing a compatible future base-version stable route
is a separate owner decision.
The existing `v0.4.0` is occupied by immutable source
`144de719a8d972162f92ec04f98ba11293ae0a8f`. That preview remains unchanged while corrected 0.4.0 RCs use their own tags.
The withdrawn 0.4.1 proposal was not a human approval or a native requirement.

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
| RC tag | `vMAJOR.MINOR.PATCH-build.N`, prerelease only |
| Future stable tag / old-updater ZIP alias | existing base-version format; separate stable policy |

Use an explicit common range `1..9999`, fail before allocation/build at
exhaustion, and never wrap. Major/minor must fit MSI's `0..255`; PE numeric
components must fit their native fields. MSI's fourth field is ignored, so
putting N there would not establish upgrade order. For the existing per-machine
`0.4.0` package, a next `0.4.x` candidate with positive N maps to `0.4.N`, which
is strictly newer; every later same-major/minor cohort increases N. Refuse a
marketing downgrade and verify the actual prior MSI version before publishing.
A committed authoritative native/publication floor binds the prior MSI identity
and digest. Inputs cannot choose an older baseline. If legacy 0.4.7 would map
a fresh N=1 to MSI 0.4.1, allocation fails before reservation; no clamping or
number reuse is allowed. Recovery requires reconciling the durable counter/history
against authoritative observations. Global N continues across patch versions:
0.4.0/build 1 maps to MSI 0.4.1, then 0.4.1/build 2 maps to MSI 0.4.2.
The numbered MSI ProductCode is a deterministic UUID v5 of the product RC tag:
unchanged cohort retries preserve it, and another cohort has another ProductCode,
even when the marketing version is unchanged. The producer reads actual prior
and new ProductCodes and refuses a collision; manifests retain both.
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
signing, native trust or the separate future stable release policy.

## Preparation commands and declared inputs

Only the publisher controller allocates, on reconciled clean `main`. No CMake
preset, compiler invocation or package driver advances the counter. Supply an
external `bpr.release-inputs.v1` JSON object with `producers` and
`previousWindows`. Each named producer declares its platform (`Darwin`,
`Windows` or `Linux`), architecture array, preset name, single `buildType`,
`toolchain` (`cmake`, `compilerId`, `compilerVersion`, `qtVersion`) and `options`.
The options explicitly name `BUILD_TESTING`, `BPR_DISTRIBUTION_BUILD`,
`BPR_BUILD_QT`, `BPR_BUILD_TOUR_CLI`, `BPR_BUILD_AGENT_INTEGRATIONS`,
`BPR_BUILD_LOCAL_AUTOMATION` and `BPR_EXTERNAL_QT_RESOURCES`, each `ON` or `OFF`.
Include every other effective `BPR_*` Boolean and the `settings` object from
`_bpr/release-observed.json` in each native preconfigured graph. That observation
also supplies the actual compiler/linker flags, generator, runtime and SDK
selection, the compiler image digest and the selected Apple/Windows SDK version
(including the compiler-default SDK); changing those settings requires a new cohort. Observation itself
allocates nothing.
`previousWindows` holds the previous `marketingVersion` and actual
`msiProductVersion` when Windows is selected; otherwise it may be null. These
values must equal the committed `packaging/release-builds/windows-upgrade-floor.json`.
That reviewed record names the published release ID/tag/source, installer and
SHA-256, bootstrapped from the verified v0.4.0 per-machine MSI. Known Git history
prevents rolling that floor backwards or replacing the same published identity.
Before a later allocation, advance the floor only from an actual native and
published observation; an unknown prior identity fails closed. Packaging requires
the exact prior MSI digest as well as its per-machine metadata, before building
or signing. The
source tree binds every tracked build/dependency definition; the receipt also
records exact SHA-256 values for the preset and dependency inventory.

Run `tools/publish-release.sh --prepare-candidate --candidate-inputs
/absolute/inputs.json --cohort-output /absolute/allocation.json`. Commit exactly
the reported counter and allocation receipt over the recorded base. Then run
`tools/publish-release.sh --freeze-candidate N --cohort-output
/absolute/cohort.json`. Preserve that external cohort file with the request's
native evidence and give identical bytes to all producers. An interrupted
reservation is recovered by repeating preparation with the same inputs. Do not
remove the common-Git reservation journal to reclaim a number.

Default artifact directories include `VERSION-build.N`, as do the macOS
producer build directories. A numbered production retry refuses an existing
output directory and preserves its bytes; audit/verify the existing result or
select a fresh directory. It never removes assets from a prior numbered cohort
or the unnumbered preview directory.

Configure each producer with `-DBPR_RELEASE_COHORT=/absolute/cohort.json` and
`-DBPR_RELEASE_PRODUCER=its-declared-name`. Configure checks the actual compiler,
CMake/Qt versions, architectures, configuration and feature flags; every build
rechecks the frozen source before compiling its identity. Ordinary local builds
leave both settings empty and remain unnumbered. Multi-configuration generators
must select exactly one configuration for a numbered producer.

Initial numbered RC publication requires `--cohort /absolute/cohort.json` and
`--prerelease`; it derives the distinct RC tag from that cohort. No marketing
version bump is required for another RC. The occupied
unnumbered preview retains its separately guarded append/promote path and its
original source and bytes; it never acquires a retroactive build number.

## Stable-copy operation

The original `v0.4.0` preview cannot be replaced. A separately authorized
`tools/publish-release.sh --copy-stable` operation can copy one complete numbered
RC to a new bare marketing-version tag, such as `0.4.0`, at the exact producer
source. It uses the existing publisher and strict three-path publisher/test
revision bridge. This prepared route does not promote or retag the RC and does
not itself supply publication authorization.

Supply the same cohort, source, full native manifests, Mac directory and RC
baseline as the append route. `--dry-run` verifies the selected bytes and exact
source RC, refuses an occupied local/remote target, and reports missing RC
assets and missing acceptance/authority without mutation. A real copy requires
separately bound `--stable-acceptance` and `--publication-authorization` records:
source/version/tag, source-RC tag, cohort digest, exact asset inventory, evidence
digests, accepted status and an empty blocking list must match.

Installed source offers continue to name the RC. Append and verify any missing
source-RC files through the existing guarded append operation first; never
rewrite those offers or repackage frozen bytes. The copy path refuses an
incomplete source RC, creates an annotated tag at the producer commit and a
draft prerelease outside Latest, uploads without replacement, and verifies the
complete set before its final stable/Latest edit. Uncertain or occupied outcomes
require inspection rather than automatic retry. Verify anonymous downloads and
native trust afterward. Base-version precedence remains unchanged: installed
0.3.4 can update to 0.4.0, while an existing 0.4.0 preview requires manual
installation of another 0.4.0 cohort.

GitHub's tag lookup does not return draft releases. The publisher discovers a
new draft through the complete release list, then checks its numeric release
ID. After an inspected partial result, `--resume-stable /absolute/checkpoint.json`
requires `bpr.release-stable-copy-resume.v1`: exact source/version/tag/sourceTag,
cohort digest, numeric releaseId, annotated tagObject and the observed existing
asset inventory. It refuses changed draft state, notes, source, tag or assets,
creates no new tag/release and never replaces a file. This checkpoint supplies
no qualification or publication authority; both bound acceptance records remain
mandatory. The [0.4.0 result](../verification/release-v040-results.md) records the
actual recovery and remaining stable native audits.

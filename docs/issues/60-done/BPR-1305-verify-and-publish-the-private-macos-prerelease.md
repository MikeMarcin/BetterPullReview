---
schema: "implement-rfc.card.v1"
id: "BPR-1305"
title: "Verify and publish the private macOS 0.1.4 prerelease"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
metadata: {"platforms":["mac"],"owner":"macos-prerelease","scope":"Owner-authorized private prerelease; no RFC-0038 closeout"}
---

The owner requested publication and selected 0.1.4 on 2026-10-07. Use the
existing publish-release skill and scripts, with tests, signing and notarization.
Keep the release private and marked prerelease; stable Latest remains v0.1.3.

## Acceptance

- The complete development test gate passes on the committed release source.
- The universal app and DMG pass runtime, source-binding, Developer ID,
  notarization, stapling, Gatekeeper and checksum verification.
- The private GitHub prerelease has its exact tag and all eight assets;
  downloaded checksums and corresponding-source binding verify.

## Gate corrections and review

`84cb3611f` prepares the version, release notes and publisher prerelease option.
`29a55ef70` corrects only the tests-on selected-diff capture and its geometry
course to include the already accepted Hide Drawers control from BPR-1295.
The exact-order check includes all 17 actions and platform offsets; the same
110-pixel right-edge bound starts after the last drawer control. The focused
course passes all appearance profiles (18.77 seconds). Formatting passes.

Reviewed pinned-source range `efa26ca02..b81a1075c`: `29a55ef70` and
`da862cf32` change
the pinned application source. Its full diff touches tests-on capture facts,
indices, comments and geometry observations. No production effect, target,
provider, credential, grant, publication or storage behavior changes; the
mutation branch digest stays `e067befefeff198fdbd992259b2ee8ea105520d50ddf7125e3b091519cb8a5df`.
`da862cf32` changes only the tests-on label course: reopen the fixture file
through the files drawer after refresh, wait for its current comparison, and
await completed draft and refresh transitions before triggering publication.
The exact body, anchor, native-attempt count, refusal, text retention and mark
assertions remain. The complete label course passes in 4.84 seconds. No
production behavior or mutation branch changes. The reviewed full-application
and source-manifest pins are resealed accordingly.

Integrated BPR-1304's committed menu repair (local `69a2c001c`, upstream
`3ef2ce429`) and its stronger comparisons. The parked card retains the owner's
native-session evidence requirement; the release gate is not owner UX approval.
Merged upstream through `f750aec6b`, preserving its Windows isolation metadata.
The duplicate ready-card copy from the cherry-pick is removed in favor of the
upstream parked card; the issue counter retains the allocated BPR-1305 receipt.

The first capsule course saw an unmounted residual disk device; its ordinary
cleanup removed it, and the unchanged focused course passed in 73.73 seconds.
The mutation-isolation scan exceeded its existing 300-second bound while
demangling every symbol table. Native decorated names already carry both
checked identifiers literally. The correction retains complete native symbol
tables, every binary, failure on unreadable tables and the positive control;
it removes only the redundant demangler pass. Hostile decorated-symbol and
unreadable-table controls cover this change. No timeout or exception is widened.

Focused mutation-isolation evidence passes for all 347 binaries in 104.65
seconds; its 26 script controls pass. Healthy HTTPS fixture deadlines now allow
two seconds per connection (eight for the redirect loop), while the dedicated
timeout case keeps 500 ms; the complete transport course passes in 21.89 seconds.
The agent finding course waits for the asynchronously presented Read action
within its existing course budget and passes in 10.97 seconds. Both profile
courses include Hide Drawers in the resident View menu, preserving the disabled
action, shortcut, ordering and Preferences-role assertions; focused public-path
and Qt courses pass in 7.26 and 1.03 seconds respectively.

Upstream then landed BPR-1303's product correction at `bba8a8d26`, through
`fd6c21b36`. Merge `0a3cf40ee` incorporates it and removes the test probe's
manual file reopening, so the course exercises the restored active tab itself.
Reviewed pinned-source range is now `efa26ca02..0a3cf40ee`: `29a55ef70`,
`da862cf32`, `bba8a8d26` and this integration. The new production code records
a handoff that preserves the diff and refreshes the exact active tab through
the existing document-tab host once files return. Head-moving refresh retains
its reload. No provider, target, grant, credential, publication or storage path
changes; mutation digest remains unchanged. BPR-1303 records forced-off failing
controls and 20 passing label runs. The label probe retains current-comparison,
completed private-draft and refresh readiness checks and every exact assertion.

The `4a7ac4456` full gate had no failures through more than 500 courses,
including the full hostile composition control, capsule cleanup, label course,
347-binary isolation, HTTPS and corrected profile courses. It was interrupted
before packaging to integrate this upstream production fix. A fresh normal
release workflow verifies the combined committed source.

M8/M9 native, vault, crash/privacy and vulnerability gaps remain open as stated
in the release notes. This prerelease supplies no successor RFC closeout.

The combined `52d5a4866` gate completed 628 courses with six failures and the
configured Debug cache-bytes skip. Four failures were the unchanged 100 ms
large-review input bound; the native empty-menu course compared the Home bar's
visible subset against the resident bar's complete View commands; the rate
course declared recovery quiet before the re-presented tab completed.

The native test now derives the resident View order from every declared window
command, including Hide Drawers hidden on Home, and still compares the other
native menus, Preferences merging, disabled commands, close gap and reopen.
Its focused course passes (7.25 seconds); native owner qualification stays parked.

The rate course includes queued tab presentation in its existing readiness wait.
That exposed the actual same-head reading regression: tab restoration cleared
and reloaded the retained diff under the new file snapshot. The bounded kept-tab
path now retains presentation while the actor resolves the new request, then
requires the complete published material key unchanged. Application session,
PR authority, comparison, options, algorithm and file entry are checked;
changed material keeps the ordinary clear/reload. Tab readiness requires the
current workbench generation and exact published request, and kept publication
queues its driver. An already exact selection is not revealed a second time,
so a viewport scrolled independently of that line stays put. Finishing or
abandoning the tab clears the revalidation marker. This changes no provider,
credential, target, grant, mutation, storage or source-identity admission path.
The old path failed the stronger course (scroll 33333 to 384); premature
completion lost selection, and omitting driver notification failed readiness.
The corrected complete course passes (22.01 seconds), including three wire
refusals, cooldowns, explicit recovery and sibling cancellation with exact
comparison, line 100 and scroll retained. Exact assertions and budgets remain.

Profiling the benchmark's own process showed hidden conversation-card text
measurement during Overview publication. Card data, counts and accessibility
stay synchronous; a hidden drawer under an already visible window measures
when shown. Standalone/pre-show geometry keeps its synchronous contract.
No card wording, styling or layout algorithm changes. The initial general
hidden deferral failed the excerpt/font unit course and was narrowed accordingly.
The card and panel courses now pass (1.18 and 0.72 seconds), responsiveness and
launch pass at unchanged budgets (9.04 and 7.82 seconds); active/pathological
courses also pass (5.53 and 3.07 seconds). The picker experiment was removed.
The complete diff-workbench course passes at all three scales (2.57–2.85 seconds).

Reviewed the complete source delta `52d5a4866..7289987eb` for these gate repairs.
Only `github_application.cpp` and its tests-only fixture evidence header affect
the pinned production app manifest. The ordinary mutation branch is unchanged.
The retained presentation cannot admit publication: the existing generation,
selected-file and epoch-context checks still precede the accepted actor result,
and its full material key must match before the retained view is adopted.
This is engineering self-review of the release slice, not native owner approval.


The authoritative `f3e866e9b` workflow completed 628 tests with zero failures
and one configured Debug cache-bytes skip (1001.16 seconds), then built and
verified the universal tests-disabled app, restricted runtime, notices, both
architecture slices, macOS floor, Developer ID/hardened-runtime signatures and
relocated launch. Apple submission stopped before upload because the default
notary profile was absent; no tag or release was created.

The owner supplied a verified replacement profile in the existing login
Keychain and authorized release-fix commits and publication. The reviewed
release-script correction adds an optional explicit Keychain selector to both
app and DMG submissions. Default lookup, signing, wait, staple, Gatekeeper,
clean-tree/test gates and private publication guards remain unchanged. Durable
command tests execute the actual option parser and submission helper, checking
complete argv for both artifacts, spaces, environment/CLI precedence, refusal
of missing/empty arguments, and stop on a failed submission.

The managed main checkout has a pending station transfer and remains untouched.
The same release owner continues BPR-1305 from an independent unbound checkout
of `f3e866e9b`, with ordinary main-branch publication and a fresh normal release
workflow. Its station status and hook permit local work; no stop observation is
fabricated and the managed checkout's ownership guard stays intact. Final source
provenance and annotated tag must use the new committed producer, not `f3e866e9b`.

Bash syntax, all ten source/publication/notary command tests, the documentation
gate and whitespace checks pass for the explicit-Keychain slice.


## Published and verified

Published [BetterPullReview v0.1.4](https://github.com/MikeMarcin/BetterPullReview/releases/tag/v0.1.4)
on 2026-10-07 through the normal release and publication scripts. Producer
and peeled annotated tag are `ab0d3b8626fd67561538190ed4e34f5c5731f395`.
The complete fresh development gate passed: 627 passing tests, zero failures,
and the one configured Debug cache-bytes skip, 1131.02 seconds total. No
`--skip-tests`, `--skip-notarize` or `--allow-dirty` option was used.

The universal tests-disabled build passed restricted-runtime, architecture,
macOS-floor, notices/source, relocated-launch and Developer ID/hardened-runtime
verification. Apple accepted app submission `04a7a4c5-4a88-4946-87ed-91d36edf1858`
and DMG submission `84e81593-6b7c-4027-98af-d0412860825d`. Both staple and validate
actions passed; both Gatekeeper assessments reported Notarized Developer ID.

Private preflight and atomic main/tag publication passed. The release is neither
draft nor stable: GitHub reports prerelease, repository PRIVATE, and Latest
remains v0.1.3. All eight assets were downloaded and compared byte-for-byte by
SHA-256 with the verified producer outputs and GitHub's asset digests. The four
product checksum files and exact version/source-revision binding passed again
on the downloaded app ZIP, SPDX and corresponding-source ZIP. Installed source
references select the separate versioned source asset, digest
`fdf51558c58f302c0f65a72f6f42699dc33d84b505e3dc18bb504d361ed7d156`.
The published release notes match the committed notes.

This closes BPR-1305's private prerelease scope. BPR-1304's native owner review
stays parked, and RFC-BPR-0038/M8/M9 qualification gaps remain explicit in the
release notes. Source-recipient access and indefinite retention obligations
remain in force; offline binding is not legal or release-qualification approval.
The managed checkout's station-transfer state was not changed.

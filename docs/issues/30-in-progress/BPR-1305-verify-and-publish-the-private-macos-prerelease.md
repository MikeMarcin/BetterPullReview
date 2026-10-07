---
schema: "implement-rfc.card.v1"
id: "BPR-1305"
title: "Verify and publish the private macOS 0.1.4 prerelease"
status: "In Progress"
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

M8/M9 native, vault, crash/privacy and vulnerability gaps remain open as stated
in the release notes. This prerelease supplies no successor RFC closeout.

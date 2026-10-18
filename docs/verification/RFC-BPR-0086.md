# RFC-BPR-0086 verification

Mandate `insights-window-v1-r1`. Immutable RFC_BASE_SHA:
`0378ab977a7ffc74e0ac5ac2f70ee37a9cf00a66`.

The authored plan’s first milestone is `window-and-storage` (BPR-1606 and
BPR-1607). The joint candidate implements both; its owner look is pending.
No later milestone or full-RFC acceptance is claimed.

| Criterion | Evidence and current result |
|---|---|
| AC01 window | Public menu and central shortcut open the profile singleton. Real composition supplies Local data and Background work. All six tabs, their ranges and three refreshes preserve content and frame; smaller-screen bounds, Escape, hidden timers, existing cache controls and shutdown are checked by `application_insights_public_path`. Owner acceptance remains pending. |
| AC05 storage | `sqlite_insights_storage` compares exact measured bytes with an independent filesystem walk, includes WAL and SHM, checks the three fixed store budgets and privacy canaries. `git_insights_storage` uses the shared cache inventory without any Git executable, and rejects linked entries. Native maintenance tests verify actual frozen export and purge. |
| AC11 first-milestone portion | The real maintenance preview includes only last measured numeric byte totals for anonymized export; identifying export gains no Insights section. Private store classification and narrow reset survival are checked. Full export privacy with later statistics remains a closeout obligation. |
| AC02–04, AC06–10, AC12 | Pending their authored milestones. |

## Engineering review

Scoped self-review over the immutable baseline through the first-milestone
candidate covered all changed source. Core adds only the statistics reset class;
the host owns serialized effects, one in-flight snapshot and the storage throttle;
Qt holds projection values and gestures. Initialization and measurements complete
on the decision lane before profile teardown. Shared Git inventory extraction
preserves its existing validation, cancellation, quota and pack inspection.
No provider, credential, mutation admission or live-GitHub behavior changed.
The composition contract pins are resealed for these exact app changes; the
production mutation-branch digest remains unchanged. This is an engineering
self-review, not independent or owner acceptance.

`frontend-design` was read at
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`.
Native profile typography and palette, a compact tab strip and a bounded file-byte
ledger were chosen for this developer tool. Offscreen Light/Dark full-window
renders were inspected for clipping and hierarchy; they do not establish native
system appearance acceptance.

## Verification frontier

- Cold developer app build succeeds with warnings as errors.
- Host throttle/lifecycle, exact storage, native maintenance export/purge, core
  reset and native reset scopes pass. The integrated public path passes.
- Clang-format 22 applied to touched C++; LLVM 22 tidy passes new host, SQLite
  adapter and window sources.
- The tests-disabled composition compiles in a focused syntax check with
  `BPR_BUILD_TESTING` and the native HTTPS test define removed.
- Documentation gate and its 67 regression tests pass.
- Architecture scan and its negative-control suite pass (228 seconds for the
  latter). The network-isolation manifest and the composed review round-trip
  contract pass. The concrete composition-selection source test passes; the
  full tests-disabled release graph suite was not repeated for this milestone.

The first run caught two real failures: lambda shadowing under warnings as
errors, and using a multi-statement batch with the SQLite helper that deliberately
accepts one statement. The public-path test failed on the latter missing snapshot.
The schema now uses individual statements inside one explicit transaction, and
both the storage test and the composed public path pass.

## Limits and continuation

These are file-length snapshots of active local stores, not a transaction over
all filesystem writers. Refused or changing entries are unavailable. Old caches
without a remembered display label show an abbreviated cache identity until an
accepted read names them. Windows and Linux native UI and containment for the new
public course are unmeasured. No idle-hour or full-pack closeout claim is made.
The live continuation keeps the exact candidate, attended session and cursor.

The manifest initially refused an unbuilt gh transport helper in the cold tree;
its prescribed target was built, and the unchanged manifest then passed.

A final lifetime audit moved Insights initialization into the existing
post-construction `start_services` phase. A constructor that subsequently
refused the profile could otherwise destroy its decision executor before the
new task posted its terminal. The corrected candidate reuses the public startup
recovery course as well as the Insights path; the first frozen build was
superseded before preparation or owner review.

The corrected source is `5a02177b30250494811033052b673c20a9b5a7b0`.
Its final quiescent developer build and three affected public courses pass:
`application_profile_recovery_public_path`, `application_insights_public_path`
and `application_review_round_trip_composition_contract` (11.21 seconds total).
Unchanged passing evidence above is reused. The earlier frozen root was removed
with the freeze tool; the replacement and its own launcher check are recorded in
[the continuation](../issues/_checkpoints/insights-window-v1/continuation.md).

## Owner rejection and correction, round 2

The owner rejected source `5a02177b30250494811033052b673c20a9b5a7b0` in
session `a25204cd201da1ca5dbff4439c52bda4`, events 6–7: “that looks almost
nothing like the mockups /Users/jaeger/Downloads/bpr_insights_window_fixed_size_mockup.html”.
That verdict remains rejected. Only its affected protection was released by
`resume-development` (event 8). The later milestones remain ineligible.

The correction is source `ae0a40211`. The exact HTML was read and rendered in
disposable headless Chrome with remote resources blocked. No owner browser
profile or desktop was read or captured. The `frontend-design` skill named
above was loaded before design work. Its plan follows the supplied reference:
900 × 542 content, a 52 px centered tab/action strip, a fixed title/status
header, 16 px margins, 8 px card gaps, a 90 px budget card and 78 px tiles.
Native profile font and palette supply the colors and typography; the HTML's
example data is replaced with real storage values and explicit empty states.
There are no invented counts, charts or acceptance of later milestones.

The Qt correction replaces stretched group boxes with compact rounded cards,
keeps the header above the page stack, uses native keyboard-addressable tab
bars for tabs and ranges, and presents Storage as thin byte/budget rows with
meters. The two icon actions retain their full accessible names and existing
host callbacks. The Storage actions retain the existing cache controls and
Reveal data folder. No new effect, store, process, network, deletion or export
policy is introduced; architecture and lifetime semantics are unchanged.

Focused checks pass: `application_insights_public_path`,
`application_review_round_trip_composition_contract` and
`architecture_boundaries` (20.58 seconds together). The public course checks
all tabs and ranges through three refreshes, an unchanged title/status header,
the clamped smaller layout, real bytes, export preview, cache controls,
Escape, hidden timers and shutdown. All six pages were rendered in Light and
Dark at the exact reference content size and inspected. The first added test
assertion incorrectly expected an unclamped width on the offscreen screen;
it was corrected to check both the reference size and the actual screen clamp.
LLVM 22 tidy and the documentation gate pass. Public courses also pass at
1.25× and 2×, with inspected rasters and recorded platform context. Prior
unaffected storage, reset, lifecycle, privacy,
network manifest and architecture negative-control evidence is reused.

The corrected shell needs a new owner Accept/Reject verdict. The continuation
binds its frozen build, short session, fingerprint, commands and cursor. No
owner acceptance, native Windows/Linux evidence or whole-pack closeout is
claimed by these offscreen checks.

The required freeze completed for `ae0a40211`; its own launcher check passes.
The exact frozen public executable also passes via the registered isolation
wrapper. CTest's initial attempt was refused because it writes its temporary
log inside the deliberately read-only tree; the equivalent wrapper invocation
from writable evidence passed without changing that tree. Frozen captures,
source/executable identities and the new short-session handoff are recorded
in the continuation. The original rejection remains preserved.

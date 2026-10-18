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

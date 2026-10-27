---
schema: "implement-rfc.card.v1"
id: "BPR-0593"
title: "Verify and close RFC-BPR-0018"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592","BPR-0678","BPR-0679","BPR-0680","BPR-0681","BPR-0682","BPR-0683","BPR-0684","BPR-0685","BPR-0686","BPR-0687","BPR-0688","BPR-0689","BPR-0730","BPR-0731"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0018"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"mandate_revision":"r2","deferred_followups":["BPR-0974","BPR-0975"]}
acceptance: [{"criterion":"AC-01: The shipped application exposes the profile-identified singleton Settings window with explicit per-workbench Ctrl/Command-comma binding, verified by the composed profile and four-scale key courses and the complete macOS native menu/shortcut/window-lifetime course; remaining Windows and Linux native courses are preserved as unaccepted standalone BPR-0974 and BPR-0975.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Pass for the amended scope: explicit binding passes four scales, prior actual profile course passes (78.15 s), and current Mac tests-OFF native course passes all 11 observations and exits 0. Windows' corrected prefix before quarantine is not its full course; Linux full focus/lifetime still fails. Those unchanged obligations remain Blocked on BPR-0974/0975, not passing evidence. Exact failures and isolation remain in the [continuation report](../../verification/BPR-0593-native-settings-continuation.md). Producing evidence: 68e0a40f, a881d9f9, BPR-0705; prior 138dd1d9; product 2d3bdbdf; observer/test upkeep e7843405; owner r2 amendment in this slice. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-02: All seven initial categories are present when their capabilities are composed, with stable sidebar/page/row identities and truthful unavailable states.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Pass for the seven fixed categories and truthful unavailable rows. Current four-scale Settings course passes; later General updater is owned by RFC-0035 and identified as a platform capability. Producing evidence: 51c24f6b, 6c0ba02b; 19a3dfd7 r1 rerun. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-03: Appearance is migrated from the embedded workbench surface to the settings center with no duplicate authority; existing mode, typography, palette, import/preview/commit/cancel/reset, and persistence behavior remains intact.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Pass. Required public import/preview/cancel/commit/export/restart/FIFO course passes after its restart uses the existing composition-only probe instead of unrelated obsolete CLI setup. Every original palette, zero-write, persistence and comparison assertion remains; final contract 24.09 s. Producing evidence: 03a8a02e, a881d9f9, ed2b3f14; product 19a3dfd7; test upkeep 138dd1d9. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-04: Every setting gesture becomes one typed domain intent, reaches only its owning host, and updates the control only from an accepted authoritative projection.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Focused paths pass and existing medium-review disposition is retained. Broader `qt_composed_github_scale_100` still stops at the existing BPR-0940 CLI-card assertion; the rest of that suite is not accepted by this rerun. Producing evidence: dd64c098, 51c24f6b, a881d9f9; 19a3dfd7 r1 rerun. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-05: Existing domain stores migrate and reopen from prior schemas; future, malformed, interrupted, and unavailable cases fail at their exact scope without corrupting another domain.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Pass for executed migration/reopen/future/malformed isolation, saved-server persistence and exact stored-palette restart. BPR-0686 pre-write validation and RFC-0034 per-theme migration retained. The unrelated CLI entry no longer prevents the required appearance persistence check. Producing evidence: 03a8a02e, dd64c098, 22e30024, 867e4bed; product 19a3dfd7; test upkeep 138dd1d9. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-06: Credential, privacy, cache, notification, and agent actions retain their existing capability and secrecy boundaries.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Existing capability/secrecy acceptance retained; both architecture gates and Settings effect instrumentation pass at the current source. No live account, credential, GitHub or GHES mutation was used. Producing evidence: 01a3ada4, dd64c098, 51c24f6b; 19a3dfd7 r1 rerun. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-07: Keyboard-only and screen-reader courses reach every sidebar item, row control, reset, confirmation, status, and close action in a natural order.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Accepted: BPR-0683 v006 decision f2fb9b06 covers the original Settings walk. Later Accounts/Home VoiceOver 506a1c44 and Appearance VoiceOver 6825fbc6 plus native picker 1b80dbd8 cover their revised pages at ed2b3f14. Current Settings keyboard checks pass. These decisions do not certify later RFC-0035 update controls. Producing evidence: d1ba791f (cc2afa7f bundle), ed2b3f14 (d500ef7a bundle); 19a3dfd7 r1 rerun. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-08: Layout and rendered evidence pass at 100%, 125%, 150%, and 200% scale in Light, Dark, High-contrast Light, and High-contrast Dark, including long text and minimum window size.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Current 112 rendered layout combinations pass. Original native Settings visual verdict f2fb9b06 is accepted; revised Accounts d0bb147d and Appearance c5349059 verdicts supplement their changed pages. Deterministic offscreen geometry is not a new native or three-platform visual verdict. Producing evidence: 6c0ba02b, d1ba791f, ed2b3f14; 19a3dfd7 r1 rerun. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-09: One-setting updates preserve sidebar selection, page scroll position, focus, unaffected persistent indexes, and all unrelated projections without routine resets.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Pass for Settings reopen, one-setting preservation and current workbench composition. Profile preferences and per-window layout retain their separate owners under RFC-0024. Producing evidence: 68e0a40f, dd64c098, a881d9f9; 19a3dfd7 r1 rerun. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-10: Hostile catalog, profile, rule, and row floods stop at declared limits with typed refusal; one window materializes at most one page per category; rapid previews coalesce; cancelled or superseded loads publish nothing late; and instrumentation observes zero database, vault, filesystem, network, process, or blocking-wait work on the Qt thread.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Pass for declared collection limits, one page per category, coalesced preview and cancellation. Current recorded capability ports execute on worker threads and publish on the decision lane. This is the specified focused instrumentation; exhaustive native syscall/chooser/vault tracing was never supplied or claimed. Producing evidence: 143abbc9, e354b45e, 29bb2980, a881d9f9, 6c0ba02b; 19a3dfd7 r1 rerun. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-11: The recorded tests-disabled macOS, Windows and Linux baseline inventories match except for documented platform capability rows, and the current macOS/Linux inventories refresh later updater, CLI and maintenance rows; the current Windows refresh remains unaccepted on standalone BPR-0974.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Pass for the amended scope: e93d9215 Mac/Windows and 9bbe3604 Linux seven-page baseline matches; 19a3dfd7 Mac dev/release equality retained. Current 2d3bdbdf Mac/Linux inventories match six pages and differ only by the explicit macOS-only CLI entry; updater and seven RFC-0014 maintenance rows are refreshed. Eleven comparison tests pass. Linux supplies the same Release/tests-OFF graph inventory scope accepted on BPR-0731, not full distribution qualification. Current Windows refresh is unaccepted on Blocked BPR-0974 due to Defender, not inferred from the old baseline. Producing evidence: d1ba791f; e93d9215/fc77eac3; 9bbe3604/987d7766; prior 138dd1d9; product 2d3bdbdf; evidence e7843405; scope clarification 56458c84; owner r2 amendment in this slice. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-12: Sign in starts supported device authorization directly and shows its code and browser action without Details, another Get code gesture, or pane scrolling; cancellation, expiry, and supported manual-token alternatives remain reachable.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Supported direct code/browser/cancel/expiry/manual flow evidence retained. Later onboarding uses the dedicated shared dialog; the old inline-row description is superseded, not a reason to restore a second code gesture. Broader legacy CLI assertions remain BPR-0940. Producing evidence: 01a3ada4, 1210eecc; accepted Accounts revision ed2b3f14. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-13: User-configured servers and account selections survive restart through typed host persistence, while remembered grants reuse the existing vault path and session-only manual credentials are never presented as durable.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Current four-process saved-server course passes with exact hostname/account metadata kept and offered after reopen. Existing device vault tests retain grant behavior; manual tokens remain session-only. No production credential was used. Producing evidence: 01a3ada4; 19a3dfd7 r1 rerun (92.67 s). Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}},{"criterion":"AC-14: Configuration leaves the primary review surface compact, preserves tuned diff/appearance behavior, and exposes direct reconnect plus an exact pending destination handoff without changing credential or provider authority.","outcome":"pass","revision":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","evidence":{"kind":"artifact","detail":"Owner-amended r2 scope only; amendment eadf48af, claim 95e64b4c. Current workbench (20.90 s) and full appearance customization/restart compositions pass. BPR-0689 exact destination/cancel/stale-stage acceptance and subsequent dedicated sign-in flow are retained. This continuation corrects only Settings shortcut binding/context; it changes no credential/provider authority, stores or compact review layout. Producing evidence: 554564f8, 01a3ada4, a881d9f9; prior 138dd1d9; current product 2d3bdbdf and Qt 6.11 test-call upkeep. Current r2 focused selection 6/6, integration 1/1, inventory 11/11; original native/human candidates remain exact. BPR-0974/0975 remain Blocked standalone follow-ups, not passing observations.","path":"docs/issues/60-done/BPR-0593-close-rfc-0018.md#closeout-checklist"}}]
---

# Verify and close RFC-BPR-0018

## Post-merge check on main, 2026-09-22

`codex/rfc-0018-0026-closeout` merged into main at `6a897af8` (parents
`5b14d7e8`, `317f0023`). Rechecking this closeout's recorded evidence on the
merged head, without re-adjudicating acceptance:

- Holds: the six focused Settings rows (`qt_settings_window` at 100/125/150/200,
  `qt_settings_effect_boundary`,
  `application_appearance_theme_customization_contract` 26.26 s) and
  `settings_inventory_comparison_tests.py` (11 tests) pass at `6a897af8`
  (`build/merge-0018-0026-integration.log`,
  `build/merge-0018-0026-settings-inventory.log`).
- Went red after this closeout: the integration row
  `application_workbench_composition_contract`, recorded 1/1 at `95e64b4c`,
  failed at the branch tip and the merge. A bisect puts it at `9210330a`
  ([[BPR-0977]]): the probe compared a layout that now carries the window's
  observed geometry. [[BPR-0984]] corrected the probe at `827ca5f6`, where the
  row passes again (22.79 s). No Settings behavior changed.

RFC-BPR-0018 stays Auto-Verified on the unchanged r2 scope; BPR-0974/0975
remain Blocked.

## Current owner-amended checkpoint r2, 2026-09-21

Run `rfc-0018-0026-closeout-20260920`, segment `rfc-0018`, mandate **r2**;
sole non-delegating owner `/root/rfc0018`, milestone `close-rfc-0018`, first of
the ordered 0018 → 0024 → 0025 → 0026 pack in the 0152 worktree on
`codex/rfc-0018-0026-closeout`. The
[owner amendment](../../rfcs/plans/ux-redesign-pack-0028/owner-closeout-r2.md)
explicitly changes terminal acceptance, separately from the observations below.
It supersedes prior blockers only by transferring their unchanged obligations
to open standalone cards; it does not turn a failed course into a pass.

**Disposition: milestone complete for the owner-amended r2 scope.**
RFC-BPR-0018 and its canonical index are Auto-Verified; this card is finished
through the shared helper at `be51ba9269d90c1ac308886d14f514775757d6f1` after
claim `95e64b4c`. The accepted amendment and
standalone follow-ups are committed at `eadf48af`. No product or test bytes
change in this r2 slice. This is not whole-pack or release acceptance.

BPR-0974 is Blocked on
`security-owner-disposition` for the exact Windows Defender finding, cleared
candidate, full native course and current inventory. BPR-0975 is Blocked on
`native-focus-surface` for the Linux focus/lifetime course. Neither is Done or a
required dependency of this amended closeout. Both preserve source/digests,
failed observations, precise prerequisites, guarded ready commands and terminal
acceptance. The original native criteria remain in the RFC.

Twenty other RFC-0018 cards remain Done. RFC-0014/BPR-0965 is accepted at
`b7eb3722`/`61907e20`; no release-security acceptance transfers from that
foundation. This r2 slice changes no product or test source. It reuses
`2d3bdbdf`'s corrected native Settings action and `e7843405`'s test/observer
upkeep, current Mac native course and Mac/Linux captures, plus earlier exact
three-platform baseline and human evidence. `56458c84` preserves the accepted
Linux inventory scope separately from RFC-0038 distribution qualification.

The canonical main integrator reserved exactly BPR-0974/0975 at `97067055`,
incorporated here as `bbba8973`, including counter and immutable receipt. The
allocator's refusal on this work branch was respected; no IDs were invented or
duplicated. A temporary local helper-path bridge was removed without a MASK
repair. The shared finish records approval against all fourteen amended
criterion rows below, not against the transferred unobserved native results.

**Verification and evidence.** At `bbba8973` (the same product/test inputs as
`2d3bdbdf` plus `e7843405` test upkeep), the focused Settings selection passes
**6/6, 27.78 s**, including all four scales, the effect boundary and the complete
appearance import/preview/commit/export/restart/FIFO course (25.10 s). All
**11 inventory comparison tests** pass, retaining missing-control/changed-state
negatives and the inventory-only limit. At claim `95e64b4c`, the integration
selection passes **1/1, 21.61 s**. The documentation verifier and all **64
regression tests** pass; expected negative fixture errors are not live failures.
Board validation is valid with **967 cards**. The explicit canonical closeout
check covers all fourteen current criteria and the immutable base; final
documentation and diff checks pass after the Done transition.

The current [verification ledger](../../verification/BPR-0593-native-settings-continuation.md#owner-amended-closeout-r2--2026-09-21)
records exact commands. Fresh generated evidence is under
`build/rfc0018-r2-{focused,inventory,integration,documentation-tests}.json` and
the final `documentation-final`, `board-final`, `closeout-final` and
`diff-final` JSON receipts. Unchanged Mac native/signature and architecture
passes, earlier three-platform inventory, actual profile/saved-server courses
and precise human verdicts retain their producing candidates. Prior broad
CLI/composed failures remain BPR-0915/BPR-0940, not newly run or counted green.

**Scope/self-review.** The immutable baseline is unchanged. The current ledger
reconciles the scoped final behavior against that base and all twenty required
Done implementation/follow-up cards; later profile, Accounts, Appearance,
updater and maintenance ownership remains explicit. Only AC-01 and AC-11
terminal wording changes under r2, with originals and failures preserved.
Documentation impact is the acceptance amendment, two standalone cards,
RFC/index/plan/checkpoint reconciliation and repaired live links. No public API,
store, auth/provider, threading, build/package or launch behavior changes in
r2; the earlier native shortcut correction retains its exact regression and
native evidence. No independent or human verdict is self-certified.

**Ownership and commands.** Fresh read-only probes confirm zero owned Windows
apps/tasks, the staged image still absent, the same Defender finding and a free
station lease. Linux reports its unchanged live/source revisions, expected
binary digest, no owned process and a free lease. Existing dirty MASK locks are
preserved. Local test/transport children are collected; no app, command,
delegate, review server or other task-owned writer remains. No new native
course, security-policy change or live GitHub/GHES action was attempted in r2.
Inert remote roots and exact raw evidence remain available to BPR-0974/0975.

**Continuation.** Stop at `close-rfc-0018`; the coordinator owns the next fresh
implementer. `close-rfc-0024` is now plan-order eligible and all its RFC
prerequisites, including RFC-0008/BPR-0242, are satisfied. Its BPR-0595 closeout
card is **not yet claimable**: BPR-0715 remains Parked on `human-review` and is
its sole unfinished dependency after this finish. No successor work or new
attended session is started here. BPR-0974/0975 remain independent unresolved
follow-ups, not blockers recreated on RFC-0018 or prerequisites added to 0024.
The final r2 bind/observation/check use
`build/rfc0018-r2-milestone-{checkpoint,observation,check}.json`; the checker is a
consistency guard, not the source of engineering acceptance.

## Prior r1 continuation checkpoint, 2026-09-21

Run `rfc-0018-0026-closeout-20260920`, segment `rfc-0018`, mandate `r1`;
sole non-delegating owner `/root/rfc0018`, milestone `close-rfc-0018`, first of
0018 → 0024 → 0025 → 0026 on `codex/rfc-0018-0026-closeout` in the 0152
worktree. The coordinator's current amendment resumes native station work after
main merge `86fa923e`; the user confirms Windows and Linux stations available.
Planning may reconcile stale facts, not weaken acceptance. This section
supersedes the earlier checkpoint's current-state claims, not its evidence.

**Disposition: Blocked on `native-settings-qualification`; RFC not closed.**
RFC-0014/BPR-0965 is now Auto-Verified/Done at `b7eb3722`, exact r4 handoff
`61907e20`, for its release/maintenance foundation. RFC-0038 preserves its full
native/security qualification. `08170fd9` removed only the satisfied canonical
blocker; helper claim `280167ac` preceded product edits. Twenty other RFC-0018
cards remain Done. No finish acceptance was submitted and no criterion waived.

**Implemented.** Native Windows execution exposed a real unbound Ctrl-comma
action: Qt's platform `Preferences` standard key is empty there. Product commit
`2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273` binds Ctrl/Command-comma explicitly
and scopes it to the invoking workbench, avoiding application-wide ambiguity
with two windows. Four-scale exact-binding/two-window key regressions pass.
The subsequent Qt 6.11 test-call upkeep uses nondeprecated `activateWindow()`.
No host/store/credential/provider authority changed. The plan boundary includes
this required native-entry correction; outcome and exits are unchanged.

**Direct evidence and remaining blockers.** The
[native continuation report](../../verification/BPR-0593-native-settings-continuation.md)
contains exact stations, sources, binaries, commands, isolation and raw captures.

- Current Mac tests-OFF Qt 6.11.2 bundle SHA-256
  `cff115f8c987c7a94f89667b59cd69b46ad833cd5500cfb06f8710cf13bedeb7` passes all
  11 Preferences/Command-comma/modeless/sibling/category/close/reopen/Quit checks
  and strict signature verification. Quit exits 0.
- Windows' corrected native course passed its menu/Ctrl-comma/modeless/Escape/
  reopen prefix, then stopped before the sibling/inventory claims. Defender
  quarantined the staged and producer images as `Trojan:Win32/Bearfoos.A!ml`,
  threat `2147731250`; the staged file is absent. Its recorded SPDX digest is
  `341e25b20111f48b7882d8c0a288de477cbcfd41eefc24b3bbbf861f1281e768`.
  No restoration, exclusion, security disablement or alternate-image execution.
  Security-owner disposition and a cleared exact candidate are prerequisites
  for further Windows work; existing BPR-0960/0962 own broader qualification.
- Linux's actual native **inventory-only** course passes all seven current
  pages and native window close with exit 0, binary SHA-256
  `8234abccb4141bbf52c91c568b25917abd2e3125f46f14e1a0073ee285b7a51e`.
  A final rerun produces an identical capture. The full modeless/keyboard/
  sibling course does **not** pass: mapped workbench XID `6291474` cannot retain
  focus from Settings XID `6291484` despite bounded X11, WM and native WSLg
  focus attempts. The observer refuses wrong-target keys. A separate two-QWidget
  control with no BetterPullReview code reproduces that failure, then passes
  focusing A after native close of B and exits 0. This isolates a station/Qt/
  observer limitation, not a demonstrated Settings composition defect. Its
  exact layer remains unresolved; this is neither unavailability nor AC-01 acceptance.
- The current Linux build requires one retained GCC `ignored-attributes`
  warning demotion; it is not a clean compiler-policy pass. The package
  producer's executed baseline gate rejects Ubuntu 26.04; the configured
  station/images supply neither Ubuntu 24.04 nor RHEL 10. This is the existing
  BPR-0961 limit, not permission to call the build a qualified distribution or
  a new RFC-0018 dependency. Done BPR-0731 accepted the same Release/tests-OFF
  graph inventory with documented diagnostic demotions. That Linux refresh is
  now supplied; AC-11's current Windows refresh remains pending.
- Mac/Linux current controls match on six pages. Linux omits only the explicit
  macOS-only `Use GitHub CLI` capability. General's updater rows and all seven
  RFC-0014 maintenance buttons are refreshed. Prior comparable three-platform
  inventories and accepted Settings/Accounts/Appearance human verdicts remain
  bound to their original candidates; visibility grants no new maintenance,
  updater, vault or release acceptance.

**Verification.** Corrected-source focused selection passes **6/6, 27.18 s**
(four Settings scales, effect boundary, full appearance/restart contract).
Workbench integration passes **1/1, 20.90 s**. Architecture boundaries, changed
C++ formatting, Mac signature and observer syntax pass. The inventory
comparison passes **11 tests**, including missing-control/changed-state
negative controls and explicit inventory-only limits. Prior broad legacy CLI
failures stay linked to BPR-0915/BPR-0940, not counted as new green evidence.
The final documentation gate, its 64 tests, board validation and diff check are
recorded with this checkpoint. The helper bind/observation/check are renewed
under `build/rfc0018-milestone-{checkpoint,observation,check}.json`.

**Upkeep and ownership.** This slice owns the new continuation report and two
captures, external Windows/Linux observers and bare-Qt focus control, comparison tests, one Qt test API
update, Settings guide/authority-inventory refresh, this card/RFC/index and
helper-repaired live links. `2d3bdbdf` owns the product action and plan boundary.
Both configured stations were actually connected and executed under leases and
external-root guards. Live checkouts and their dirty MASK locks are unchanged.
Owned remote roots are `C:/Users/mikem/bpr0018-native-280167ac` and
`/home/mmarcin/bpr0018-native-280167ac`; source/build/capture/security evidence
is retained inert for continuation. All app/observer/build children are waited;
both exact Windows scheduled tasks and the diagnostic RAIL bridge are stopped
and removed. Station leases are free. No delegate, server or repository writer
remains; no live GitHub/GHES mutation or publication occurred.

**Next action.** Obtain the security disposition/cleared Windows candidate and
complete its full native course/current inventory; resolve the native Linux
focus/lifetime course on a usable target. Ready commands and preserved paths
are in the report. BPR-0961's broader distribution qualification stays with RFC-0038.
Then rerun only affected native checks, rebind and finish BPR-0593 if its ledger
permits. Do not recreate the satisfied RFC-0014 dependency or infer a waiver
from its r4 scope transfer. `close-rfc-0024` is the next authored milestone but
is not eligible while this one is unaccepted. No new attended-review session or
server: these are security/native execution gaps, not unanswered visual verdicts.

## Previous milestone checkpoint, 2026-09-20

Run `rfc-0018-0026-closeout-20260920`, segment `rfc-0018`, mandate `r1`;
owner `/root/rfc0018`, non-delegating `rfc-implementer`. Assignment: first
milestone of 0018 → 0024 → 0025 → 0026 in the 0152 worktree on
`codex/rfc-0018-0026-closeout`. The owner's planning amendment permits stale
RFC reconciliation, not an acceptance/dependency waiver. The plan now explicitly
includes native evidence and bounded test-course upkeep, with unchanged exits
and no new product work. The d31 record accepts its
`attended-preparation-pack-head` prerequisite.

**Disposition: blocked; independent macOS work complete, RFC not closed.**
All twenty other RFC-0018 cards are Done. This card was re-readied at
`e64bf640` and claimed with the shared helper at `1b1c62dc`; the resumed
engineering/evidence slice is `138dd1d9870a8a77b1746d7e586565fae5cef962`.
The prior reconciliation is `38334184`. No finish acceptance was submitted:
neither Done nor human-pending would describe the remaining obligations.

- RFC-BPR-0014/BPR-0965 is still open: canonical `rfc-0014-closeout` blocker.
- AC-01 still lacks Windows/Linux native menu/shortcut/window-lifetime courses.
  Their existing inventory captures clicked the toolbar Settings button only.
- AC-11 now has a comparable macOS packaged capture and a passing like-for-like
  three-platform baseline comparison. Current Mac dev/release also match.
  A current Windows/Linux refresh of the later updater/shared-sign-in rows
  remains unperformed, explicitly scoped to their owning RFC-0035/BPR-0915
  changes. It requires native target surfaces outside this exclusive Mac scope.
- No new human verdict is needed merely to repeat the accepted
  Settings/Accounts/Appearance observations. The broad legacy CLI failures are
  precisely linked to BPR-0915/BPR-0940 below, not a remaining Settings restart
  failure or a claim that the whole suite passes.

**Candidate scope.** Product source, application CMake and packaging remain
byte-identical to initial `19a3dfd71324d59c3f0f6e4c23ff3ffc3c69faf8`.
Tests changed only to repair the appearance restart entry and capture/compare
native inventories. The immutable RFC base remains
`e851eb8a851ab605cf5a85260a1317ecdf66773b`. The scoped comparison was
reconciled through the implementation/follow-up cards and later profile,
Accounts and Appearance owners; unrelated intervening work is not attributed
to this closeout.

**Native evidence.** The
[Mac inventory and course report](../../verification/BPR-0593-macos-settings-inventory.md)
records exact binaries, isolation, commands, raw captures and the cross-platform
comparison. Current dev SHA-256 is
`b6ae3791a26e6278e92b492c519148c33b1ee546b46e2f69564b511e0d59ade9`;
current tests-OFF Release is
`df25f694275a20397b090896e5a2f3237d3d08c4b234d3474676701a8a101a78`.
Comparable tests-OFF Mac source is the exact Windows revision `e93d9215`,
binary `e7c15865cf7636dda67ee9337ac60e7ac7cb707c404a082ae93e84cb49a0566c`.
All three native Preferences/Command-comma/modeless/two-window/seven-page
courses pass and Quit exits 0. Both release signatures verify strictly.
Current Mac inventories are exactly equal; seven baseline pages match
Windows/Linux actionable controls; five unaffected current pages still match.
Later General/CLI differences have explicit ownership/capability explanations.
Local arm64 bundles are not RFC-0014 notarized/universal distribution evidence.

**Verification.**

- Initial serial integration was 9/11 (43.37 s), not a green suite. Unchanged
  appearance/workbench stores, Appearance panel and application workbench
  composition passed; the latter took 22.07 s.
- The required appearance contract's restart was entering the old inline CLI
  self-test, failing at stage 0 before persistence assertions. It now uses the
  existing composition-only probe, with all original palette, accepted-write,
  zero-restart-write and observation assertions retained. Full customization,
  export, restart and FIFO-refusal contract passes.
- At `138dd1d9`, the final serial focused run passes **6/6, 26.70 s**:
  four Settings scales (112 page/appearance/minimum-size combinations),
  Settings effect boundary, and the complete appearance contract (24.09 s).
- Actual two-window profile public course passes, 78.15 s. The saved-server
  GitHub.com/GHES four-process course passes, 92.67 s.
- Six inventory comparison tests pass, including missing-control/changed-state
  negative controls. Swift typecheck passes with warnings treated as errors.
- `qt_composed_github_scale_100` still stops at the exact BPR-0940 CLI-card
  assertion. The extra empty-root composition control exits 0, reports the
  non-imported palette `#7fb0e8:#ffffff`, and its CTest wrapper fails the
  separate obsolete CLI-label regex (actual label empty). BPR-0915 already
  records non-fixture stage-0 inventory drift; BPR-0940 owns its broader
  assertion reconciliation. Neither failure is silently turned into acceptance.
- Both architecture gates, documentation gate, all 64 documentation tests and
  `git diff --check` pass. An interim architecture scan found the task-owned
  extracted archive as a second source tree; removing that copy restored the
  unchanged scanner's pass. The bundle/source archive were preserved.
- The missing local `.agents` deployment still requires
  `PYTHONPATH=/Users/jaeger/.agents/skills/implement-rfc/scripts` for the
  documentation commands. No global MASK repair or topology substitution.

Logs are `build/rfc0018-focused-final.log`, `rfc0018-profile-course.log`,
`rfc0018-public-path.log`, `rfc0018-integration.log`,
`rfc0018-theme-negative-control.log`, `rfc0018-boundaries-final.log`,
and `rfc0018-documentation{,-tests}-final.log` under `build/`.
Native/build logs and generated evidence are indexed in the Mac report.
The renewed helper bind/observation/check remain
`build/rfc0018-milestone-{checkpoint,observation,check}.json`.

**Changed paths and upkeep.** `138dd1d9` owns the new Mac verification report,
its three JSON captures, `tests/scripts/macos_settings_inventory.swift`,
`tests/scripts/settings_inventory_comparison_tests.py`, the appearance
contract CMake change, authority-inventory addendum and milestone-plan boundary.
The earlier `38334184` supplies the Settings guide, README/architecture/RFC
reconciliation and strategy snapshot. This handoff updates this card, RFC/index
and helper-repaired live links; it rewrites no protected review packet or
historical verdict. No product API/store/security mechanism changed.

**Ownership and command state.** This leaf is the sole writer. All task-owned
commands and three captured apps have completed; no app, test child, server,
agent or background writer remains. No subagent or live GitHub/GHES mutation.
The disposable Unix-socket directory was removed after verifying it empty.
The extracted historical source/build tree was removed after preserving its
source tar and digest-verified bundle at
`build/rfc0018-baseline-candidate/BetterPullReview.app`. Other inert ignored
build output remains available. No personal application data or other checkout
was modified. Historical station dispatches below authorize no current writer.

**Continuation.** Resolve RFC-0014/BPR-0965 and supply the precisely bounded
Windows/Linux native evidence above, then rebind and close BPR-0593 only when
its ledger and canonical dependencies permit it. `close-rfc-0024` is the next
authored milestone but is not dependency-eligible while this one remains
unaccepted. No attended session/server was created: these remaining obligations
are canonical/target-platform work, not unanswered human judgment.

Packet-pointer upkeep, 2026-09-20: the RFC-0013 v004 hand-back is preserved in
the [current v005 attended-review packet](../../verification/reviews/rfc-bpr-0013-attended-v005/operator-packet.md).
Its sole refreshed source/anchor question remains unanswered; the positive v004
loading observation and separate device-relaunch r17 acceptance are preserved.
This pointer changes no RFC-0018 acceptance or
historical verdict below.

## Current frontier, 2026-09-17 (run-20260916-stations, dispatch dsp-009, mandate r6): in progress — checkpoint written before long work

This section is a live checkpoint for dsp-009 and is rewritten at its end.
The dsp-008 section below holds except where this one corrects it.

**Heads at this checkpoint.** Main `a87c0c2a` (local; `de6bc6bf` pushed).
`origin/linux-lane` = `de6bc6bf`, **held there on purpose** (see below).
`origin/windows-lane` = `cbf6d920`, labelled CHECKPOINT by `bpr-win`, not
merged. The `project-integrator` lease is taken per command with
`station_lease.py run`, not held by a long-lived process.

**Done so far.** `bpr-linux`'s BPR-0906 RETURN `de6bc6bf` (docs only: the
Linux exit-path measurement, a survivor is *reclaimed* not refused, and a
second leaking row `application_attended_launch_contract`) is fast-forwarded
onto main and pushed. [[BPR-0896]] is claimed (`a87c0c2a`).

**In flight on the Mac.** One `rfc-worker` on Fable (mandate r6) building
[[BPR-0896]] in an isolated worktree with its own build directory; its brief
pins the design: a new `observe_review_thread_comments` operation compiling
`query ObserveThreadComments` (POST to the GraphQL root), a GraphQL-envelope
parser behind the same `parse_review_comment_observations` entry, **outdated
and non-User comments are tolerated non-matches and never abort** a
reconciliation, `fullDatabaseId` read as a string, side from the thread's
`diffSide`, the CLI transport refuses the new operation with the other four,
and session rows that assert every request's method/path/operation-name
explicitly under the unchanged positional `fake_transport`. Its rows are
**held** until `bpr-linux`'s spike checkpoint lands, and the linux-lane ref is
held at `de6bc6bf` (defect side) so the spike's acceptance test stays runnable.
A five-row POSIX check of `bpr-win`'s checkpoint runs in `build/r2-dev`
(`build/r2-logs/dsp9-win-checkpoint-posix.log`); `build/dev` is a day stale and
must not be used for evidence.

**Decisions made.** Spike first for the `tests/github_contract/**` question,
told to `bpr-linux`, which has started on Fable. The bpr-win checkpoint's three
non-green corrections are to be kept, not reverted, pending the POSIX check.

**Next.** Answer `bpr-win`; collect the worker; merge the spike checkpoint;
land BPR-0896 on top; current macOS sweep and [[BPR-0893]] once the machine is
quiet; then the dsp-008 objective order.

## Current frontier, 2026-09-17 (run-20260916-stations, dispatch dsp-008, mandate r6): a P1's blocking question is answered from the published schema, and the Windows lane is collected

Dispatch dsp-008 ends here. The dsp-007 section below still holds except where
this one corrects it. It was a short dispatch: mandate **r6** arrived a few
minutes in and directed a close at the next clean integrated boundary, so this
is a collection-and-unblock dispatch rather than an implementation one.

**Heads.** Main `f413907cf425653a1ae8b4d1a8c1413634767e78`, pushed.
`origin/linux-lane` and `origin/windows-lane` are fast-forwarded to **exactly
that same commit**, verified by `git ls-remote`, and both stations have been
told. The `project-integrator` lease is **released**. On the Mac no worker,
scout, reviewer, test process or scratch worktree remains, and no
BetterPullReview process is running. Logs are `build/r2-logs/dsp8-*`.

### Mandate r6, and what it means for the next dispatch

Owner, verbatim: *"Okay. Use only fable from here on out we're hitting limits
and going to have unused fable"*. Reason given: usage limits on the current
route with unused Fable capacity. **Every agent dispatched from here routes to
Fable** — workers, scouts, reviewers, test runners, utilities — regardless of
what a role's configured route says, and each is told to route its own children
the same way. Both stations have accepted it and are enforcing it station-side.
`bpr-linux`'s live BPR-0906 writer keeps its existing route and finishes rather
than being restarted to change models.

It is a **resource-routing instruction and not an authority change**: nothing
moves on scope, ownership, or what may be touched. Both stations reached that
reading independently. `bpr-linux` additionally flagged to its own user that it
is acting on a *relayed* mandate rather than one its user typed, and kept that
flag standing after the coordinator confirmed the quotation — which is the
right instinct and is recorded here so the next dispatch does not re-litigate
it.

One cost it priced honestly, worth carrying: BPR-0904's spike has a
judgement-heavy classification step, and `bpr-linux` has said that if a
classification comes back without evidence it can point at, it will **re-run
that step and say it did** rather than ship a table that reads as authoritative
and is not. That is the same shape as the defect BPR-0904 exists to fix.

### [[BPR-0896]] is unblocked. The answer is yes, and no owner decision is needed

The P1's two blocking server-behaviour questions were answered **read-only from
GitHub's published public schema** (`https://docs.github.com/public/fpt/schema.docs.graphql`),
checked field by field against `comment_observation`
(`github_mutation_contract.cpp:1045-1076`). No live GitHub or GHES request was
made and none is needed.

**Question 1 is yes**, so question 2 is moot for choosing the shape: the fix is
a **thread-scoped GraphQL observation**. `PullRequestReviewThread.comments`
returns `PullRequestReviewCommentConnection!` and the comment type carries
every field the exactness check consumes. The full mapping is on the card. Three
rows of it are not what an implementer would guess:

- **`fullDatabaseId`, not `databaseId`.** `databaseId: Int` is a 32-bit signed
  GraphQL `Int`; `BigInt` is documented as string-encoded because the value may
  exceed it. The check stores a `std::uint64_t` and refuses zero, so the parser
  reads a *string*, the opposite of the REST shape.
- **The side is not on the comment.** `PullRequestReviewComment` has no side
  field at all; `PullRequestReviewThread.diffSide: DiffSide!` supplies it, and
  `enum DiffSide` is spelled exactly `LEFT`/`RIGHT` — the two strings the check
  compares against.
- **The author's ids need an inline fragment.** `interface Actor` exposes only
  `avatarUrl`, `login`, `resourcePath`, `url`; `databaseId` and `id` require
  `... on User { ... }`, and the check demands a nonzero `User.Id`.

**The constraint found on the way is worth more than the answer.**
`parse_review_comment_observations` (`:1738-1753`) tolerates `identity_mismatch`
per comment but **aborts the whole reconciliation on anything else**, and
`comment_observation` returns `malformed_response` — not `identity_mismatch` —
when `Line == 0` or the commit fails to parse. REST always sends a numeric line
and a `commit_id`. **GraphQL does not:** `line: Int` is null for an outdated
comment (`originalLine` carries it) and `commit: Commit` is nullable. So one
outdated comment in a thread would abort the baseline reconciliation that runs
*before* the reply is written. The fix has to decide that deliberately and pin
it, because a fixture author writing a happy thread will never produce the case.

Still open on that card and unchanged: the CLI-transport decision
(`gh_cli_transport.cpp:1476-1479` refuses all four GraphQL-rooted operations by
design), and the third acceptance point, which [[BPR-0904]] gates.

### Done on main in dsp-008: both of `bpr-win`'s RETURNs

**`df54d00b` — two composition courses were refused for a descriptor, not a
budget.** Both narrowed their data root with `file(CHMOD ... OWNER_READ
OWNER_WRITE OWNER_EXECUTE)`, which is 0700 on POSIX and only clears the
read-only attribute on Windows, so the root never carried the protected DACL the
stores demand and both rows died at `STARTUP_FAILED`. They now call
`bpr_narrow_owner_private` and take the owned parent [[BPR-0875]] gave the other
twelve courses. **Verified here on macOS at the merged head**, which is the
point of merging it centrally: `application_review_ledger_composition_contract`
PASS 35.78 s and `application_defect_composition_contract` PASS 57.13 s, ctest
exit 0 (`build/r2-logs/dsp8-merge-ctest.log`).

**`3c8f25c0` — Windows had the same silent green as Linux, by a different
route.** [[BPR-0890]]'s Linux half closed a row that stayed green over an
auto-reaped child when `waitpid` returned `ECHILD`. The station was asked to
take *one look* for a Windows analogue rather than assume the shapes matched,
and there is one: the launcher's `fail()` prints only
`BPR_NETWORK_ISOLATION_UNAVAILABLE` and exits 125, which matches neither
inherited failure expression, and two of its call sites — `fail_win32("wait")`
and `fail_win32("exit-code")` — run *after* the child has already printed the
marker its row passes on. Both now print
`TEST_RUNNER_CHILD_EXIT_FAILED:no-status` first. The pre-launch failures are
deliberately untouched, and the reasoning is the keeper: the child never ran
there, so a child-exit marker would claim something untrue.
`test_network_isolation_structure_contract` and `test_network_isolation_manifest`
both PASS here, ctest exit 0 (`build/r2-logs/dsp8-isolation-ctest.log`); the
changed file compiles on Windows only, so that half is taken on the station's
evidence by design.

**Asking for one look paid twice in two dispatches.** Both times the answer was
"yes, and by a different mechanism". That is worth a habit: when a defect is
fixed on one platform, ask the other station for a *look*, not a port.

### [[BPR-0900]] has three authors, not two, and they disagree

Found while answering `bpr-win`'s question about where a tests-owned Windows
narrowing helper should live. **It already exists, twice.** `make_root_private`
is byte-identical in `tests/end_to_end/epoch_ordinary_update_public_path.py` and
`tests/end_to_end/epoch_portable_occurrence_public_path.py`.

The two harness authors write **different descriptors**. The Python one adds the
process's own AppContainer SID grant when contained, and removes SYSTEM,
Administrators and OWNER RIGHTS; the CMake one does neither. Both docstrings
claim to match the product's `make_storage_root_owner_private`. They cannot both
be right. The card now carries the table and the consequence for its acceptance
row: pin all three, or say which pair is pinned and why the third is out of
scope.

The decision given to `bpr-win`: the helper's home is
`tests/end_to_end/bpr_narrow_owner_private.py`, named to match the `.cmake`
helper, and the job is **promotion of the existing copies, not authorship** — a
third spelling of this descriptor is the one outcome to avoid. Not
`scripts/contained_temporary_support.py`, for a reason beyond ownership: that
module's subject is a temporary root usable after containment, and a test-only
descriptor definition would be handed to about twenty importers that do not want
it.

The AppContainer grant is a **cheap lead** on `bpr-win`'s six remaining
container-temp rows and explicitly **not** a diagnosis: that station measured
`application_profile_owner_admission_public_path` failing *uncontained* too,
which rules the container grant out for that row.

### Sweep status — no new full sweep was taken in dsp-008

| Platform | Result | When |
|---|---|---|
| macOS dev | **380/383** | carried from dsp-007 at `22023667` |
| macOS, this dispatch | four rows at the merged head, all PASS, ctest exit 0 | `build/r2-logs/dsp8-merge-ctest.log`, `dsp8-isolation-ctest.log` |
| Windows | **338/364** | carried from dsp-007 at `86b04901` |
| Linux | **368/369** | carried from dsp-007 at `88dc42fc` |

The three full-sweep figures are dsp-007's and are **not** re-measured here.
dsp-008 ran four targeted rows, chosen as the ones the merge could have broken.
A successor wanting a current macOS number must run the sweep; r6 arrived before
there was room for one.

### Station state — read this before dispatching

- **`bpr-win` returned twice and both are merged.** No writer live, tree clean,
  nothing in flight. `origin/windows-lane` is at `f413907c`. Its next work,
  unless redirected: the **six remaining container-temp rows** of the five-row
  family (diagnosed down to the exact refusal — the row fails uncontained too,
  the refusal is `detail::open_state_directory(Root)`, an `icacls` narrowing
  gets the store to *accept* the root and strands the course on a later,
  unmeasured "bounded public course condition" assertion), then [[BPR-0902]]
  with its rewritten card and the one-line `RefreshRetrieveAttempts` marker
  first. It has the helper-home decision and the BPR-0900 lead.
- **A correction it made about itself, worth keeping:** its first probe passed
  forward slashes, which `canonical_windows_root` refuses outright
  (`catalog_database.cpp` ~2571), so that probe measured the probe. It found
  that and re-measured with native paths.
- **`bpr-linux` has a live writer and nothing pushed this task.** Its local HEAD
  is `1ec82785`, tree clean, lane ref `f413907c` **recorded but deliberately not
  fetched** — it does not move the tree under a live writer, which is the rule
  dsp-006 paid to learn. Queue, in order: BPR-0906 writer RETURN → sync,
  reporting before-and-after SHAs → [[BPR-0904]] spike → [[BPR-0905]] survey.
- **The [[BPR-0904]] spike, confirmed not superseded**, scoped to
  `tests/github_contract/**` only so it cannot collide with `bpr-win` in
  `tests/end_to_end/**`. Two conditions added in dsp-008 and accepted: the keyed
  transport is **pushed on a labelled CHECKPOINT rather than reverted**, because
  a number without the diff that produced it cannot be re-derived; and if the
  acceptance test comes back **negative** — BPR-0896's `GET /graphql?per_page=100`
  does *not* go red under the keyed transport — the station **stops there and
  reports the negative**, with no strengthening of the transport until it turns
  red. A measurement you tune until it catches the case is not a measurement.
- **The BPR-0906 measurement's shape**, so nobody re-asks for it: paths not a
  count (orderly quit, crash, SIGKILL, harness termination, data root removed
  under a live owner), each with survives yes/no and the unlinking code by
  file:line; and at the **top** of the return, whether a survivor is
  **reclaimed or refused** on the next start with the same root, with an
  explicit statement of whether that was established by a real start or only by
  reading. Reclaimed means BPR-0906 is close to inert and [[BPR-0895]] decides
  almost alone; refused means BPR-0906 is the serious one and the decision
  inverts.
- **The Windows-adjacent offer stays declined.** `bpr-linux` offered a POSIX
  analogue of the private-descriptor family; dsp-007 declined it and dsp-008 did
  not revive it. `bpr-win` is inside that work with the mechanism in hand, and
  two stations reasoning about the same rule from opposite sides is how you get
  two answers. Handover condition if it becomes portable: files named.
  `bpr-linux`'s stated limits: no Windows, no MSVC; it can read a diff, reason
  about a POSIX/Windows split, or reproduce a portable half.

### Next objectives, in order

1. **[[BPR-0896]]**, now unblocked and still P1 — a live reply cannot be
   submitted at all. Build the thread-scoped GraphQL observation using the
   mapping on the card, and decide the outdated-comment case deliberately. The
   CLI-transport question still needs an answer and is not a reason to wait.
2. **Collect `bpr-linux`**: the BPR-0906 measurement, then the [[BPR-0904]]
   spike's number and its acceptance test. The spike lands in
   `tests/github_contract/**`, which BPR-0896's rows also want — sequence them,
   do not run both.
3. **`bpr-win`'s six container-temp rows**, then [[BPR-0902]].
4. **[[BPR-0905]]**, then [[BPR-0893]] — and a current macOS sweep, which
   dsp-008 did not take.
5. Then [[BPR-0906]]/[[BPR-0895]] decided together, [[BPR-0874]],
   [[BPR-0898]], [[BPR-0899]], [[BPR-0900]], [[BPR-0903]], and finally
   [[BPR-0854]] and [[BPR-0861]], still In Progress for Windows alone.

### Open owner asks

**[[BPR-0896]]'s two questions are removed from this list** — answered from
published documentation in dsp-008, no owner decision required. The rest are
unchanged and none was decided here: [[BPR-0897]] (the release-build mutation
boundary, three answers offered, now carrying the fact that a reply could never
have been written against a real server anyway); [[BPR-0653]]'s private Qt API
in a shipped target (integrator recommends accepting, with a recheck obliged on
every Qt upgrade) and its criterion-4 capture; [[BPR-0825]] item 3 (recommend
(c)); [[BPR-0644]]; [[BPR-0817]]; [[BPR-0824]]; [[BPR-0803]]; [[BPR-0800]];
[[BPR-0801]]; [[BPR-0604]]; [[BPR-0905]]'s question of whether the tree wants a
stated rule for sizing bounds; and the [[BPR-0794]]/[[BPR-0795]] attended
packet, intact.

### Hazards learned in dsp-008

- **A published schema can retire a "needs a live target" blocker.** BPR-0896 sat
  blocked across two dispatches on two questions about GitHub's server. Both were
  answerable in one fetch of the public schema file. Before parking a question as
  requiring a live system, check whether the vendor publishes the answer.
- **`git fetch` followed by reading remote-tracking refs is a snapshot, not the
  truth.** Main moved twice inside this dispatch's first ten minutes while
  `refs/remotes/origin/*` still read the old SHA from a fetch taken seconds
  earlier. Resolve lane and base SHAs with `git ls-remote` when another writer
  may be live, and with `git rev-parse --verify` before quoting one.
- **A predecessor that has "returned" may still be committing.** Two commits
  landed on main between this dispatch's first `git log` and its second. They
  were dsp-007's supplementary work and entirely legitimate; the hazard is
  assuming a handoff SHA is final because a return said so. Re-resolve before
  merging or pushing.
- **The station-writer lease is per-process, not per-session.** The
  `project-integrator` lock is held by a wrapped process and released when that
  process exits; a lease "held" at orientation may belong to the predecessor and
  free itself minutes later. Check `station_lease.py status` and re-acquire
  rather than inheriting an assumption.
- **Ask for a look, not a port.** Two dispatches running, asking the other
  platform's station to look for an analogue of a just-fixed defect has found a
  real one both times, by a different mechanism each time.

## Current frontier, 2026-09-17 (run-20260916-stations, dispatch dsp-007, mandate r5): both stations were blocked by a lane ref, and one reply has never worked

Dispatch dsp-007 ends here. The dsp-006 section below still holds except where
this one corrects it.

**Heads.** Main `026e87ccddee8c2e4fadbac8e16abd3e9db5aa52`, pushed, 16 commits
since `e2ec4b4b`. `origin/windows-lane` is **already fast-forwarded** to that
same commit and `bpr-win` has been told. `origin/linux-lane` is at
`7dc78341` — **ahead of main and unlabelled**; see Station state. The
`project-integrator` lease is **released**. On the Mac no worker, scout,
reviewer, test process or scratch worktree remains, and no BetterPullReview
process is running. Logs are `build/r2-logs/dsp7-*`.

### Sweep status — all three platforms swept in this dispatch

| Platform | Result | When |
|---|---|---|
| macOS dev | **380/383** | this dispatch at `22023667`, `dsp7-0901-full-sweep.log` |
| macOS, merged head | the sweep's one non-[[BPR-0893]] failure **re-run green** | `dsp7-merge-ctest.log`, 8/8 |
| Windows | **338/364** | this dispatch at `86b04901`, `docs/verification/dsp7-windows-full-sweep.md` |
| Linux | **368/369** | this dispatch at `88dc42fc`, 484.61 s at `-j10` |

macOS's three failures were `test_network_isolation_structure_contract` —
**fixed by merging `bpr-win`, and verified green here at the merged head** —
and two [[BPR-0893]] footprint rows. `application_large_review_loaded_close`
**passed** this time having failed in dsp-006's sweep, which is BPR-0893's own
instability showing again.

Windows went from 309/362 to **338/364: twenty-seven rows green, and no row
that was green at `2b2e5549` is red**, compared by name against the recorded
sweep rather than from memory. Linux went from 367/367 to 368/369 with two more
rows configured and 4.88 s more wall clock — **supervising every row costs
nothing measurable**. Its one red row is the exemption-cause tripwire, already
fixed on main by `bpr-win`'s merge and verified green here.

**Every platform's sweep is now from this dispatch, and they are not
comparable to each other by design:** macOS and Windows ran with BPR-0901's
fix, Linux ran deliberately on the defect side because its clone had never
fetched main. That was the more useful side to measure from.

### The first thing dsp-007 found, because it blocked both stations

dsp-006 dispatched both envelopes with `base_commit d6dfca2b`, which existed
**only on `main`**. A station fetches its own lane and nothing else, so neither
could reach its own base. `origin/linux-lane` was at `618bad62` and
`origin/windows-lane` at `23e7eb18`, both older than the base they were told to
work from. `bpr-linux` was stopped dead by it. `bpr-win` had been working
around it silently, checking ancestry and merging the `main` SHA explicitly
each time — the right defence, and the reason nobody noticed sooner.

Fixed by fast-forwarding both lane refs to `e2ec4b4b`, which contains
`618bad62`, `23e7eb18` and `d6dfca2b`. **The generalisation is the point:** a
lane ref is not advanced by merging the lane into `main`. dsp-005 pushed the
lanes forward and dsp-006 did not, so the protocol has to be explicit —
*after merging a lane, push the lane ref forward, then tell the station.*

### The station protocol gained two things and both stations adopted them

- **CHECKPOINT or RETURN, in the first line of every reply.** dsp-006 merged
  `618bad62` as if it were a return while a writer was live, which is what
  created the reconciliation `bpr-linux`'s writer is now paying for. Both
  stations now label every pushed tip. An unlabelled tip is treated as a
  checkpoint and queried, never merged.
- **`bpr-win` is at `86b04901`, labelled CHECKPOINT. It is fetched and
  deliberately not merged**; a full sweep is running against that exact tree
  and the RETURN will be labelled.

### `bpr-linux`'s answer: main is not stale

The question that could have invalidated `main` is answered, from an in-flight
read of the uncommitted writer tree and explicitly **not** a return. The
BPR-0880 msgid **identifier list does not move**. Five **version floors** drop
at patch level, `2.39.5` to `2.39.0` (`gitattributesSymlink`,
`gitignoreSymlink`, `mailmapSymlink`, `nulInHeader`, `unterminatedHeader`).
Three are INFO, so the emitted `--strict=<id>=ignore` string only *widens*, and
only on an installation in `[2.39.0, 2.39.5)`; the two fatal rows never enter
the policy. Every Git at or above `2.39.5` — which is every installation this
run uses, including Apple Git 2.50.1 here — gets a byte-identical string.
**So `caeb98da`'s merged copy is incomplete in a permissive direction, not
wrong, and nothing was built on a stale table.** The floor change lands once,
with the writer's return; it is deliberately not split out early.

### Done on main in dsp-007

**[[BPR-0894]] is Done** (`424a64cc`). The architecture boundary rule reserved
the *token* of a platform's per-user directory variable to the GitHub CLI
transport, so no component could read one to find a directory — which is what
forced [[BPR-0881]] to probe `/run/user/<uid>` directly. The rule now separates
*constructing* a child environment (still the transport's alone) from *reading*
a variable as a directory path through `environment_directory("NAME")`
(permitted anywhere), for the four variables that are directory publishers:
`XDG_RUNTIME_DIR`, `USERPROFILE`, `APPDATA`, `LOCALAPPDATA`. The other reserved
names stay reserved with a reason each, on the card. Sixteen hostile forms are
pinned, including a discovery read followed by a construction naming the same
variable, so the strip cannot launder a leak. **Falsified against the
pre-change rule**: the same discovery test fails on all four variables there,
which is what shows the narrowing is load-bearing rather than a test that would
pass either way. The product probe is deliberately unchanged while `bpr-linux`
measures BPR-0881's Linux criterion against the current resolution order; the
follow-up is on the card.

### [[BPR-0896]] is now P1: a live reply has never worked at all

A read-only survey checked the card against the code. The defect is confirmed
and **understated**. `submit_and_reconcile` runs a **pre-admission baseline**
reconciliation (`github_mutation_contract.cpp:2116`) before the POST, using the
same broken compiler, so against a real server a reply **fails before anything
is written**. Not "cannot reconcile or recover" — cannot be submitted, on the
operation a reviewer uses most.

Two further corrections are recorded on the card rather than edited in: the
machinery the card said to reuse returns no comment data and cannot be reused
as-is, and there are three GraphQL-rooted operations rather than two. Two
server-behaviour questions are named that decide the fix's shape and that this
repository cannot answer by reading; **neither shape should be built before
they are answered.**

**[[BPR-0904]] is new and explains why nothing caught it.** The mutation
fixtures' `fake_transport` answers from a positional queue and **ignores method
and path**, so no row can fail a wrong request URL. Exactly two assertions pin
a request path across the whole mutation surface. `validate_outbound_review_mutation_request`
cannot cover the gap either: it validates by recompiling the canonical form and
comparing, so it reproduces the same defect and passes.

[[BPR-0897]] gained one fact and no recommendation: since a reply could never
have been written against a real server, that card's residual could not have
produced a write for that operation. It shrinks the exposure and is not a
reason to answer (1).

### [[BPR-0901]] is Done, and the answer is none of the card's three hypotheses

- **[[BPR-0901]] is diagnosed, and the answer is none of the card's three
  hypotheses. It is a budget.** Both rows drive `--epoch-fixture-phase` four
  times, so they are epoch-phase rows — but they carry
  `bpr_fixture_run_budget_ms` (**40000**) instead of
  `bpr_epoch_phase_budget_ms` (**120000**). Seventeen sites already use the
  epoch budget; these two were missed when [[BPR-0555]] moved the rows it knew
  about. dsp-006's two failures were `RUN_BUDGET_EXPIRED:elapsed_ms=40035` and
  `elapsed_ms=40052` against a 40000 ms line — **over by 35 and 52
  milliseconds**, on a phase that takes about 28 s driven directly on a quiet
  host. Confirmed independently by the integrator from `build/r2-logs/dsp6-*`
  rather than taken from the worker.

  That explains every observation the card found puzzling. They alternate
  because whichever row meets more contention reaches 40 s first. They never
  failed together for the same reason. dsp-005's sweep missed them because a
  1.4x margin usually holds. Eleven green runs at HEAD in four contexts missed
  them because none was heavy enough. And the reason it was read as an epoch
  defect is that the row's headline said "seed process failed" while
  `RUN_BUDGET_EXPIRED` sat further down the dump — so the fix also promotes
  that line to the headline.

  **This is not macOS-only, and that is established on two checkouts without
  running anything.** Those two registrations in
  `tests/end_to_end/CMakeLists.txt` are not platform-conditional, and
  `bpr-linux` quoted both of them verbatim from its own HEAD: `TIMEOUT 240`,
  `BUDGET_MS ${bpr_fixture_run_budget_ms}`, `RUNS 4`. So all three trees carry
  it. Both stations were told to report the exact `elapsed_ms` rather than
  diagnose it, and not to patch it in a lane — the fix edits a file all three
  of us edit.
- [[BPR-0893]] was given to the worker as a possible shared cause, since two
  cards described macOS rows in `build/r2-dev` failing intermittently. On this
  evidence they are **not** the same fault: BPR-0901 is a clock, BPR-0893 is a
  footprint budget. BPR-0893 remains open on its own terms.

**The evidence was on disk the whole time.** Eleven expiries are recorded
across four dispatches in `build/r2-logs/`, every one within 53 ms of the
bound — which is what a clock looks like and what a race does not. One of them,
`dsp2-0818-contract-4a2be1b1.log`, is dated **2026-09-13, four days before
`81844fef`**, so hypothesis 1 dies on a date rather than on an absence of
failures. That mattered: [[BPR-0893]] had already shown that green runs in this
build directory prove very little.

**The card's own Defect section was wrong, and that is why four dispatches
misread this.** The marker chain it records for `#343` —
`EPOCH_EFFECT_FAILED:revision-prepare:5`, `GITHUB_FIXTURE_FAILED`,
`REVIEW_LEDGER_FAILED` — is not what the log shows; `revision-prepare:5`
appears only in dsp-004 logs for a different card. The marker that *was* in
both failures, and was omitted, is `RUN_BUDGET_EXPIRED`. **A card quoting
markers from the wrong log is worse than a card quoting none**, and nobody
could establish where they were copied from.

**Landed** (`1ad7d7aa`): both rows move to the epoch budget, 40 s → 120 s,
4.3x the 28 s quiet measurement — the argument being that it makes them
ordinary among seventeen sites rather than a pair with a bound of their own.
`TIMEOUT` rises 240 s → 660 s **because it must**: the row bounds itself at
484000 ms and the harness requires the worst case to fit in three quarters of
the timeout, so 240 s would not configure at all. `RESERVE_MS` stays 4000.
Both drivers now lift `RUN_BUDGET_EXPIRED` into the headline, which is the half
of [[BPR-0555]] these two never got.

**All three platforms measured, and only macOS crosses the line.** Windows
passed both rows at 44 s. Linux, measured deliberately on the **unfixed** bound
with a fresh configure, passed at 10.53 s and 11.59 s under `-j10` sweep load —
whole row, all four runs, against a 40 s *per-run* bound, so roughly fourfold
clear. All **sixteen** Linux rows carrying that bound passed, including
`application_saved_server_contract`, which is recorded expiring elsewhere at a
1.7x margin. So the bounds are not uniformly thin; they are thin **on one
host**, and which host crosses first differs by row.

**A limitation worth carrying forward:** this dispatch asked all three
platforms for an exact `elapsed_ms` and the request was unanswerable wherever
the rows passed. `src/app/main.cpp:4379` prints the elapsed figure **only
inside the failing-verdict block**, so a healthy run reports no margin at all.
Row wall time is the only proxy. That is on [[BPR-0905]], because a margin
invisible until it is breached is a margin nobody maintains.

### [[BPR-0894]] survived a review that was told to defeat it, but not on the first try

The narrowing was reviewed read-only with the explicit brief to break the rule
rather than confirm it, and it found **two holes, both introduced by the
change**. The strip was unanchored, so any name *ending* in
`environment_directory` was laundered — plausible rather than contrived, since
the transport already contains `canonical_environment_directory`. And the
accessor is a file-local function, so the rule trusted a spelling: a file could
define its own that constructs rather than reads. Both are closed in
`efcdff4c`, and five laundering forms are pinned. **All five pass against the
first landing's script and fail against the current one**, which is the check
the original acceptance lacked — its construction test would have passed with
the narrowing removed entirely.

The acceptance recorded before that review was wrong on its second criterion
when it was written. It is corrected on the card rather than rewritten. Two
pre-existing gaps the review separated out — a `KEY=VALUE` spelling never
contains the quoted token, and a name can be assembled from pieces — are
recorded with their snippets and deliberately not fixed, because they defeat
the old rule and the new one equally and apply to the whole 5,400-line script.

### Two new cards, and one of them is a pattern rather than a defect

[[BPR-0904]] — the mutation fixtures' transport answers from a positional queue
and **ignores method and path**, so no row can fail a wrong request URL.
Exactly two assertions pin a request path across the whole mutation surface,
and the egress validator cannot help because it validates by recompiling the
canonical form and comparing, so it reproduces the same defect and passes. This
is why [[BPR-0896]] was found by reading and could not have been found by a
test.

[[BPR-0905]] — **four more bounds sized for a quiet host**, one already
recorded expiring: `application_saved_server_contract` at a 1.7x margin,
`application_defect_composition_contract` and
`application_review_ledger_composition_contract` at 2.2x, and `bpr-linux`'s
`sqlite_epoch_catalog_behavior_contract`, a 180 s timeout against a 122.65 s
isolated run. The four rows are an afternoon; the card exists because the same
shape appeared in **three separate investigations in this one run**, in three
different mechanisms, and each was first read as a defect in the thing being
measured. A bound with a thin margin does not fail honestly — it fails as
though its subject were broken. The card warns explicitly against raising every
bound as a reflex, because two of the three were found *because* a bound was
tight enough to fail.

### Next objectives, in order

1. **`bpr-win`'s five-row family** — the store refusing a root whose parent
   lacks the private descriptor — then [[BPR-0902]], whose cause is now read
   and whose first step is one marker line and one run.
2. **[[BPR-0896]]**, now **P1**: a live reply cannot be submitted at all. It is
   blocked on two server-behaviour questions this repository cannot answer by
   reading; they decide the fix's shape and neither shape should be built
   before they are answered. [[BPR-0904]] is its companion and explains the
   silence.
3. **[[BPR-0905]]**, then [[BPR-0893]] — settle whether the footprint rows
   measure the product or the machine, now that its sibling question has been
   settled the other way.
4. Then [[BPR-0906]], [[BPR-0895]] — decide them together, they are the same
   relocation seen from opposite ends — and [[BPR-0874]], [[BPR-0898]], [[BPR-0899]], [[BPR-0900]], [[BPR-0903]],
   and finally [[BPR-0854]] and [[BPR-0861]], still In Progress for Windows
   alone.

### Station state — read this before dispatching

- **`bpr-win` is working again and `origin/windows-lane` is at `df54d00b`,
  ahead of main and UNLABELLED.** It has been asked for CHECKPOINT or RETURN;
  it is **not merged**, and the lane ref is deliberately **not** advanced even
  though main and `origin/linux-lane` are both at `7fe5811a`. Its tip is ahead
  of what is merged and forcing the ref would destroy work.
- Its earlier RETURN `2fc69fc1` is in main and
  `origin/windows-lane` was fast-forwarded at that point. It has been
  given two tasks and is **not** blocked: the five-row family it named (a store
  refusing a root whose parent lacks the private descriptor, for rows rooted
  under the container temp rather than a course-owned parent — it has the
  mechanism from [[BPR-0875]]), then [[BPR-0902]] with the analysis now on its
  card. Its item 3
  is answered: **no uncontained row exits non-zero while green** — it measured
  each row's own command from `--show-only=json-v1` rather than building a
  second launcher mode, and every green row exits 0. The one red row is red on
  both signals. Two rows it could not classify are resolved: with no
  `PASS_REGULAR_EXPRESSION`, ctest's verdict *is* the exit status, so they are
  judged by the stronger signal and never needed BPR-0890's guard. It also
  fixed two Windows defects in the tests tree, one of which — a raw Windows
  interpreter path eaten as a CMake escape, producing a generated file listing
  no tests — would have failed on any Windows station.
- **The decision it asked for:** record the uncontained-row gap deliberately
  rather than add a no-container launcher mode. Agreed and told. The
  measurement that would justify the machinery came back empty, and a second
  mode on the isolation seam is the last place this suite should gain one. The
  conditions are that the measurement and its *method* go on [[BPR-0890]], and
  that the re-measurement trigger is named: a row newly pinned
  `WINDOWS_UNCONTAINED` makes it stale.
- **`bpr-linux`'s BPR-0880 writer returned and is merged** (`271bb452` →
  main). Its lane had diverged and it merged rather than rebased; **there was
  no conflict in the end**, because the writer had already adopted dsp-006's
  comment text verbatim, so the two edits landed as one change. The residue
  question resolved itself and is **not** a gap: the cache calls
  `remove_failed_index_residue` unconditionally off the `!indexed.has_value()`
  branch, so the new refusal is cleaned like every other, pinned by
  `git_cache_contract_tests.cpp:1405`, which passes. The writer's own first
  draft made dsp-006's mistake in the other direction and its row caught it on
  2.53.0, independently.
- **`bpr-linux` returned its whole envelope and now holds two tasks**, both
  measurements, neither authorised to fix anything.
  1. [[BPR-0906]]'s first scope item — which exit paths skip the endpoint
     unlink — instructed to measure and not fix, because the choice is
     entangled with [[BPR-0895]] and must be made once for both. **The station
     added the question that was missing from my scope text and it is the
     important one:** is a survivor *reclaimed or refused* on the next start
     with the same root? The endpoint name is a digest of the normalised root,
     so that answer decides whether the accumulation is inert or harmful — and
     it decides which of the pair is the serious one.
  2. **[[BPR-0904]]'s central measurement**, because the station had spare
     capacity and said so rather than expanding task 1. It is building the
     keyed `fake_transport` as a **spike** — not as the fix — to produce the
     number the card says must be measured rather than estimated: how many
     rows break when the fixture queue stops ignoring method and path, and for
     each, whether it is a fixture that never declared its path or a row where
     the *product* asks for something nobody intended. **It is also the
     acceptance test for the approach:** does [[BPR-0896]]'s live
     `GET /graphql?per_page=100` become a red row under it? If not, the keyed
     transport is weaker than BPR-0904 assumes, and that must be known before
     anyone builds it. Scoped to `tests/github_contract/**` only, so it cannot
     collide with `bpr-win` in `tests/end_to_end/**`.
- **A Windows-adjacent offer from `bpr-linux` was declined on purpose.** It
  correctly guessed that `bpr-win`'s five-row private-descriptor family may
  have a POSIX analogue — both clauses say "nobody but me can write here" in
  different dialects. But `bpr-win` is inside that work with the mechanism in
  hand, and a second station reasoning about the same rule from the other side
  is how two stations produce two answers. Hand it the analogue *after*
  `bpr-win` returns, with the files named.
- The station also asked whether to re-run
  `test_network_isolation_structure_contract` just to confirm it green on
  Linux. Told not to: it is green on macOS at the merged head, the fix is in a
  file with nothing platform-conditional about it, and it will be observed on
  the next run that has another reason to exist.
- **Two card dispositions were made here, not by the station.** [[BPR-0881]] is
  **Done** (`90c40e49`): the Linux criterion closed it, and the number that did
  so is the in-root endpoint measuring 108 against a `sun_path` of 108.
  [[BPR-0875]] **stays In Progress** for the Windows remainder alone; its
  macOS-and-Linux line is closed by the 13/13.
- **One of the two defects `bpr-linux` reported needed no card**: its red
  `test_network_isolation_structure_contract` is the same tripwire `bpr-win`
  had already fixed, and the same one my macOS sweep hit. Three platforms met
  it, one station fixed it, and it is green at the merged head. The station was
  right to refuse to paste the expected strings in rather than confirm the
  measurements.
- **`bpr-linux` returned its whole envelope.** `ef630436` is
  merged; its tree is clean and nothing is queued for it. [[BPR-0881]]'s Linux
  criterion, [[BPR-0875]]'s thirteen Linux rows, [[BPR-0890]]'s Linux launcher
  and the full sweep are all in.
- **It then corrected two of its own claims, unprompted**, which is worth more
  than either claim was: "no `RUN_BUDGET_EXPIRED` anywhere" was false (one
  appears, on the row whose subject *is* an expired run), and "roughly
  fivefold" compared serial times against sweep conditions. The merge commit
  that landed the sweep still says fivefold; this section is the correction.
- **Both stations must reconfigure** before their next sweep: [[BPR-0901]]'s
  budget is a configure-time argument to the course script.

### Open owner asks

Unchanged from dsp-006, none decided here: [[BPR-0897]] (the release-build
mutation boundary, three answers offered); [[BPR-0653]]'s private Qt API in a
shipped target (integrator recommends accepting, with a recheck obliged on
every Qt upgrade) and its criterion-4 capture, seven steps; [[BPR-0825]] item 3
(recommend (c)); [[BPR-0644]]; [[BPR-0817]]; [[BPR-0824]]; [[BPR-0803]];
[[BPR-0800]]; [[BPR-0801]]; [[BPR-0604]]; and the [[BPR-0794]]/[[BPR-0795]]
attended packet, intact.

**New in dsp-007, and it blocks a P1.** [[BPR-0896]]'s fix cannot be shaped
without two facts about GitHub's server that this repository cannot establish
by reading:

1. does GraphQL `PullRequestReviewThread.comments` return fields sufficient for
   the exactness check the reconciliation already performs — path, line,
   commit, author, `viewerDidAuthor`?
2. would replies returned by the REST `/comments` collection satisfy that same
   check, given they carry `in_reply_to_id` and a synthesized position?

If (1) is yes the fix is a thread-scoped GraphQL observation; if (1) is no and
(2) is yes it is a REST reconciliation whose path comes from the thread's pull
request instead of the mutation's GraphQL path. **They are different work and
neither should be built before the questions are answered.** Either route
closes it: someone reading the published schema, or a user-named safe live
target. [[BPR-0897]] is adjacent but separate — it asks whether a release build
should compose a live provider at all, and it now carries the fact that a reply
could never have been written against a real server anyway.

**Also worth a decision, not urgent:** [[BPR-0905]] asks whether this tree
wants a stated rule for sizing run budgets and row timeouts, or accepts that it
does not have one. Three instances in one run is the evidence; the cheapest
answer is to make an expiry name itself in the failure headline, which
[[BPR-0901]] already did for two drivers.

### Hazards learned in dsp-007

- **A lane ref does not advance when you merge the lane.** See above. It cost
  one station its whole envelope.
- **`issue_board.py move` does not commit.** A `claim` immediately after a
  `move` refuses, because the card it is about to move is already dirty.
  Commit the promotion first.
- **Do not run anything on the machine while an intermittency experiment is
  running.** CPU contention is a confound that changes the result without
  changing what is being tested, and a run taken under unknown load is not
  comparable with one taken idle.
- **Test numbers are not identity.** dsp-006's sweep failed `#342
  application_overview_since_review_contract`; the same row is `#343` today,
  because BPR-0890 added one. Resolve names against `ctest -N`, always.
- **A card's quoted markers can come from the wrong log.** [[BPR-0901]]'s
  Defect section quoted a marker chain that appears only in another card's
  logs, and it sent four dispatches to the wrong subsystem. Quote with a
  log path and a line number or do not quote.
- **Do not run anything while an intermittency experiment runs**, and say so in
  the brief. Load is a confound that changes the answer without changing what
  is tested.
- **Look at the existing logs before constructing an experiment.** The whole of
  BPR-0901 was decidable from eleven lines already on disk. Twenty-two green
  runs were spent before anyone grepped for the marker that was in every
  failure.
- **Brief a reviewer to defeat the change, not to confirm it.** The BPR-0894
  review found two real holes in a change that passed its own new tests. A
  review that only re-runs the author's reasoning finds what the author found.
- **An acceptance record can be wrong when written.** When review falsifies
  one, correct it in a new section and say what was missed; do not rewrite it.
- **`git stash push` cannot take an untracked path.** Commit the finished
  independent thing instead of trying to park it.
- **In zsh, `$H:refs/heads/x` is not what you wrote.** `:r` is a parameter
  modifier, so the expansion silently became
  `<sha>efs/heads/linux-lane` and the push failed with a refspec that matched
  nothing. Write `"${H}:refs/heads/x"`. A literal SHA followed by `:refs/...`
  is unaffected, which is why this only appeared once a variable was used.
- **`cmd | tail` inside an `&&` chain masks the failure and the chain keeps
  going.** AGENTS.md already says not to pipe a gate; the chained form is the
  same bug wearing a different hat. Two lane pushes failed in one command and
  the command reported success. Redirect to a file and test `$?`.

## Current frontier, 2026-09-17 (run-20260916-stations, dispatch dsp-006, mandate r5): the three P1 review findings are answered, and one was answered by being overturned

Dispatch dsp-006 ends here. The dsp-004/dsp-005 protocol and hazards below still
hold except where this section corrects them.

**Heads.** Main `90bc535b`, pushed, working tree clean, 37 commits since
`026d610f`. `origin/windows-lane` `23e7eb18` and `origin/linux-lane` `618bad62`
are **both fully merged into main**; neither lane has fast-forwarded yet, and
both stations have been told to. The `project-integrator` lease is **released**.
On the Mac no worker, scout, test process or scratch worktree remains, and no
BetterPullReview process is running. Logs are `build/r2-logs/dsp6-*`.

**Both stations are working**, each on an envelope dispatched at `d6dfca2b`;
see "Station state" below, which is the first thing to read before dispatching
anything.

### Done on main

[[BPR-0879]] **P1** and [[BPR-0867]] are Done. [[BPR-0880]] **P1** and
[[BPR-0875]] **P1** are merged and In Progress for named remainders.
[[BPR-0881]] **P1** is complete on macOS and In Progress for Linux only.
[[BPR-0868]] and [[BPR-0890]] landed with one platform's half each.

**Usable behaviour gained.**

- **A thread can no longer be resolved in somebody else's pull request.** The
  session performs a pre-flight read of the thread's own pull request, compiled
  from the mutation's own request so it inherits every guard the write has, and
  refuses `undesignated_target` **before** anything is written. The reviewer's
  response-side binding is kept as a second check. Identity compares node ids
  and database ids as well as owner/name/number, because a rename moves
  `nameWithOwner` and the ids do not. A resolve costs three round trips where
  it cost one; that is the price of refusing before writing, and none of it
  races.
- **A pull request carrying a warning-class fsck object loads.** Before, Git's
  own warning was promoted to fatal, so a repository with an old zero-padded
  file mode could never be opened while GitHub serves it and `git clone`
  accepts it.
- **A second launch of a non-default macOS profile activates the running
  window.** It never could for any user name longer than one character.
- **Eleven of thirteen Windows rows go green with zero cleanup terms**, and an
  ordinary quit on Windows no longer exits 1 with a repair presentation in
  every window.
- **A row that prints its marker and then aborts now fails.** It passed before,
  on any of 208 rows carrying a pass expression.

### The three decisions dsp-006 made rather than parked

**1. [[BPR-0880]]: accept what Git accepts.** A closed, explicit
`--strict=<msgid>=ignore` list covering exactly the default WARN/INFO-severity
identifiers, error-class findings still fatal, and an error-class refusal that
names the object class instead of the bare `process_failure`. `=ignore` rather
than `=warn` because `=warn` still writes to stderr and the runner turns any
stderr on an accepted exit into `malformed_output`; with `=ignore` stderr stays
empty and [[BPR-0846]]'s exact-output rule is untouched rather than relaxed.
Encoded for the **oldest** Git across the three platforms, because Git answers
an unknown identifier with `fatal: Unhandled message id` and would fail every
import on that installation.

**2. [[BPR-0875]]: fix the harness, not the rule.** The station asked whether
the Windows parent-trust clause should be relaxed to match the POSIX one. It
should not, and the framing hides why: the POSIX clause's entire content is
"nobody but me can write here", and on Windows the only way to say that
sentence is the DACL walk. "Match POSIX" cannot be honoured literally — what it
would mean is carving out an exception for an inherited `Authenticated Users:(M)`
entry, which is exactly what makes a directory foreign-writable. The Windows
build tree really is writable by any authenticated user and the POSIX one at
0755 really is not. The rule reports a true difference.

**3. [[BPR-0881]]: move Linux's socket too**, although the card required only
macOS. The real macOS root is 94 bytes and `94 + /activate.sock` is exactly 108,
which is Linux's own `sun_path` limit and which the code refuses with `>=`.
Leaving the rendezvous in the data root left Linux one long user name from the
identical defect.

### [[BPR-0875]] is not what its card said, and this run caused it

`bpr-win` instrumented all three named sources before attempting a fix, as the
card demanded, and **none of the three fires**. All thirteen rows name one
source, 48 times:
`agent-workspace-recovery:<data-root>\agent-workspace-ownership-v1:repair-required`,
set at **startup** and only reported at the quit, which is why a whole sweep
read it as a shutdown fault.

**This run introduced it.** [[BPR-0622]]'s `c94e02b1`, dated 2026-09-17, added
`Q_OS_WIN` to the guard around the recovery post in `start_services`. Before
that Windows never called recovery at all, and the station's 2026-08-27 matrix
run has `application_workbench_composition_contract` passing in 6.90 s.

Two things a person cares about, both measured rather than argued: **the
reviewer's window layout was never at risk** (the durable layout write never
refused in any run), and **a real installation is unaffected** — the same row
rooted under the real `%LOCALAPPDATA%` parent passes with zero cleanup-source
lines while rooted in the build tree it fails. One variable, both directions.
So the card's "user-visible" framing is **test environment only**.

### [[BPR-0890]] earned its place twice on its first day

All **208** rows this build configures with a `PASS_REGULAR_EXPRESSION` were
measured, none skipped. Exactly one green row carried a non-zero exit —
`rfc0017_release_absence_negative_control`, which exits 1 by design and had that
nowhere recorded. So the macOS answer is a **well-measured zero** and the guard
is a regression net rather than a bug fix.

It then caught two things the same afternoon:

- `application_review_behaviour_contract` and
  `application_overview_since_review_contract` failing intermittently
  ([[BPR-0901]]);
- and, on the [[BPR-0880]] merge, `git_process_contract` asserting the
  **cache's** residue guarantee at the process layer. The row's pass expression
  still matched, so it would have gone **green with a failing assertion inside
  it** had the guard not landed hours earlier.

Linux and Windows halves are written up on the card with the exact change each
launcher needs; both are dispatched.

### The Windows containment question, answered once

Recorded in full on [[BPR-0872]], which the other four point at. The rule:

> A row runs contained unless it needs a kernel object an AppContainer token
> cannot reach. A row whose subject is the repository's own files, tooling or
> build metadata, and which reaches no network, buys nothing from the container
> and should be pinned `WINDOWS_UNCONTAINED` with its measured cause.

Twelve rows are now measured and pinned, and the vocabulary gained two terms it
needed: `child-process-denied` (CreateProcess refused with WinError 5 before the
tool runs, which is a different obstacle from `dos-volume-name` that Git hits
*after* it starts) and `repository-content-outside-checkout`.
**`test_network_isolation_child` is deliberately never pinned** — its job is to
prove the container denies a child, so uncontaining it would make it prove
nothing while turning the gate green ([[BPR-0903]]).

**The constraint that shapes every Windows envelope:** the gate is exact in both
directions, the keywords live in `tests/**` which a station owns, and the
dictionary lives in `scripts/**` which no station may edit. Land both in **one
commit on main** after the station has measured.

### Pins: four reseals, each with its own recorded review

`68f8152e` ([[BPR-0881]]'s diagnostic, review on the card), `c1723a3f`
(BPR-0875 naming), `d3b5a3ef` (BPR-0877 lock skip) and `d6dfca2b` (startup
naming) — the last three reviewed in their merge commit messages. Current
values: whole file `e04f598b…`, manifest `218b5e01…`, mutation branch
`a7e2081e…` **unchanged, as it has been since it was first pinned**.

**A station's printed pin values stop being true the moment you merge**, unless
the merge brings no other `src/app` change. Always reprint at the merged tree.

### Sweep status

| Platform | Result | When |
|---|---|---|
| macOS dev | **378/382** | this dispatch, `dsp6-0890-sweep.log` + `-rerun.log` |
| macOS core-only | **146/146** | this dispatch, at the merged head |
| Linux | 367/367 | dsp-005 at `2b2e5549`; **not re-swept here** |
| Windows | 309/362 at `2b2e5549`, **+11 and +3 rows measured individually** | no new full sweep |

macOS's four failures are the three [[BPR-0893]] footprint rows and one
[[BPR-0901]] intermittent. **A new full sweep on each station is the most
valuable single thing the next dispatch can ask for**, because this dispatch
changed the twelve-course data root on every platform, the temp helper's
behaviour at 56 call sites, and the Git import policy.

### Station state — read this before dispatching

- **`bpr-linux`** is **holding deliberately**. Its BPR-0880 writer is still
  running in that checkout with a dirty tree (`installed_git_process.cpp`,
  `git_process_contract_tests.cpp`, the card and the architecture doc), and the
  station will not fast-forward or dispatch a second writer under a live one.
  It will send that writer's reply, then fast-forward, then take the three
  measurements in `task-20260917-lnx-0881-0875-0890`.
- **I merged `618bad62` while that writer was in flight.** By the letter the
  pushed tip is durable truth, so the merge stands — but it was a **checkpoint,
  not a return**, and I made a same-file edit to
  `tests/git_contract/git_process_contract_tests.cpp` that the writer must now
  reconcile rather than overwrite. Restoring the assertion I removed turns
  macOS red. The open question is whether the in-flight work changes the msgid
  table or the version floors, or only the card and documentation; if it changes
  the table, main's merged copy is stale.
- **`bpr-win`** is on `task-20260917-win-0890-0877-0876`: the Windows exit-status
  marker, the suite measurement, and specifically **whether any of the thirteen
  uncontained rows exits non-zero while green** — those rows clear the isolation
  prefix entirely, so they have no launcher and no marker, and that is exactly
  where a shutdown fault is expected to live.

### Open owner asks

Unchanged from dsp-005: [[BPR-0653]]'s private Qt API in a product target
(integrator recommends accepting, with a recheck obliged on every Qt upgrade);
[[BPR-0825]] item 3 (recommend (c)); [[BPR-0644]]; [[BPR-0817]]; [[BPR-0824]];
[[BPR-0803]]; [[BPR-0800]]; [[BPR-0801]]; [[BPR-0604]]; the
[[BPR-0794]]/[[BPR-0795]] attended packet, intact; and [[BPR-0653]]'s criterion
4 capture, seven steps.

**New: [[BPR-0897]].** Establishing that no real pull request was ever mutated
across [[BPR-0879]]'s gap turned up the boundary that answer rests on. The
attended lane is gated field by field against compile-time constants, but
`select_observed_review_mutation` composes the live provider for **whatever pull
request is open** in a non-attended release build. That is presumably the
product; the card asks the owner to say so before anyone outside this repository
runs it, and offers three answers that lead to different work.

### Ten new cards

[[BPR-0894]] boundary rule over-reserves a directory variable name; [[BPR-0895]]
macOS reaps the new rendezvous directory; [[BPR-0896]] a reply's reconciliation
GET is the GraphQL root with a REST query string, so a live reply can never
reconcile; [[BPR-0897]] the release-build mutation boundary (owner);
[[BPR-0898]] invisible node and broken server read alike; [[BPR-0899]] the
layout write-failed flag cannot tell a refusal from a write in flight;
[[BPR-0900]] harness and product both write the owner-private descriptor and
nothing pins that they agree — which thirteen rows now depend on; [[BPR-0901]]
two macOS epoch fixture courses fail intermittently and alternate; [[BPR-0902]]
the device refresh course rotates once and then requires re-enrolment;
[[BPR-0903]] the container counterexample probe needs a Windows expectation, not
a pin.

### Hazards learned in dsp-006

- **Backticks inside `git commit -m "…"` are command substitution.** Two phrases
  were silently deleted from a commit message and had to be amended back. Write
  the message to a file and use `-F`.
- **Never write an envelope field from memory.** dsp-006 invented the `checkout`
  path for *both* stations. Both caught it, because `checkout` is informative;
  every other identity field would have been a refusal.
- **Resolve row names against `ctest -N`, never a card's prose.** Five of
  thirteen names lacked their `_contract` suffix, so a station's first run
  silently matched 8 of 13. A regex that quietly matches a subset is the worst
  kind of wrong.
- **"Nothing to merge" is a claim about `origin/<lane>`, not about the station.**
  Say "origin/<lane> equals base_commit; fast-forward your local lane to it".
- **Ask whether a pushed tip is a checkpoint or a return before merging it.**
  `bpr-linux` will now state which in its acceptance notes; ask the other
  station for the same.
- **A `grep | head` guard reports the pipe's exit status.** A conflict-marker
  check written that way said "markers found" when there were none.
- **Do not add a `std::string` field to a by-value seam-crossing status record.**
  Every seeding process died with `0xC0000409`; `github_application_status`'s
  layout is load-bearing and its header says so. An accessor is the answer.
- **State a child's exit status; never infer it from a PASS expression.** Two
  separate faults this dispatch were invisible until the exit status was on the
  row's own output.
- **A build directory cannot be copied to a new path** and there is no ccache
  here. `build/core-only` carries `sqlite_contract` but not `git_contract` or
  `github_contract`, which is what made two concurrent Mac slices possible.
- **Workers still burn their 56 turns.** Three of four hit the limit; all three
  resumed cleanly with their edits intact, so the limit is a checkpoint rather
  than a loss — but brief them to chain build-and-test into one command per turn
  from the first build.

### Next objectives, in order

1. **Collect both stations' replies** and merge. `bpr-linux`'s answer about
   whether the in-flight msgid table changed is the one that could invalidate
   something already on main.
2. **[[BPR-0881]]'s Linux criterion** and **[[BPR-0875]]'s twelve courses on
   Linux** — both are criteria this dispatch created and neither is measured.
3. **[[BPR-0901]]**, because an intermittent row teaches people to rerun. The
   discriminating experiment is written on the card; run it before changing
   anything.
4. **A full sweep on each station**, for the reason in Sweep status above.
5. Then [[BPR-0902]], [[BPR-0876]]'s startup-refusal family, [[BPR-0874]],
   [[BPR-0893]], and finally [[BPR-0854]] and [[BPR-0861]], which are still
   In Progress for Windows alone.

## Current frontier, 2026-09-17 (run-20260916-stations, dispatch dsp-005, mandate r5): handoff after three sweeps and five critical reviews

Dispatch dsp-005 ends here. The dsp-004 protocol and hazards below still hold.
The `project-integrator` lease is **released**.

**Heads.** Main `638cc7f9`, pushed. `origin/linux-lane` and
`origin/windows-lane` are both fast-forwarded to exactly that; all three refs
agree. Both stations are **idle** — every envelope sent in dsp-005 was
answered and merged, and nothing was dispatched after their last replies. On
the Mac no worker, reviewer, test runner, scratch worktree or test process
remains, and no BetterPullReview process is running. Logs are
`build/r2-logs/dsp5-*`.

**Round-trip pins.** Last resealed in the `2b2e5549` merge on two recorded
reviews, covering `2e71061d..2b2e5549`; exactly three commits in that range
touched a pinned app source. The mutation-branch pin still has never moved.

### The three sweeps, all at the same commit `2b2e5549`

Run deliberately on one head so they are comparable. **This was the run's
first full sweep on macOS and on Windows.**

| Platform | Result | Evidence |
|---|---|---|
| Linux | **367/367 green**, warning-clean `--clean-first` build | `docs/verification/dsp5-linux-full-sweep.md` |
| macOS | 378/381 | `build/r2-logs/dsp5-mac-full-sweep.log` |
| Windows | 309/362 | `docs/verification/dsp4-windows-merged-head-sweep.md` |

All eleven failures from the earlier Linux sweep are fixed and green.

**macOS's three** are macOS-only large-review footprint budgets inside
`if(APPLE)`, carded as [[BPR-0893]] with a baseline comparison: two predate
this dispatch, and the third is **not deterministic** — the same baseline
source passes twice in a clean worktree and fails half the time in
`build/r2-dev`, which rules out the regression reading the first numbers
suggested.

**Windows's 53** are carded by measured cause: [[BPR-0872]] (11 rows, no child
process in the container), [[BPR-0873]] (documentation rows needing
out-of-repository skill content *and* a git child), [[BPR-0874]] (5 rows,
`registry_unavailable` off the bare temporary data root), [[BPR-0875]] (13
rows failing on cleanup alone, 11 with `presentation=1`), [[BPR-0876]] (6
contained courses), [[BPR-0877]] (4 device-authorization rows),
[[BPR-0869]]/[[BPR-0870]], and [[BPR-0878]] as intake for 8 unrelated one-offs.

### Done on main in dsp-005 (7 cards)

[[BPR-0855]], [[BPR-0856]], [[BPR-0857]], [[BPR-0858]], [[BPR-0859]],
[[BPR-0860]] and [[BPR-0862]]. Evidence is on each card.

**Usable behaviour gained.** Four courses that had been stranded on Overview
now exercise what they claim ([[BPR-0863]]); the workbench composition course
names a stalled width store instead of eating its 40 s budget; the UIforETW
course proves intraline role paint in the **rasterised** viewport on macOS and
Linux with identical colours; the formatting gate is green under clang-format
22 and 23; and the conditional-branch gate reads `OK:700` on two platforms and
two toolchains.

### Mandate r5, and the five critical reviews

r5 waived the r4 stable-point gate ("start them soon regardless of windows
health"). The coordinator ran five read-only reviews at `20418bfe`: four
Claude/fable partitions and one Codex pass over the whole tree. **All five
returned.** Adjudicated per r4 — quick and safe fixed, the rest carded.

**Fifteen cards, [[BPR-0879]] to [[BPR-0893]].** The three to read first:

- [[BPR-0879]] **P1** — thread resolve/unresolve is not bound to the
  designated pull request by any server observation. Found **independently by
  two reviews**, and Codex's only blocking finding.
- [[BPR-0880]] **P1** — the cache import refuses warning-class fsck objects
  Git and GitHub accept, so a pull request containing an old zero-padded file
  mode can never load. Locally reproduced.
- [[BPR-0881]] **P1** — on macOS the activation socket path exceeds
  `sun_path` for every username longer than one character, so a second launch
  of a non-default profile can never hand off. The contract passes only
  because its fixture asserts the one condition the real root cannot meet.

Also [[BPR-0875]] is now **P1 and user-visible**: `profile_desktop.cpp:851-853`
gives a person quitting on Windows exit 1 from the same three unnamed sources.

**Two quick fixes taken** (`638cc7f9`): the workbench-window row timeout
raised 120 s to 300 s at the Windows station's measured request, and
`attended_launch_contract.py` taken off literal `/tmp`.

**Two offered quick fixes deliberately deferred**, with reasons on the cards
rather than in a commit: the device binding store's unguarded fallback post
(folded into [[BPR-0884]], because what a failed job reports is the same
decision that card already owes) and the unpinned fail-closed verdict on
ordinary rows (folded into [[BPR-0890]], because an assertion wrong in the
strict direction turns the manifest gate red on every platform at once).

### Two integration repairs worth knowing about

Both were expectations generalised from one platform, and neither station
could have seen them:

- `bpr-win` tied the strip identity's **owner** to its **title**, but the
  ladder drops the title first and the owner second, so all three
  `qt_workbench_window_scale_*` rows failed on macOS. The owner is now its own
  `width_case` field.
- At 200% the zoom/unzoom row exposed a **latent bug** the lane's new
  `rect().contains` guard revealed: the row had been passing by
  double-clicking **outside the strip entirely** (measured `point 483,15`
  against a `398x30` toolbar). It now proves the point is inside the strip.
- A reviewer caught a **cross-lane** break neither station could: [[BPR-0859]]'s
  exact 27-action strip row would have been false on Windows once [[BPR-0653]]
  landed, which adds two actions. It is now platform-aware, and `bpr-win`
  confirmed it counts Windows's 29 correctly.

### Open owner asks

- **New in dsp-005: private Qt API in a product target.** [[BPR-0653]]'s frame
  removal uses `QNativeInterface::Private::QWindowsWindow::setCustomMargins`,
  so `src/app/CMakeLists.txt` now takes QtCore's and QtGui's versioned private
  header directories under `WIN32`. The precedent in this tree is **test-only**;
  this is the first shipped target to depend on them. **The integrator
  recommends accepting it** — there is no public Qt API for removing the title
  bar while keeping the frame — but it pins the app to the Qt the build
  provides and obliges a recheck on every Qt upgrade. A refusal would be
  surgical.
- Unchanged: [[BPR-0825]] item 3 (recommend (c)), [[BPR-0644]], [[BPR-0817]],
  [[BPR-0824]], [[BPR-0803]], [[BPR-0800]], [[BPR-0801]], [[BPR-0604]], the
  [[BPR-0794]]/[[BPR-0795]] attended packet (intact), and [[BPR-0653]]'s
  criterion 4 capture — now with a seventh step from the reviewer: maximise,
  minimise from the taskbar, restore from the taskbar, confirm it returns
  maximised with the strip at the work area's top.

### Still open, with the next step

- [[BPR-0854]] and [[BPR-0861]] are In Progress **only** for Windows. Both
  pass on macOS and Linux; their Windows rows are blocked behind
  [[BPR-0875]]'s cleanup cluster and [[BPR-0876]].
- [[BPR-0653]] In Progress for criterion 4 alone.
- [[BPR-0750]], [[BPR-0794]], [[BPR-0795]] unchanged.

### Next objectives, in order

1. The three P1 review cards, [[BPR-0879]], [[BPR-0880]], [[BPR-0881]]. Each
   is user-visible and each has its evidence and a proposed shape.
2. [[BPR-0875]] — 13 Windows rows and a user-visible exit code behind one
   unnamed cause. Name the three sources before attempting a fix.
3. The rest of the Windows groups, [[BPR-0872]] to [[BPR-0878]]. Several ask
   the same question — which rows should be contained at all — and should be
   answered together.
4. [[BPR-0893]] — settle whether the macOS footprint rows measure the product
   or the machine before touching the budgets.
5. Then [[BPR-0854]] and [[BPR-0861]] can finish.

### Hazards learned in dsp-005

- **Never hand-expand a short SHA.** This dispatch fabricated plausible 40-hex
  values twice. Substitute `$(git rev-parse <short>)`, and before pushing run
  every 40-hex string in the message through `git rev-parse --verify`.
- **`issue_board.py finish --paths` takes one multi-value list.** Repeated
  `--paths A --paths B` collapse to the last, silently: a commit carried only
  the card while its message described the whole change. Check
  `git show --stat`.
- **A failed conflict resolver plus `git add` stages conflict markers.**
  Neither the board validator nor the documentation gate detects them. Verify
  the markers are gone *before* staging, and note that on a renamed file the
  markers carry a path suffix (`<<<<<<< HEAD:<path>`), which a naive pattern
  misses.
- **Four courses in this run were stranded on Overview.** Check a failing
  course's modality before anything else.
- **A station's "uncarded defect" may be a stale view.** Both stations
  reported rows already carded and fixed after their synced base.
- **Ask reviewers to falsify named claims, especially assertion *changes*.**
  That is what caught the cross-lane strip-row break and confirmed
  "stronger, not looser".
- **A guard that newly refuses an invalid input can expose a test that relied
  on the old silence.** Measure what the test was really pointing at.
- **A green sweep is not a green project.** The repo-wide formatting gate is a
  build target no ctest row observes ([[BPR-0862]], [[BPR-0865]]), and a crash
  after the marker passes any of 117 rows ([[BPR-0890]]).
- **Workers burn their 56 turns fast.** Both Mac workers hit the limit
  mid-diagnosis. Tell them to chain build-and-test into one command per turn
  from the start.

## Current frontier, 2026-09-17 (run-20260916-stations, dispatch dsp-004, mandate r4): handoff at an integrated boundary

Dispatch dsp-004 ends here, and a fresh integrator takes over. The protocol
and hazards in the dsp-001 to dsp-003 sections below still hold.

**Mandate r4.** At a good stable point the coordinator starts two read-only
whole-project critical reviews: Fable and Codex. The integrator reports only
the stable point and its main SHA. **This handoff is not a stable point:**

- both stations are still working;
- the full suite on main is not green (see below).

**State at handoff.**

- **Heads.** Main is `fe75b3a4`, pushed. `windows-lane` is at `20d99914`,
  where `bpr-win` is working. `linux-lane` is at `e794da85`, where `bpr-linux`
  is working.
- **Mac.** No worker, reviewer, scout, scratch worktree or test process
  remains. The `project-integrator` lease is released.
- **Owner's app.** PID 88792, started Wed Sep 16 19:31:44, was running at the
  start of dsp-004. At handoff (10:43 local) no BetterPullReview process is
  running. dsp-004 never signalled it or used `build/dev`.
- **Round-trip pins.** Last resealed at `fe5b4b64` for [[BPR-0861]]: reviewed
  range `08c1aa30..bb736f53` plus that change. Before that, `08c1aa30` for
  [[BPR-0848]].

**Done in dsp-004 (14 cards).**

- [[BPR-0848]], [[BPR-0847]], [[BPR-0850]], [[BPR-0849]], [[BPR-0853]];
- [[BPR-0846]], [[BPR-0836]], [[BPR-0851]], [[BPR-0852]], [[BPR-0840]].

Their evidence is on each card, and the logs are in `build/r2-logs/dsp4-*`.

**Usable behaviour gained.**

- **Haves-trimmed packs load.** A pull request update whose pack a server
  trimmed with haves now loads, on all three platforms. The import accepts
  `index-pack` exit 1 only with exact output, and only after full closure is
  verified.
- **Missing objects no longer force repair.** A missing-object refusal now
  leaves a usable cache instead of `repair_required`.
- **Connectivity refusals name their findings.** They report fsck's findings by
  kind.
- **Capsule image build.** The production capsule image build detaches its own
  leaked write-once device, or refuses. A gate pins the detach.
- **Test and gate repairs.** `git_protocol_boundary`, the Linux profile
  activation and owner admission courses, and Windows
  `epoch_maintenance_integration` are green again.

**In progress, with the next step.**

- [[BPR-0857]]: the v1 downgrade and version 7 assertions; passes on macOS.
  Needs a Linux run.
- [[BPR-0858]]: segment-overflow unavailable course plus the new
  `application_approximate_diff_public_path`; macOS 3/3. Needs Linux runs.
- [[BPR-0861]]: the probe asks the runtime for its tray; resealed. Needs
  Linux and Windows runs (the row is registered on every platform).
- [[BPR-0653]]: `bpr-win` `task-20260917-win-0653-implement`, a 902-pixel
  Windows floor and three-character labels as the last yield, per the owner's
  answer. At merge:
  - run the Qt selectors it names on macOS;
  - reseal the `src/app` pins with a recorded review;
  - criterion 4's attended capture stays an owner task.
- `bpr-linux` `task-20260917-lnx-sweep-fixes` covers [[BPR-0856]], [[BPR-0855]]
  and [[BPR-0854]]. At merge:
  - run macOS;
  - review and apply its proposed `scripts/verify_conditional_branches.py`
    patch, since `scripts/**` is outside lane authority;
  - reseal if `src/app` moved.
- [[BPR-0859]] (UIforETW intraline role paint) and [[BPR-0860]] (workbench
  composition width-store stall): Ready, Mac. Read-only scouts ruled out the
  suspected commits; the cards list the runtime candidates. Each needs
  instrumentation and a bisect from the last green run (2026-09-11), with
  `build/r2-dev` exclusive. BPR-0860 also caps its uncapped wait.

**Full-suite status.** `bpr-linux`'s first full sweep at `a1864fa2` passed
355/366. All 11 failures are carded above (BPR-0854 to BPR-0861), and 9 of them
also fail on macOS. Integration passes had run focused selections, so these
rows had been red since 2026-09-08 to 09-15. The fixes landed so far are
BPR-0857, 0858 and 0861 on macOS.

**Owner asks (exact), unchanged from dsp-003.**

- [[BPR-0825]] item 3: where the Windows owner-private descriptor definition
  lives, (a), (b) or (c). The integrator recommends (c).
- [[BPR-0644]]: whether a remembered device grant should survive a
  locked or denied Keychain, or an offline launch; plus the attended relaunch
  more than 8 hours after sign-in.
- [[BPR-0817]]: the exit code under a group-writable data parent, and whether
  the refusal must show at Home.
- [[BPR-0824]]: option (a) is provisional.
- [[BPR-0803]], [[BPR-0800]], [[BPR-0801]], [[BPR-0604]].
- The attended packet for [[BPR-0794]] and [[BPR-0795]]
  (`docs/verification/reviews/rfc-bpr-0013-attended-v001/operator-packet.md`,
  bundle `build/app-release-dsp3`) is intact.
- [[BPR-0653]]'s width floor is answered: 902 on Windows.

**Next objectives, in order.**

1. Integrate the two station replies above as they arrive.
2. Send the next `bpr-linux` envelope: Linux runs of BPR-0857
   (`sqlite_anonymous_epoch_authority`, `sqlite_epoch_catalog_storage_contract`),
   BPR-0858 (both diff courses) and BPR-0861
   (`application_native_notification_attended_contract`).
3. Send the next `bpr-win` envelope, after BPR-0653: the Windows runs of
   BPR-0861 and BPR-0858's two rows.
4. Mac: [[BPR-0860]], then [[BPR-0859]].
5. Run a macOS full-suite sweep and a second Linux sweep. Then, with the
   stations idle, report the r4 stable point with the main SHA.

**Hazards learned in dsp-004.**

- **Sweep the full suite periodically.** Focused integration passes let 11
  rows stay red for days.
- **zsh eats `$H:refs/...`** as a history modifier. Write `${H}:refs/...` in
  push refspecs.
- **`ls` is aliased to a coloured listing.** Use `/bin/ls` or explicit file
  names in command substitution.
- **Workers held behind a quiet-machine gate burn turns waiting.** Tell them
  when the gate is clear, and do not edit shared sources during their
  measurement runs.
- **Reseal every change to pinned `src/app` sources**, even test-only fixture
  access, with a recorded review. An `rfc-reviewer` before the build caught a
  second cause in BPR-0857.
- **Stations may not edit `scripts/**`.** Ask for proposed patches and apply
  them on main.
- **Git for Windows leaves `tmp_pack_*` READONLY|ARCHIVE.** The cache's exact
  removal handles it.

## Current frontier, 2026-09-17 (run-20260916-stations, dispatch dsp-003, mandate r3): handoff at an integrated boundary

Dispatch dsp-003 ends here, and a fresh integrator takes over. The station
protocol from dsp-001 and dsp-002 still holds. The state at handoff:

- **Heads.** Main is `0a3eff37`, pushed. `windows-lane` and `linux-lane` are
  fast-forwarded to it.
- **Stations.** Both are idle: every envelope sent in dsp-003 was answered
  and merged.
- **Mac.** No worker, reviewer, scratch worktree or test process remains.
  The owner's app (PID 88792, started Wed Sep 16 19:31:44) was never touched.
  The `project-integrator` lease is released.
- **Round-trip pins.** Last resealed at `0c58092e`, for BPR-0843.

**Done in dsp-003 (31 cards):**

- [[BPR-0814]], [[BPR-0622]], [[BPR-0802]], [[BPR-0823]], [[BPR-0828]], [[BPR-0830]];
- [[BPR-0837]], [[BPR-0820]], [[BPR-0826]], [[BPR-0812]], [[BPR-0832]], [[BPR-0834]];
- [[BPR-0841]], [[BPR-0829]], [[BPR-0813]], [[BPR-0831]], [[BPR-0809]], [[BPR-0844]];
- [[BPR-0838]], [[BPR-0842]], [[BPR-0835]], [[BPR-0845]], [[BPR-0833]], [[BPR-0843]];
- [[BPR-0839]] (refetch kept), [[BPR-0673]].

Their evidence is on each card. Mac integration logs are
`build/r2-logs/dsp3-int{1..12}-*`.

**Usable behaviour gained, in brief:**

- Every tests-enabled automated run gets a private owner-private profile, and
  no dev-preset course reaches the owner's default profile.
- The Unix agent supervisor no longer races its child's process group.
- Private stores bind SQLite's held handle to the admitted name on Windows.
- The Windows manifest gate reports zero findings.
- The large-review refresh pulse fell from 93.5 ms to about 65 ms.
- Several stale courses now name their failed terms.

**Attended preparation, ready (human part open).** See
[the rfc-bpr-0013-attended-v001 packet](../../verification/reviews/rfc-bpr-0013-attended-v001/operator-packet.md).
It uses the tests-disabled bundle in `build/app-release-dsp3`, built from
`94f6f885` and signed with the Developer ID identity; its executable SHA-256
is `96fa16fe…`. Three questions are ready and protected in the checkout
session `cbd48b91…`:

- `BPR-0794-cli-selected-diff`;
- `BPR-0795-loading-visible`;
- `BPR-0644-device-relaunch-8h`.

The owner must quit their own BetterPullReview first.

**Integrator decisions made in dsp-003:**

- [[BPR-0820]]: `--test-application-data-root` is now refused only with a
  fixture.
- [[BPR-0833]]: the capability rows are pinned rather than changing the
  storage ancestor rule.
- [[BPR-0841]] item 3: declined.
- [[BPR-0839]]: the refetch is kept.
- [[BPR-0829]]: the course waits for retention and counts 4 leases.

**Owner asks (exact).**

- [[BPR-0825]] item 3: where should the one Windows owner-private descriptor
  definition live? (a) AdapterGit calls a new SQLite validator; (b) the
  SQLite adapter includes the agent header; (c) a new neutral header-only
  target with a new architecture-boundaries owner. The integrator recommends
  (c).
- [[BPR-0644]]: should a remembered device grant survive a launch whose
  Keychain answers locked or denied, or whose network is down (including an
  offline launch after the access token expired), instead of being erased
  under BPR-0541's fail-closed rule?
- [[BPR-0817]]: (1) under a group-writable data parent, should closing the
  last window exit 1, or exit 0 with the refusal only in the app? (2) Is the
  agent-panel refusal enough, or must it also show at Home?
- [[BPR-0824]]: the seam option (a) was chosen provisionally by dsp-002; the
  owner may override.
- Unchanged: [[BPR-0803]], [[BPR-0800]], [[BPR-0801]], [[BPR-0604]].
- The attended packet above.

**Next objectives, in suggested order:**

1. [[BPR-0846]] (P1, suspected): a fetch that sends haves may get a pack the
   cache import refuses (`index-pack --check-self-contained-and-connected`
   exits 1). Reproduce it offline with a local `upload-pack` that honours
   haves before changing anything, and give any import-rule change a
   critical review.
2. [[BPR-0847]] (Linux): `application_profile_activation_public_path` fails
   at "owner orderly drain". This goes to `bpr-linux`.
3. The next `bpr-win` envelope: check that BPR-0843's `main.cpp` compiles in
   the Windows developer build, and run
   `application_epoch_fixture_noncanonical_root_cause` if it is registered
   there (it is registered on macOS and Linux only).
4. P3 Backlog: [[BPR-0840]] (agent composition rows under load on macOS),
   [[BPR-0836]] (the capsule production image leak) and [[BPR-0848]]
   (`main.cpp` format; reseal).
5. [[BPR-0750]] still waits on RFC-BPR-0008's order.

**Hazards learned in dsp-003.**

- Commit card-only changes with `git commit -- <paths>` while a candidate is
  staged.
- After `issue_board.py move`, stage every file whose links the move
  repaired.
- Do not rebuild `build/r2-dev` while another agent runs its binaries.
- Keep the Mac quiet during pulse measurements.
- On macOS, agent composition rows fail under a load average above about 15.
- On Windows, a contained process cannot use a `QTemporaryDir`: its DACL is
  owner-only, without the container SID.
- A Windows AppContainer cannot walk the stores' ancestor chain. Rows that
  need it are pinned `dos-volume-name`.
- `application_review_round_trip_composition_contract.cmake` run with `-P`
  needs `BPR_APPLICATION`. Use `-DBPR_PRINT_PINNED_DIGESTS=ON` only to print
  the pins.

## Current frontier, 2026-09-17 (run-20260916-stations, dispatch dsp-002, mandate r3): handoff at an integrated boundary

Dispatch dsp-002 ends here, and a fresh integrator takes over. The station
protocol and hazards in the dsp-001 section below still hold. Main is
`780d0a1c`, pushed. `linux-lane` is fast-forwarded to it. `windows-lane` is
not: `bpr-win` is building on it from `de58ebc6`. At return there are no Mac
worker subagents, reviewers, scratch worktrees or test processes, and the
`project-integrator` lease is released.

**Protocol additions learned in dsp-002.**

- **Round-trip pins.** When a merge or fix moves the round-trip contract's
  pins, reseal them by the procedure at the top of
  `tests/end_to_end/application_review_round_trip_composition_contract.cmake`:
  run `-DBPR_PRINT_PINNED_DIGESTS=ON`, and name the reviewed range in the
  commit that records the reviews. The last reseal is `6a58b6f8`
  (`7e42643f..fe629c90`); the mutation-branch pin has never changed.
- **Worker turn limits.** Workers stop at a 56-turn limit. Tell them to
  chain build and test into single blocking commands, and resume the same
  worker with SendMessage.
- **Orphaned test apps.** A course that hangs until its harness timeout can
  leave its test app running and costing CPU. Before timing claims, check
  `pgrep -fl build/r2-dev/src/app` and scratch worktree paths, and kill only
  those, never PID 88792 (the person's app from `build/dev`).
- **Stale regexes.** Several contiguous-block pass expressions were stale
  (BPR-0828, BPR-0829); a new status line needs a regex line.
- **Default profile.** Courses without a fixture or a private data root
  contend for the default profile while the person's app is open: [[BPR-0820]].

**Done in dsp-002.**

- Mac regressions: [[BPR-0815]], [[BPR-0818]], [[BPR-0819]], [[BPR-0821]],
  [[BPR-0827]], [[BPR-0822]].
- Station cards finished on main: [[BPR-0816]], [[BPR-0806]], [[BPR-0824]].
- [[BPR-0799]], Done on `bpr-linux`.

**Integrated station work.**

- `windows-lane` BPR-0622 slices (a) `e11f19eb`, (b) `dd88b9c9` and
  (c)(d) `c94e02b1`, each merged `--no-ff` with a Mac run.
- `linux-lane` `4e4fd6b9` and `3b968fcc`.
- Logs: `build/r2-logs/dsp2-int{1..9}-*`.

**Open, with the next step.**

- [[BPR-0622]], In Progress: the Windows criteria are implemented and
  verified, a critical review found no blocker (follow-ups in [[BPR-0825]]),
  and the Mac and Linux coverage of slices (a) and (b) passes. It waits only
  on Linux coverage of slices (c) and (d), in `task-20260917-lnx-confirm`
  step 1. Then finish it.
- [[BPR-0802]], In Progress: the macOS half is done at `49d7df69`. It waits
  on Windows confirmation (`task-20260917-win-hardening` step 1: expect 24
  findings, one fewer) and Linux confirmation (`lnx-confirm` step 1).
- [[BPR-0826]], In Progress: the macOS criterion is met. It waits on the
  Windows rerun, which is NOT yet in the Windows envelope; add it to the
  next one. The Linux rerun is in `lnx-confirm`.
- [[BPR-0823]] and [[BPR-0828]], In Progress: macOS met; they wait on the
  Linux reruns in `lnx-confirm`.
- [[BPR-0829]], In Progress: the history line is fixed. The course still
  fails because `PUBLIC_PREVIEW_SMART_ACQUISITIONS` is 2 on macOS against the
  expected 1. Next, a Mac diagnosis: a duplicate acquisition, or a count that
  depends on timing.
- [[BPR-0644]], Parked human-review: the fixture criteria pass after a
  critical review and a correction (`1925d0cc`, `3ad87ee4`). Pending: the
  attended relaunch and the owner question below.
- [[BPR-0750]], [[BPR-0794]], [[BPR-0795]]: as in dsp-001. The attended read
  still needs its focused set rerun and a tests-disabled bundle rebuilt.

**Active station envelopes (writers active).**

- `bpr-win` `task-20260917-win-hardening`, base `de58ebc6`:
  1. BPR-0802 Windows confirmation;
  2. [[BPR-0812]];
  3. [[BPR-0825]];
  4. [[BPR-0813]].
- `bpr-linux` `task-20260917-lnx-confirm`, base `780d0a1c`:
  1. confirmation runs for BPR-0622 (c)(d), BPR-0802, BPR-0826, BPR-0823,
     BPR-0828, BPR-0829 (x5, with the acquisition count) and BPR-0644's
     courses, recorded in `docs/verification/dsp2-linux-confirmation.md`;
  2. [[BPR-0830]].

**Owner asks (exact).**

- [[BPR-0817]] criterion 2, already relayed:
  (1) Under a pre-existing group-writable data parent, should closing the last
  window still exit 1, or exit 0 with the refusal shown only in the app?
  (2) Is the agent-panel refusal enough, or must it also be visible at Home?
- [[BPR-0644]]: after a restart, should a remembered device grant survive a
  launch whose Keychain answers locked or denied, or whose network is down
  (keep the material, fail visibly, retry at the next launch)? Today such a
  launch erases it, under BPR-0541's fail-closed rule. The same question
  covers an offline launch after the access token expired. Also: the
  attended relaunch more than 8 hours after a device sign-in.
- [[BPR-0824]]'s seam question was answered by the integrator with option
  (a); the owner may override.
- Unchanged: [[BPR-0803]], [[BPR-0800]], [[BPR-0801]], [[BPR-0604]], and the
  [[BPR-0794]]/[[BPR-0795]] attended read.

**Next Mac objectives, in order.**

1. Integrate the station replies above, then finish BPR-0622, BPR-0802,
   BPR-0823, BPR-0826 and BPR-0828 on their confirmations.
2. [[BPR-0829]]'s acquisition-count diagnosis.
3. [[BPR-0820]], the default profile.
4. [[BPR-0809]], [[BPR-0814]], [[BPR-0673]] (refresh-32 pulse margin
   93.3 ms against 100).
5. Attended preparation for BPR-0794/0795 and BPR-0644's relaunch: one
   tests-disabled bundle at a current head.

## Current frontier, 2026-09-17 (run-20260916-stations, dispatch dsp-001-r2, mandate r2/r3): handoff at an integrated boundary

This is the run's live frontier; other cards link here. Mandate r2: this
integrator owns all authorized work and supersedes the earlier sessions.
Amendment r3: keep `.mask.json` on the SSH tap. Main is `743c59d7`, pushed.
Mac preflight is ready again, and both JaegAW stations report ready.

**Station protocol, as learned in this run.** Replies reach the integrator only
through the coordinator's relay.

- Stations fetch only their own branch.
- The integrator merges a lane with `--no-ff`, which keeps the station SHAs its
  evidence cites. Portable changes get a Mac run before `main` is pushed.
- The integrator then fast-forwards `origin/<lane>` to `main`, and the station
  runs `git merge --ff-only origin/<lane>`.
- Always resolve full SHAs with `git rev-parse`. One envelope was refused for a
  hand-expanded SHA.
- A lane that fetches a newer `main` counter makes `issue_board.py` refuse. The
  fix is the fast-forward above, never a ref deletion or a rewind override.

**Active station envelopes (writers active):**

- `bpr-win` `task-20260917-win-workspace-recovery`, base
  `743c59d79b09f18d4178f0702a0f377f53082ccd`: [[BPR-0622]]'s Windows
  implementation (journal identity/ACL, job lifetime measured before design,
  Git job assignment, startup recovery wiring). Evidence goes to
  `docs/verification/BPR-0622-windows-native-evidence.md`. The card is
  integrator-owned, and the envelope says stop on a design decision. Planned
  next Windows envelopes: [[BPR-0653]], then [[BPR-0812]] and [[BPR-0813]].
- `bpr-linux` `task-20260917-lnx-build`, base `20a72bf7`, in this order:
  1. [[BPR-0816]] (GCC 15.2 `-Werror`, portable sources; verify on the Mac at merge).
  2. Stock-build reruns for [[BPR-0622]] and the [[BPR-0806]] Linux storage
     courses. These are evidence files only; both cards are integrator-owned.
  3. [[BPR-0817]] (owner-only data folders). An exit-code question may park.
  4. [[BPR-0799]] criterion 1 rerun under umask `0002`.

**Done in this run:** [[BPR-0747]], [[BPR-0692]], [[BPR-0666]], [[BPR-0804]],
[[BPR-0805]], [[BPR-0807]], [[BPR-0808]], [[BPR-0810]], [[BPR-0811]], [[BPR-0798]] (Windows),
[[BPR-0730]] (Windows) and [[BPR-0731]] (Linux).

- The AC-11 row below now links the Windows and Linux inventories. Both match
  their dev builds with identical node counts.
- Comparing them against the attended macOS inventory remains a closeout step.

**Integrated evidence.**

- `build/r2-logs/int2-ctest.log` at `a5667197`: 39 of 40 pass. The only failure,
  `test_network_isolation_manifest`, reports [[BPR-0802]]'s findings.
- Critical reviews of BPR-0806 and BPR-0807 found no blockers. Their
  follow-ups are carded or fixed.

**Integrator-owned open cards:**

- [[BPR-0806]], In Progress: waits only on the Linux storage courses.
- [[BPR-0622]], In Progress: Linux evidence recorded; Windows implementation
  not started.
- [[BPR-0750]], In Progress by its own recorded decision: waits on RFC-BPR-0008
  order.
- [[BPR-0794]] and [[BPR-0795]], In Progress: the attended native read of
  `EpicGames/UnrealEngine#1773` through the GitHub CLI profile. First rerun
  their focused set and rebuild the tests-disabled bundle, because main has
  moved.
- [[BPR-0799]], Blocked: on [[BPR-0817]].

**Next Mac objectives, needing no person, in suggested order:**

1. [[BPR-0815]] (P1, `application_device_authorization_panel_course` fails at a
   clean `53a0a747`) and [[BPR-0818]] (P1, `application_review_behaviour_contract`
   loses review and issue-comment roots, red at a clean `8b2bea5b`). Then
   [[BPR-0819]] (stale round-trip composition digest, reseal only with review
   reconciliation).
2. [[BPR-0802]]'s Mac half. On the Mac the manifest gate now reports only its
   findings.
3. [[BPR-0644]]'s fixture criteria (P1).
4. [[BPR-0809]], [[BPR-0814]] and [[BPR-0673]].

**Owner asks not yet recorded on cards:**

- [[BPR-0803]]: CLI way-in inside the device flow.
- [[BPR-0800]]: Dif corner versus the AI tools slot.
- [[BPR-0801]]: whole-queue refresh.
- [[BPR-0604]]: selection, and live point-cost measurement.
- [[BPR-0644]]: attended relaunch more than 8 hours after sign-in.

**Hazards.**

- A user BetterPullReview app runs from `build/dev` (PID 88792). Use
  `build/r2-dev`, never rebuild `build/dev`.
- The rule amendments in BPR-0807 are security-relevant for [[BPR-0788]].
- The stopped DIF session's superseded draft is preserved as `8ee34d1f` on
  `claude/bpr-dif-integration-v1-d932e2`, in the worktree moved to
  `/Users/jaeger/Documents/GitHub/BetterPullReview.superseded-worktrees/bpr-dif-integration-rfc-3c2904`.

## Current frontier, 2026-09-12 (d31, mandate r20): attended-preparation milestone complete

Run `attended-prep-2026-09-10`, dispatch d31, producer
`/root/integrator_d29`, mandate r20. The adopted
`attended-preparation-pack-head` definition is
`sha256:201de49a086f8f809fea06d6749caf3fecb43e7fa8c78a960f55af31b64bfee5`;
its renewed nine-card obligation fingerprint is
`sha256:3d54a5fe84a1c059e08af0b1c2437ad7ca09a5d09ed22d9244677f643e177630`.
The assignment is structurally valid with resolved references. **Disposition:
milestone complete** for the nine authored cards, with the four exits checked
separately below. This does not close any RFC.

1. **Digest-bound bundle and revisions — pass.** The original pack-head
   [v006 identity](../../verification/reviews/rfc-bpr-0028-attended-v006/session-identity.json)
   records the tests-disabled bundle built at `d1ba791f`, executable SHA-256
   `cc2afa7fb84d443a1bba93373b990be83748be20451fa0b6adde45cc6fae4c1c`.
   Its fifteen ready candidates include all nine authored cards, and every
   candidate in that bound set names `cc2afa7f` in its revision. The final
   [v011 identity](../../verification/reviews/rfc-bpr-0028-attended-v011/session-identity.json)
   binds the remaining BPR-0646 candidate at source `16e242a5` to executable
   SHA-256 `dfd1c4cae5739559b143138c28a0277bc9f5e7c40ef1556bee47d967cff2f187`;
   the preserved local bundle still matches that digest, verifies with
   `codesign --verify --deep --strict`, and has `BUILD_TESTING=OFF`. The v006
   shared build path was subsequently reused; its digest is historical
   packet evidence, not a claim that the old executable still occupies it.
2. **Pack integration — pass by affected-check rerun.** The d30 set in
   `build/d30-milestone-integration.log` passed four Settings Qt scales,
   three workbench Qt scales and the workbench application composition
   contract (eight of nine). The general application composition contract
   initially failed on a source-manifest repin and a profile lease held by a
   detached d28 app. Commit `ce6f1836` repinned the intentional fixture
   source and isolated automated packaged evidence runs; the affected
   contract then passed 1/1 in
   `build/d30-automated-root-regression-contract.log` while the other app
   remained open. `build/d30-final-boundaries.log`, the documentation gate
   and `git diff --check` passed. This is a corrected split pass, not a
   single green nine-selector batch. Later changes to Release automated
   evidence, tests, whitespace and documentation do not alter the attended
   UI path. The separate full strict-tidy failure is recorded below; tidy
   is not one of this exit's named integration checks.
3. **Prepared session and pointers — pass.** The v006 packet prepared the
   bound set at sequence 259 with fifteen ready candidates and a checked
   offline-page digest. The final v011 packet prepared the one remaining
   candidate at sequence 372 with its own checked offline page. Both
   [[RFC-BPR-0028]] and [[RFC-BPR-0018]] point to v011 and retain the v006
   origin for session `cbd48b91000b00f1e72e1ad62c7c384c`.
4. **Verdicts and card acceptance — pass.** The live board returns all nine
   authored cards in `60-done/`; their 28 acceptance rows all record Pass.
   The session has matching-fingerprint decisions for the ten questions
   covering those nine cards (BPR-0674 had two). Its final BPR-0646 decision
   `4feeaa9e-5705-4c53-a516-6fc311de305d` accepted the exact v011
   candidate at sequence 373; hand-back sequence 374 recorded `pending []`.
   Earlier rejects remain attached to their historical revisions. No bound
   owner ask remains.

The [BPR-0750](BPR-0750-close-rfc-0029.md)
direct-check gaps in rows 2, 4, 5 and 7 remain separate from this milestone;
RFC-BPR-0029 stays Accepted behind Proposed RFC-BPR-0008. RFC-BPR-0018 and
RFC-BPR-0028 remain Proposed/open pending their own closeout and Windows/Linux
station evidence on [[BPR-0730]] and [[BPR-0731]]. Do not repeat the declined
[[BPR-0735]] editor observation or [[BPR-0746]] cursor probe. The next planned
`close-rfc-0018` milestone is not yet eligible because its station criteria
remain open; coordination must select the next eligible bounded outcome.

## Current frontier, 2026-09-12 (d30, mandate r20): integration evidence and binding renewal

Run `attended-prep-2026-09-10`, dispatch d30, producer
`/root/integrator_d29`, mandate r20, adopted milestone
`attended-preparation-pack-head`. The d30 assignment definition was
`sha256:201de49a086f8f809fea06d6749caf3fecb43e7fa8c78a960f55af31b64bfee5`
and obligation was
`sha256:33d953bc570693a53c5098acd8d175a6655c8402aa1d053c2ec3847c9c4ce33b`.
Board progress changed the live card fingerprints, so the d30 assignment is
stale and needs renewal before any formal milestone completion claim. All
nine authored cards are now in `60-done/`; their original acceptance and
human verdict records remain on their own cards. The protected v009/v010/v011
operator packets and signed bundles were not edited. The v006 session
identity binds all nine authored cards to its signed tests-disabled bundle
and offline review page; both RFC-BPR-0018 and RFC-BPR-0028 point to the
current v011 session and retain the v006 origin.

The current-head direct pass built the app and focused tests. Host, SQLite
migration, epoch Qt and GitHub pager selectors passed 4/4 in
`build/d30-direct-checks.log`; the composed selector initially failed on a
fixture query-order mismatch, then passed 1/1 after correction in
`build/d30-composed-correction-test.log` (200.36 s). The integration set had
Settings Qt at four scales, workbench Qt at three scales and workbench
composition passing 8/8 in `build/d30-milestone-integration.log`. Its
application composition contract initially found an intentional source
manifest drift and `PROFILE_ALREADY_OPEN` against a separately running d28
packaged app. Commit `ce6f1836` repins the manifest and gives packaged
automated self-test/render runs a private temporary profile root. The
contract then passed 1/1 in
`build/d30-automated-root-regression-contract.log` while that app remained
open. This is a repaired split pass, not a green single nine-selector batch.
Final `check_boundaries`, documentation gate, 64 documentation script tests,
touched formatting, Python compilation and `git diff --check` passed. No
unresolved source failure remains in this selector set.

The broad pre-push coding target at this head first found three unrelated
baseline files with clang-format alignment drift; the proposed edits were
identical under `git diff --ignore-all-space` and were committed separately
as `14723207`. The rerun in `build/d30-coding-checks-final.log` passed the
648-file format check, then strict clang-tidy failed with 117 diagnostics
across 33 of 383 translation units. A line-level comparison against both
`origin/main` and the d30 starting head `b3314d90` found zero diagnostics
on added or modified C++ lines. This global tidy debt is recorded as a red
check, not claimed as a clean full target; the focused and integration
selectors above remain green.

RFC-BPR-0029 direct-check bookkeeping on [[BPR-0750]] now has rows 3, 6 and
8 passing, rows 2, 4, 5 and 7 partial, and the exact v011 owner verdict on
row 9. BPR-0750 stays In Progress, and RFC-BPR-0029 stays Accepted behind
Proposed RFC-BPR-0008. RFC-BPR-0018 and RFC-BPR-0028 stay open for their
separate Windows/Linux station and closeout criteria. [[BPR-0735]] remains
blocked by the owner-declined editor observation and [[BPR-0746]] by the
owner-declined probe; do not repeat either ask. [[BPR-0730]] and [[BPR-0731]]
own the Windows/Linux stations. The detached d28 packaged app (PID 22988 at
this check) is outside this invocation's ownership and was left running; all
d30 builders, selectors and fixture writers are stopped. The next bounded
step is coordinator renewal against the moved-card source, followed by an
exit-by-exit milestone disposition without changing the protected packets.

## Prior frontier, 2026-09-12 (d30, mandate r20): protected-link migration and direct checks

Run `attended-prep-2026-09-10`, renewed dispatch d30, producer
`/root/integrator_d29`, mandate r20, binding definition
`sha256:201de49a086f8f809fea06d6749caf3fecb43e7fa8c78a960f55af31b64bfee5`
and obligation `sha256:33d953bc570693a53c5098acd8d175a6655c8402aa1d053c2ec3847c9c4ce33b`.
All nine authored obligations and four exits remain bound. Starting clean main
HEAD `b3314d90`; the d29 accepted v011 decision and signed digest remain
unchanged. The current board obstacle is BPR-0646 and BPR-0729's old relative
links in protected review packets. The documentation gate already recognizes
a relocated historical review-card link only when a unique canonical card
filename exists in the board, so d30 is exercising a narrowly scoped board
configuration that leaves those packets untouched while repairing other live
links and then running the full documentation gate. No packet or bundle is an
owned write path.

Three non-overlapping rfc-worker slices own RFC-BPR-0029 direct checks in
parallel: SQLite migration row 7 (`tests/sqlite_contract` and related SQLite
source); host/transport rows 2, 3 and 8 (`tests/application`,
`tests/github_contract` and related host/transport source); composed/Qt rows
4, 5 and 6 (`tests/end_to_end/epoch_portable_occurrence_public_path.py`,
epoch Qt test and `epoch_line.cpp`). The integrator alone owns board,
acceptance, docs and commits. Workers use isolated build output. Current
source paths and command results are pending; next discriminator is whether
both accepted cards can move without packet-byte changes, followed by the
current-head integration selector pass. No live pull request mutation is in
scope.

The protected-link migration succeeded: [[BPR-0646]] and [[BPR-0729]] are
Done at `58921104` and `73371ea1`. The v009/v010/v011 operator-packet
SHA-256 values remain `c094952c…`, `6ce8d3f1…`, and `13a86dd5…` exactly as
before the moves. The full documentation gate and scoped board validation
pass, with old packet locators resolved by the existing unique-card rule.
The v006 session identity binds all nine authored cards (ten candidate
questions) to one tests-disabled bundle digest and an offline page; RFC-0018
and RFC-0028 now point to v011 and that original page. The three worker
source/test slices have stopped editing. Changed paths are confined to the
epoch host/pager tests, SQLite behavior test, epoch Qt test, composed fixture
script, and tests-guarded `src/app` fixture seams. The next command is one
current-head focused build followed by direct selectors and the milestone
integration set. Workers hold no active command and remain available for
correction. The d30 assignment fingerprint is mechanically stale after the
two board finishes; a renewed binding is required before completion claim.

## Current frontier, 2026-09-12 (d29, mandate r20): v011 verdict received

Run `attended-prep-2026-09-10`, dispatch d29, producer
`/root/integrator_d29`, mandate r20, remains bound to plan
`ux-redesign-pack-0028` / milestone `attended-preparation-pack-head`, definition
fingerprint `sha256:201de49a086f8f809fea06d6749caf3fecb43e7fa8c78a960f55af31b64bfee5`.
The nine authored obligations and four exits remain in force. Clean synchronized
main baseline was `1d3f7d48`; no source changes are planned for this verdict
intake. The owner accepted the exact v011 `BPR-0646-workbench-chrome` candidate
at sequence 373 (decision `4feeaa9e-5705-4c53-a516-6fc311de305d`, fingerprint
`b6641835…`, empty note) on revision `app-release-d28-dfd1c4ca-2026-09-12.7`.
Sequence 374 handed back with no pending candidates. The protected
[v011 identity](../../verification/reviews/rfc-bpr-0028-attended-v011/session-identity.json)
binds the signed tests-disabled bundle to executable SHA-256 `dfd1c4ca…`;
the local executable digest still matches. [[BPR-0743]] AC4 is recorded pass
and the card is Done at `641dae15`. [[BPR-0646]]'s remaining attended criterion
is recorded pass on the exact v011 verdict; its v008 failure remains in the
card's historical body. A dry-run move to Ready refuses because it would
rewrite the protected v009 operator packet's relative link. The card remains
in Backlog with all current acceptance passing, pending safe path-preserving
bookkeeping. There is no further owner question on this criterion.
The d28 composed and focused checks remain applicable because source inputs
are unchanged. [[BPR-0729]]'s accepted v010 criterion remains Parked on
`immutable-review-link`; [[BPR-0735]] and [[BPR-0746]] retain the owner's
declined observations. [[BPR-0730]]/[[BPR-0731]] remain own-station work.
[[BPR-0750]] was promoted and claimed for a separate RFC-BPR-0029 closeout
audit at `4e29df56` and `79c99936`. Its row 1 two-catalogue course and row 9
owner verdict have direct passing evidence; rows 3 and 8 have partial evidence,
and rows 2 and 4–7 still need direct checks. RFC-BPR-0029 therefore remains
Accepted, with canonical predecessor RFC-BPR-0008 still Proposed. RFC-BPR-0028
and RFC-BPR-0018 remain open; no closeout is inferred from the v011 verdict.
The audit is committed at `cb6ed0c7`; the v011 candidate protection was
released at session sequence 375 after its decision was recorded. All nine
authored milestone cards have consumed owner verdicts: eight are Done and
[[BPR-0646]] is all-pass in Backlog solely because the protected-link guard
blocks its move. The four bound exits are not all established: digest-bound
candidate pages and preparation occurred incrementally across historical
bundles, the d26 pack integration and d28 focused/public-path checks passed,
but no single current-head pass proves the literal combined exit 2; exit 4's
Done-or-exact-remaining-ask wording is not met by an all-pass card whose board
move is blocked. This milestone remains partial rather than being relabeled
complete. The next bounded objective is a path-preserving board migration
for BPR-0646 and BPR-0729, or the missing RFC-BPR-0029 direct checks under a
renewed independent scope. No source/build writer, review host or child agent
remains active. Local commits are ahead of origin and remain unpushed
because the tracker and RFC closeouts are unfinished.

The d29 assignment's BPR-0646 source fingerprint was `sha256:338160eb…`.
After the accepted criterion and BPR-0743 link were recorded, the pure
assignment helper observes `sha256:3c66c6ee…` for that source and an
obligation fingerprint of `sha256:33d953bc…`. This reflects d29 progress,
not a change to the authored nine obligations or four exit texts. A successor
must renew the same milestone binding explicitly against this live state;
the old d29 binding must not be presented as current acceptance proof.

## Current frontier, 2026-09-12 (d28, mandate r19): historical comparison in progress

Run `attended-prep-2026-09-10`, dispatch d28, producer `/root/integrator_d28`, mandate r19, remains bound to `ux-redesign-pack-0028` / `attended-preparation-pack-head`, definition fingerprint `sha256:201de49a086f8f809fea06d6749caf3fecb43e7fa8c78a960f55af31b64bfee5`. All nine authored obligations and four exits remain open as recorded below. Clean synchronized main baseline: `9a0845be`. BPR-0743 is In Progress; BPR-0646's unchanged handle criterion is prepared for owner judgment on a new signed bundle; no verdict is inferred.

The baseline composed two-catalogue occurrence test passed before edits (`ctest --preset dev --output-on-failure -R '^application_epoch_portable_occurrence_two_catalogues$'`, 1/1). It proves identity/order and the current negative condition: historical event nodes have no local comparison epoch. The d28 objective is exact historical Git retention with recovery, a derived comparison base explicitly distinct from unknown historical base, a same-transaction occurrence binding during imported epoch finalization that leaves latest live authority intact, and a composed signed-out comparison course. REST after-head evidence alone still cannot equate a same-head local observation with an event.

Commit `d1bcd1cf` carries the Git/cache, catalogue, core, host, Qt and composed-course historical comparison slice. It fetches an evidenced head under accepted live PR authority, derives a comparison base, journals/retains the pair, and binds the exact event to its imported epoch without moving latest live authority. The expanded signed-out course passes (100.73s, `build/logs/bpr-0743-d28-expanded-composed.log`): an empty-timeline live seed followed by unchanged-head reopen imports the new events; an independent fresh catalogue produces the same portable identities and order; an unfetchable first head remains unbound while a later fetchable event gains an epoch; the real Qt gesture moves the handle and resolves comparison. Repeated-head event IDs remain distinct and may each retain separate epochs without equating them to the same-head live observation. The final focused build and 8/8 selected tests pass (`build/logs/bpr-0743-d28-final-focused-{build,tests}.log`); documentation, board, touched format and architecture boundaries pass. Critical read-only review found no further concrete blocker in persistence, recovery or core ownership. [[BPR-0743]] AC2/AC3 now pass; AC4 and [[BPR-0646]]'s unchanged attended handle criterion remain pending on the owner's observation. The tests-disabled Release build first exposed a fixture-only variable outside its guard; `16e242a5` fixes it, and both dev and isolated Release builds pass. The latter is locally signed, verifies strictly, and has executable sha256 `dfd1c4cae5739559b143138c28a0277bc9f5e7c40ef1556bee47d967cff2f187`. In a live macOS public-preview window on NVIDIA/stdexec#1846, three evidenced nodes appeared; dragging the right bracket from current to Aug 9 settled an exact pair Ready for diff. The local account state in that window was not independently verified; the composed course separately pins signed-out behavior. The [v011 packet](../../verification/reviews/rfc-bpr-0028-attended-v011/operator-packet.md) is ready and protected at sequence 372 with one unchanged BPR-0646 criterion, fingerprint `b6641835…`. It labels incomplete history and does not claim the nodes are a complete upload count. Owner verdict is still pending; v009/v010 protected bytes remain unchanged.

## Current frontier, 2026-09-12 (d27, mandate r19): one-pane verdict received

Run `attended-prep-2026-09-10`, dispatch d27, producer
`/root/integrator_d27`, mandate r19, remains bound to plan
`ux-redesign-pack-0028` milestone `attended-preparation-pack-head`, definition
fingerprint `sha256:201de49a086f8f809fea06d6749caf3fecb43e7fa8c78a960f55af31b64bfee5`.
All nine authored card obligations and four milestone exits remain in force.

The v010 attended session recorded the owner's explicit **accept** for
`BPR-0729-one-pane-layout` at sequence 367, decision `0f6d7348`, on the
exact protected revision `app-release-a6dc1565-2026-09-12.6` and fingerprint
`70a2a9417b2532450cf66f71228fbabe1f627de748ed8ea25767cecec875228c`.
The sequence-368 hand-back had no pending candidates and added no verdict.
[[BPR-0729]]'s final signed-bundle criterion is now evidenced; the d26
engineering checks remain the evidence for its other criteria. This does
not decide [[BPR-0646]]'s epoch-handle criterion.
All nine BPR-0729 acceptance records now pass, but its board path remains in
Parked with `parked_on: immutable-review-link`: `issue_board.py finish --dry-run`
would rewrite the protected v010 operator packet's relative link to the card,
and a bare move would fail the documentation link gate. The packet bytes are
unchanged; this is a tracker migration constraint, not another human question.

[[BPR-0743]] remains In Progress under Accepted RFC-BPR-0029. Its portable
occurrence/partial-history slice is committed at `2df4d110`: a bounded
independent timeline read, SQLite v4 lossless sidecar, real two-catalogue
A→B→A public path, and honest Qt ambiguity/unknown-base labels. Four
false-association counterexamples were reproduced red before a conservative
host correction; the final critical re-review found no concrete blocker on
that correction. The final dev build and 9/9 focused selectors pass
(`build/logs/bpr-0743-d27-conservative-{dev-build,focused}.log`). Touched C++
format, architecture boundary, core epoch policy, documentation and board
gates pass. The global format target exposed three untouched baseline files;
the optional hermetic composition contract was stopped during its Release
rebuild after the composed public paths already passed. No live PR was mutated.

REST after-head events cannot prove that a same-head local observation is the
same update. The host retains both and declares overlap ambiguous; no unsafe
event/local binding or supersession is claimed. Historical events have no
durable local comparison epoch yet, so [[BPR-0646]]'s unchanged handle
criterion is **still not judgeable** and no attended bundle or owner question
is prepared. The next bounded slice is cached historical retention, explicit
historical append that preserves live authority, atomic binding on durable
finalization, honest derived-base labels and a composed comparison course.
RFC-BPR-0018 and RFC-BPR-0028 remain open, with all nine authored obligations
and four exits preserved below. [[BPR-0735]] remains Blocked on the owner's
declined editor observation; [[BPR-0746]] remains Blocked on the declined
probe, and [[BPR-0730]]/[[BPR-0731]] remain own-station work.

## Current frontier, 2026-09-12 (d26, mandate r18): one-pane bundle ready

Run `attended-prep-2026-09-10`, dispatch d26, producer
`/root/integrator_d26`, mandate r18 (r16 at dispatch), plan
`ux-redesign-pack-0028`, milestone `attended-preparation-pack-head`, definition
fingerprint `sha256:201de49a086f8f809fea06d6749caf3fecb43e7fa8c78a960f55af31b64bfee5`.
The clean synchronized main baseline was `3e1be52e`. No original work item
or exit was removed from the authored plan.

[[BPR-0729]]'s accepted one-pane Read/Review implementation is committed at
`3dcbf237`; its board/evidence disposition is `83d7e676` and it is Parked
for its one remaining owner criterion. The first absent-surface assertion was
red before implementation. Independent review found and reproduced a pure
deletion round-trip jump (top row 70 → 8) and a missing composed-GitHub
thread test; both are corrected, and row 70 remains exact at all three Qt
scales. The final focused set is 14/14 green
(`build/logs/bpr-0729-d26-final-focused.log`). Both application composition
contracts and the unchanged diff public path pass
(`build/logs/bpr-0729-d26-final-contracts.log`). The other nine selectors
from d25's pack integration pass pass at this source head
(`build/logs/bpr-0729-d26-pack-remainder-tests.log`); with the current
composed/window and composition results, the same eighteen-selector coverage
is green at the d26 source. Boundaries, strict tidy, touched-file format,
documentation and whitespace gates are clean. The 100/125/200% one-pane
source, gutter and projected-thread crops were inspected.

The tests-disabled bundle built from `83d7e676` is locally signed and verifies
with executable sha256
`a6dc1565908cfacc5eb7d573d8d05983c51aa10780db2052e1c138682870eaf2`.
[Attended v010](../../verification/reviews/rfc-bpr-0028-attended-v010/operator-packet.md)
has one digest-bound candidate, `BPR-0729-one-pane-layout`, revision
`app-release-a6dc1565-2026-09-12.6`, fingerprint
`70a2a9417b2532450cf66f71228fbabe1f627de748ed8ea25767cecec875228c`,
ready and protected at sequence 366. Its offline page is self-contained;
no host, watch or owner verdict exists. One question asks the owner to judge
Review in one pane and the empty-side ribbon shapes. The supplied renders
are same-source offscreen evidence, explicitly not a signed-bundle verdict.

RFC-BPR-0029 is Accepted at `5d5df7cd` under mandate r17; [[BPR-0750]]
has its immutable accepted base and [[BPR-0743]] is Ready for the next
independent epoch product slice. [[BPR-0735]] records the mandate-r18 answer
verbatim, **"unsure, don't care"**, as no confirmation; it is Blocked on
`owner-declined-editor-observation` without another routine ask.
[[BPR-0646]]'s epoch-handle criterion is still unjudgeable until BPR-0743
lands. The bound milestone therefore remains **partial**: the current-head
integration pass supports exit 2, while exits 1 and 3 do not yet cover all
nine plan candidates and exit 4 still needs its exact card verdicts/asks.
RFC-BPR-0018 and RFC-BPR-0028 remain open. BPR-0730 and BPR-0731 remain
own-station work. All d26 leaf results have been collected; source writers,
builds, checks and attended processes are quiescent. The next bounded
engineering objective is BPR-0743 under Accepted RFC-BPR-0029, while the
BPR-0729 candidate's exact source paths and bundle installation stay
protected for its owner verdict.

## Intermediate frontier, 2026-09-12 (d26, mandate r18): BPR-0729 claimed, revised RFC accepted

Run `attended-prep-2026-09-10`, dispatch d26, producer
`/root/integrator_d26`, mandate r18 (r16 at dispatch), plan `ux-redesign-pack-0028`, milestone
`attended-preparation-pack-head`; assignment definition fingerprint
`sha256:201de49a086f8f809fea06d6749caf3fecb43e7fa8c78a960f55af31b64bfee5`.
The clean baseline was `3e1be52e` on main, synchronized with origin.
[[BPR-0729]] is dependency-ready and was promoted (`f29e01d2`) and claimed
(`383f233a`). Its accepted one-pane surface is the current bounded slice.

The integrator owns the Qt View action and composed-window tests, plus board,
verification and commits. `rfc_worker_bpr0729_preference` owns the core,
application and SQLite preference path and its tests.
`rfc_worker_deep_bpr0729_surface` owns the diff-workbench/link-map Qt surface
and its direct tests. `rfc_scout_bpr0729_surface` completed read-only intake.
The one-pane surface's first runnable assertion failed on the absent unified
source with the build green (`build/logs/bpr-0729-d26-surface-red-{build,test}.log`);
the owners are implementing the paths before one combined focused build.

The owner answered the revised RFC-BPR-0029 acceptance ask **yes** in mandate
r17. Its accepted source base is `5d5df7cd`; [[BPR-0750]] was reserved at
`8e5fe049` and opened at `1ac3132f` for the closeout. [[BPR-0743]] is Ready.
Epoch product work remains a subsequent safe
slice after BPR-0729 ownership clears. In mandate r18 the owner answered
[[BPR-0735]]'s editor-observation ask, verbatim, **"unsure, don't care"**.
This is not confirmation that editor noise is gone; its AC3 remains pending.
The card is Blocked on `owner-declined-editor-observation`, and the same
question will not be asked again by default. The milestone's
original nine obligations and four exits remain bound; this intermediate
card progress closes none of them.

## Current frontier, 2026-09-12 (d25, mandate r16): integrated slices and exact remaining asks

Run `attended-prep-2026-09-10`, dispatch d25, producer
`/root/integrator_d25`, mandate r16, plan `ux-redesign-pack-0028`, milestone
`attended-preparation-pack-head`. The clean baseline was `8302a401` on main,
28 commits ahead of origin. The v009 session handed back at 362 and released
protections at 363. No session host, watch or process remains. Every d25
child result is collected; writers and commands are quiescent.

The owner **did not accept RFC-BPR-0029 as written**. The revised
[RFC-BPR-0029](../../rfcs/RFC-BPR-0029-machine-independent-epoch-identity-update-history.md)
(`2f151966`) carries A2's A→B→A as two evidenced occurrences sharing a
head, A3's labeled GitHub event or consistently selected commit dates, and
the prohibition on treating surviving commits as push boundaries or a
derived comparison base as the original historical base. Existing catalogue
rows remain lossless even when host/event evidence cannot yield a portable
key. The RFC stays **Proposed**; [[BPR-0743]] stays **Blocked** and [[BPR-0646]]
waits behind it. The one remaining owner ask is whether to accept the *revised*
RFC. No epoch product work or RFC-0029 closeout ID was created.

Three slices are **Done**:
- [[BPR-0748]]: owner-approved conditional macro purple, with operators,
  literals, headers, attributes, builtin-test operands and `#define` bounds.
  Source `a8b0394a`, finish `3c25b3a9`. The first red showed
  `__cplusplus` near-grey; independent review found two more counterexamples
  and the same syntax owner reproduced and corrected both. Final five checks,
  format and tidy pass (`build/logs/bpr-0748-d25-correction-*`).
- [[BPR-0749]]: one-pixel rule centered at `(width - 1) / 2` in the native
  handle (macOS 7 → column 3; Fusion 4 → column 1). Source `b00c276e`, finish
  `24f7e2ed`. Style-aware raster red before fix, required Qt scales green
  after (`build/logs/bpr-0749-d25-{red-test,green-tests-2}.log`). Cursor
  delivery remains [[BPR-0746]]'s **Blocked, owner-declined-probe** issue.
- [[BPR-0736]]: real Return and Enter queue key events dispatch the selected
  row once; invalid selection and mouse activation are covered. Source
  `7e53ff78`, finish `d240806a`; red and three-scale green logs are
  `build/logs/bpr-0736-d25-{red-test,green-tests}.log`.

[[BPR-0735]] is **Parked on human-review** (`48296f42` implementation,
`0e37a9c3` partial finish). The three named clangd checks load the dev
compile database and report none of the missing headers/types. Full check
mode exits 3 on unrelated Homebrew clangd code-action tweak self-tests on
two files; the Qt test file exits 0. Its sole remaining ask is the owner's
observation that the editor noise is gone in one named file. No attended
packet is needed.

At source head `d240806a`, the pack-head integration pass is **18/18 green**:
both application composition contracts, workbench and Settings Qt paths,
syntax and architecture boundaries (`build/logs/d25-pack-head-integration.log`).
The documentation gate and `git diff --check` pass. Plan exit 2 is evidenced
at this source head. Exits 1 and 3 still need a current-head app-release
bundle and digest-bound attended packet; exit 4 still holds BPR-0646's exact
human ask. No v010 packet was prepared because BPR-0646 is not yet a
candidate. RFC-BPR-0018 and RFC-BPR-0028 remain open; BPR-0730 and BPR-0731
stay on their Windows/Linux stations.

The next independent large slice is [[BPR-0729]]'s accepted one-pane Read
surface, suitable for a fresh bounded engineering context; it was not
claimed in d25. The next human actions are the revised RFC-0029 acceptance
answer and, independently, BPR-0735's editor observation. The d24 frontier
below remains evidence for prior work not superseded here.

## Intermediate frontier, 2026-09-12 (d25, mandate r16): owner answers received; engineering in progress

Run `attended-prep-2026-09-10`, dispatch d25, producer
`/root/integrator_d25`, mandate r16, plan `ux-redesign-pack-0028`, milestone
`attended-preparation-pack-head`. The clean dispatch baseline was `8302a401`
on main (28 commits ahead of origin). The v009 attended session handed back at
362 and released protections at 363; it has no host, watch or process.

The owner **did not accept RFC-BPR-0029 as written**. A2 says A→B→A is two
occurrences sharing a head, without inventing an occurrence GitHub cannot
evidence. A3 selects GitHub event time where present, a consistently selected
commit timestamp otherwise, explicit labels distinguishing commit dates from
push times, local first-seen kept separate, and deterministic display ordering
that does not claim equal timestamps establish push order. RFC-BPR-0029 stays
Proposed; BPR-0743 stays Blocked and BPR-0646 waits on it. A revised proposal
and its remaining owner ask are in preparation; no epoch product work is
authorized by these answers.

The owner answered **yes** to BPR-0748's separate syntax question, with the
specific operator/literal/operand and `#define` boundaries in mandate r16.
BPR-0748 was promoted and claimed (`a3c9ae4a`, `4a767cef`); its bounded syntax
slice is owned by `rfc_worker_bpr0748_syntax`. BPR-0749 was promoted and
claimed (`6bbb49be`, `90aff668`); its independent seam slice is owned by
`rfc_worker_bpr0749_seam`. Both workers own only their named source/test paths;
the integrator owns documentation, acceptance, commits, checks and push.
No worker result has yet been accepted. Do not start overlapping writers.

The d24 frontier below remains the evidence for prior work until superseded by
this dispatch's completed results.

## Current frontier, 2026-09-12 (d24, mandate r15): the epoch work has its own RFC, and BPR-0742 is Done

Dispatch `attended-prep-2026-09-10` d24 (mandate r15), same milestone
`attended-preparation-pack-head` of plan `ux-redesign-pack-0028`. The owner
answered d23's one question in chat: "It's own rfc". The RFC work changed no
product source. Nothing is pushed.

### RFC-BPR-0029, Proposed and not accepted (`10af12bc`)

[RFC-BPR-0029](../../rfcs/RFC-BPR-0029-machine-independent-epoch-identity-update-history.md)
records BPR-0743's design pass as proposed decisions, and the owner's sentence
("An Epoch is an update to the PR, …") as its motivating requirement.

**How the id was chosen.** RFC ids in this repository have no counter and no
receipt; the receipts under `docs/issues/_reservations` are for card ids only.
0029 is the next free number, and `git log --all -S RFC-BPR-0029` is empty. No
closeout card is allocated while the RFC is unaccepted, as with RFC-BPR-0021
and RFC-BPR-0028.

**What it settles, as proposals:**
- **Relationship.** It supersedes RFC-BPR-0003 in part, and only once accepted:
  what identifies an epoch, and what base OID and observed time mean for an
  imported epoch. RFC-BPR-0003 got a pointer and stays Auto-Verified.
- **A correction to the design pass.** "A digest over the authority scope"
  cannot mean `pull_request_authority_scope_id`, which is random and partitioned
  by account. The inputs are GitHub's numeric pull request id, a host term, and
  the head OID. `git_host_id` is not a provider value in the paths examined.
- **A gap the design pass left.** It asserts a cross-machine order but defines
  no ordering key, and after imports the ordinal order is local.

**What it carries open.** Seven questions:
1. head collision;
2. unobserved base-branch events;
3. paging a large timeline;
4. merge-base validation;
5. the date, and with it the order;
6. the unbuilt site list (a search adds `src/app/src/large_review_fixture.hpp:305`);
7. the host term, and what backfill can compute.

**Three owner asks, one question each.** A1 is acceptance, A2 is the same
commit twice, A3 is an update's date. Nothing is built before A1 is answered
yes, and A2 and A3 fix the identity and the order.

### The board (`17cbd086`, `37f9ba90`)

- [[BPR-0743]]: rfcs `["RFC-BPR-0029","RFC-BPR-0028"]`. It is off human-review
  and **Blocked on `rfc-bpr-0029-owner-asks`**. AC1 stands; AC2 to AC4 are
  unchanged.
- [[BPR-0646]]: `depends_on: ["BPR-0743"]`, now in **Backlog**. On this board a
  parked card cannot hold an unfinished dependency, and nothing about the card
  waits on the owner. Its handle criterion is unchanged, and it returns to
  human-review when BPR-0743 is Done.
- RFC-BPR-0028 got a dated pointer, because its AC-02 waits on this work.
  RFC-BPR-0018 needs no change.
- The v009 packet's two links were repaired in their own commit, paths only,
  following d23's precedent.

Checks at `37f9ba90`:
- `verify_documentation.py` prints DOCUMENTATION_OK.
- Board `validate` is valid across 741 cards.
- `queue --rfc RFC-BPR-0003` is 60 of 60 done again.
- `queue --human-waiting` no longer lists either card.

### BPR-0742 is Done (`7c6e2ca9` the work, `3b80eea2` the finish)

An unreadable workbench layout store now says so. Composition passes each
window its one availability decision (`!WorkbenchPersistenceAvailable`). The
window keeps "The saved workbench layout is unavailable." on its status line
past the projections that clear a host failure. BPR-0718's fallback is
unchanged: composed defaults, and the store left untouched.

- **Reproduced red first**, against the product code at `004ee4ec` with only
  the course's new assertion: `status=No pull request is open in the
  workbench.` (`build/logs/bpr-0742-d24-red.log`).
- **Integration pass, 13 of 13**, on the tree that was committed
  (`build/logs/bpr-0742-d24-integration.log`):
  - `application_composition_contract`, 143.89 s;
  - `application_workbench_composition_contract`, 22.06 s;
  - `application_review_round_trip_composition_contract`, 11.22 s, with its two
    source digests repinned for the `github_application.cpp` change;
  - the layout, restart and multi-window courses;
  - `qt_workbench_window` and `qt_composed_github`, each at three scales;
  - `architecture_boundaries`.

  clang-format is clean on the five touched C++ files. **Both application
  composition contracts stay green.**
- **Not rerun:**
  - The review-restoration course (about 586 s): nothing it asserts reads the
    status line, and its store is readable.
  - The `coding-format-check` target, which d23 recorded red at HEAD for three
    unrelated files.
- **The slice owner stopped at its turn limit** before returning. No process of
  its was left running. The integrator inspected its diff and logs, adopted the
  partial work, and ran the verification above itself.

### Commits, in order

All carry the pack's `Co-Authored-By: Claude Opus 5` trailer. No history was
rewritten.
- `10af12bc`: RFC-BPR-0029
- `17cbd086`: the v009 packet links, paths only
- `37f9ba90`: BPR-0743 and BPR-0646 rebound
- `51dac838`: claim BPR-0742
- `004ee4ec`: the frontier before BPR-0742
- `7c6e2ca9`: BPR-0742's work
- `3b80eea2`: finish BPR-0742
- then this frontier

### Noticed, not created

Two shells running `find / -name bpr-0746-cursor-probe.log`, started by an
earlier dispatch of this coordinator session, were still running throughout.
They write nothing to the repository, and this dispatch left them alone.

### Next, in order

1. **RFC-BPR-0029's three asks, to the owner.** A1 is acceptance, A2 is the same
   commit twice, A3 is an update's date. Nothing epoch-related can be
   implemented before A1. [[BPR-0743]] is Blocked on the asks, and [[BPR-0646]]
   waits on BPR-0743.
2. [[BPR-0749]] when the seam is next touched, and [[BPR-0748]] as its own
   attended candidate.
3. No v010 packet has been prepared, and nothing forces one. Neither
   RFC-BPR-0018 nor RFC-BPR-0028 became closable.

d23's frontier follows. It is still authoritative on everything not corrected
above.

## Frontier, 2026-09-12 (d23, mandate r14): the probe refused, the contract green, BPR-0743 back with one question

Dispatch `attended-prep-2026-09-10` d23 (mandate r14), same milestone
`attended-preparation-pack-head` of plan `ux-redesign-pack-0028`. Four cards
moved, one new card, one design pass recorded and escalated, and the milestone's
exit criterion 2 is green for the first time in three dispatches. No product
source changed; one test course did. Nothing is pushed.

### What the owner's message did, and on what basis

His message, in chat, in full: "I don't care to do any cursor probe, i've
already confirmed that the cursor is working correctly now." He wrote it after
being told that the probe log was never captured, that his v009 accept rested on
a criterion whose objective half was missing, and that d22's reconciliation is
an account and not a diagnosis. The message makes two separate statements, and
they land differently.

**[[BPR-0739]] is Done (`1abfb9c5`), on his word, and the card says so.** AC3
("the owner resizes the conversations drawer on the signed bundle with the
cursor visible throughout") has only ever had one possible witness, and nothing
here can observe his screen. This time the criterion was put to him and he
answered it knowing it was disputed. That is not the v009 aside, which stays
refused as a verdict. The report is also bound to a known binary, and that was
checked:
- The probe instance was still running as pid 72017, and its file hashes to
  `c2381f32…`, the v009 candidate revision exactly.
- `diagnostics.log` showed no other instance.
- `git diff --stat 35438244 aff9fc64 -- src/adapters/qt` is one file, with zero
  lines matching cursor, splitter, handle, `underMouse` or `paintEvent`, so it is
  the same program he rejected in v008.

**What it rests on is written on the card:** his word and nothing else. There is
no log, no measurement and no product change on this path, and the reversal
against v008 is unexplained. BPR-0746's hypothesis 1 predicts exactly what he
described (fresh entries work, a resting pointer decays), so the close does
**not** refute it. One report of the arrow returning reopens the card.

**[[BPR-0746]] does not close. It is Blocked on `owner-declined-probe`
(`0e092e30`).** Closing it on the same sentence would be wrong, because its two
open criteria do not ask him for anything:
- AC1 asks for a log.
- AC2 asks for a check asserted against the hypothesis that log would select.

He has declined the only instrument that produces either, and no agent can stand
in for him: every hypothesis turns on what AppKit does on the next *real*
pointer motion. AC2 is recorded as unobtainable, not pending. What landed at
`bbc4db98` checks the admissibility of the card's evidence, not cursor delivery,
and is not recorded as satisfying AC2. AC4 passes on BPR-0739.

The card leaves human-review because nothing is waiting on him: he answered.
Blocked is the lane that says the work cannot proceed and names why. A new
`parked_on` token would have needed a tracker-rule change, and `human-review`
would have been false. Nothing is refuted. The three hypotheses and the
corrected absolute-path command are kept.

**`handleWidth` is measured, not left open.** `PM_SplitterWidth`, read off the
shipped Qt 6.10.3 in a scratch program under the offscreen platform, is **7 for
the macOS style and 4 for Fusion**, and the offscreen default style is `fusion`.
The "seven pixels" both cards quote is right for the signed bundle, and d22's 4
is Fusion, so there is no contradiction. The consequence is that the Qt suite
measures a style the product does not ship. That, together with his unanswered
"it wasn't drawn in the right location" (`rule_edge()` puts the rule in the hot
zone's boundary column), is **[[BPR-0749]]** (new, Backlog, P3).

### The composition contract ran, and what it settled

The owner quit the probe instance ("I quit the running instance"; shutdown
logged at 17:45:33Z). That was verified before anything ran: pid 72017 was gone,
no BetterPullReview process existed, and nothing held the catalogue or
`activate.sock`.

`application_composition_contract` **passed in 172.22 s on the first attempt**,
with nothing changed. That demonstrates d22's diagnosis: the block was the
profile lease and nothing else.

**[[BPR-0744]] is Done (`dc231174`).** All 11 AC3 tests were rerun at `f953ce9c`
rather than reused, although `git diff --stat bbc4db98 HEAD -- src tests
packaging` is empty.

**Milestone exit criterion 2 is green at the pack head.** Beyond those 11, 13
more passed (`build/logs/d23-exit2-integration.log`):
- `application_workbench_composition_contract`, the second composition contract
- `qt_workbench_window_scale_{100,125,200}` and `qt_composed_github_scale_{100,125,200}`
- `qt_settings_window` and its three scales, and `qt_settings_effect_boundary`
- `sqlite_workbench_settings_storage`

`architecture_boundaries` and the documentation gate also pass.
`coding-format-check` stays red only for the three files already red at HEAD,
none of them touched here.

**[[BPR-0748]]'s broadening question is untouched** and is not implemented on
the strength of the accept.

### BPR-0743: the design pass, and the exact ask

**Parked on human-review (`b61e9c05`), AC1 pass, nothing implemented.** The full
pass is on the card (`f953ce9c`), and a short note is on RFC-BPR-0003.

**The signed-out obstacle does not exist.** The probe was run with no
credential: `GET /repos/NVIDIA/stdexec/issues/1846/timeline` returned **HTTP
200** against the 60-per-hour budget. It names both force pushes with GitHub-side
ids and the head each moved *to*. One of those heads, `6ffbd0ac`, is absent from
the commits list and still fetchable, which demonstrates the "commits aren't
updates" claim rather than asserting it.

The same response exposed three limits:
- The REST event has **no before-commit**. GraphQL has one and is refused
  anonymously.
- Array position is not an ordering key, and `committed` dates are author dates.
- An ordinary push produces no event at all.

**The decisions:**
1. **Derivation.** A digest over the authority scope and the **head OID**,
   carried beside `review_epoch_id`. It is not the event `node_id`, which exists
   only for force pushes and which the catalogue excludes by design. It is not
   the base/head pair either, because the base is time-dependent even for a live
   read. A force push back to an earlier head would give two updates one
   identity; that is open and recorded.
2. **Ordinal.** Not redefined. Redefining it can't buy portability while
   Incarnation stays in the id. Two of its seven serializations are beyond the
   catalogue's reach (git refs on disk, and MCP and agent-launch bindings already
   emitted). Density is an adapter guarantee that paging depends on.
3. **Identity can be made stable; population cannot be made equal.** A head seen
   live and then force-pushed over can't be recovered by any other machine.
4. **The base OID is mandatory and origin-blind** (`epoch_transition.cpp:2199-2218`),
   and the timeline supplies none for a historical update.
5. **Migration.** Neither re-mint nor renumber. Add a column, backfill it from
   the stored head, and let the import append. This works because a historical
   epoch already advances neither live ordinal.

**The conditional fires, on kind rather than size.** Identity, base OID, observed
time and the schema version all move: four of the six things RFC-BPR-0003 pins.
**The ask, one question:**

> The epoch work you asked for changes what RFC-BPR-0003 says an epoch *is* —
> what identifies it, what its base OID means when GitHub only remembers the
> head, and whether its timestamp is when you saw it or when it was pushed.
> Do you want it as an RFC of its own, or as an amendment recorded against
> the closed RFC-BPR-0003?

[[BPR-0646]] stays parked with its handle criterion unchanged, and it can't be
asked until this lands.

### BPR-0741 is Done; BPR-0742 was deliberately not started

**[[BPR-0741]] (`aab99d2b` the work, `e72a1347` the finish).** The red was
reproduced at HEAD before any change. The course now signs in from the landing
card before its first paste, and it passes in 80 s against 180 s. The layout and
restoration courses still pass (the restoration course at 586 s against 1800 s,
its normal runtime). The integrator checked the claim that no pin was dropped,
separately from the slice owner: one assertion was replaced by four, and every
other message is identical and in the same order.

**[[BPR-0742]] is not started.** It is a P3 product change in the composition
path, the same `github_application.cpp` branch the two composition contracts
exercise, and those contracts went green for the first time this dispatch. It
belongs in a fresh context, not the tail of this one. It stays Ready and nothing
blocks it.

### Commits, and attribution

In order:
- `0e092e30`: BPR-0746 Blocked, BPR-0749 carded
- `1abfb9c5`: finish BPR-0739
- `f953ce9c`: the design pass
- `db2f416f`, `4428f162`: protected packet links, paths only
- `dc231174`: finish BPR-0744
- `b61e9c05`: park BPR-0743
- `aab99d2b`: BPR-0741's work
- `e72a1347`: finish BPR-0741
- then this frontier

Every commit here uses `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`,
per the pack's standing rule. No history was rewritten and the attribution
question is not re-raised.

### Next, in order

1. **The owner's answer on BPR-0743**: its own RFC, or an amendment. Nothing
   epoch-related is implementable until then, and [[BPR-0646]] waits behind it.
2. **[[BPR-0742]]**, in a fresh context. Keep both application composition
   contracts green.
3. **[[BPR-0749]]** when the seam is next touched, and [[BPR-0748]] as its own
   attended candidate with two pictures.
4. No v010 packet was prepared and none is forced. Neither RFC became closable:
   nothing touched RFC-BPR-0018, and RFC-BPR-0028 still holds BPR-0743, BPR-0646,
   BPR-0729, BPR-0653, BPR-0736, BPR-0746 and BPR-0749.

d22's frontier follows. It is still authoritative on everything not corrected
above.

## Frontier, 2026-09-12 (d22, mandate r13): the v009 verdicts are consumed

Dispatch `attended-prep-2026-09-10` d22 (mandate r13), same milestone
`attended-preparation-pack-head` of plan `ux-redesign-pack-0028`. The four
v009 verdicts are read, reconciled and landed. Two cards are Done, three keep
their human-pending disposition with the reasons recorded, one new card
carries a question that was not answered, and one product change landed.

**Every verdict was verified before it was used**, not taken on trust: all
four decisions are `actor: human` and each decision fingerprint equals both
the live candidate's and the one in
[session-identity.json](../../verification/reviews/rfc-bpr-0028-attended-v009/session-identity.json).
None is stale.

| Candidate | Outcome | What it did |
|---|---|---|
| `BPR-0734-read-one-pane` | accept, empty note | [[BPR-0734]] **Done** (`80762210`), [[BPR-0745]] **Done** (`51dd2af2`), RFC amendments landed, [[BPR-0729]] rewritten |
| `BPR-0739-seam-hover` | select, "Drop it" | source change landed; [[BPR-0739]] stays Parked, AC3 still `fail` (`b8fb5983`) |
| `BPR-0744-directive-ranges` | accept, empty note | AC4 `pass`; AC3 still blocked; question carried to [[BPR-0748]] (`bdcd563d`) |
| `BPR-0746-cursor-probe` | accept, note | criterion **not** satisfied; stays Parked (`d32a3117`) |

Commits: `bbc4db98` (the work), `6ac7d344` (packet link repair, paths only),
then the five board commits above.

### The contradiction, reconciled

His note on the hover verdict said "now that the cursor is fixed", and
[[BPR-0746]] exists because it is not. Both reports are his and neither is
discarded.

**It is not the binary.** `git diff --stat 35438244 aff9fc64 -- src tests` is
six files, all of it [[BPR-0744]]: the classifier, the appearance roles and
three test or fixture files. `git diff 35438244 aff9fc64 -- src/adapters/qt`
contains no line matching cursor, splitter, handle, `underMouse` or
`paintEvent`. The executable he ran is `c2381f32…`, recomputed on disk. On
this path the v008 and v009 bundles are the same program.

**The escape route was tested and is false.** The obvious way out — that the
lit rule in his v008 screenshot was stale paint and never proved his pointer
was on the handle — was written as a test assertion and went **red**:
`Qt::WA_Hover` is set on the handle, arrival costs one repaint and departure
another, and `underMouse()` follows both, at scales 100 and 200. The
assertion was rewritten to the measurement rather than the measurement
explained away. So BPR-0746's refutation of BPR-0739's cause 3 keeps its
foundation, and that measurement is now in the suite so it cannot rot.

**What is left is the gesture, and it is hypothesis 1.** v007 ("flickers only
for a brief moment and the returns back to the normal cursor") and v008 (a
pointer held still for a photograph) are a pointer **at rest**. v009's
candidate instructed him to "move the pointer **on and off** that rule to see
both states", so he was **re-entering**. Hypothesis 1 predicts exactly that:
Qt sets the cursor once on enter, AppKit reverts it on the next real motion,
and Qt never re-asserts — every fresh entry works and every rest decays to the
arrow. That is an **account, not a diagnosis**. It is what the missing log was
written to discriminate, hypotheses 2 and 3 are not excluded, and nothing was
implemented on it.

### The log still does not exist, and why

He ran the probe. Pid 72017 is the bundle's executable, started 16:06:37Z from
`ttys001`, piped to pid 72018 `tee build/logs/bpr-0746-cursor-probe.log`. Both
were still alive at 16:57Z. `lsof` on 72018 shows cwd
`/Users/jaeger/Documents/GitHub` — one directory above the checkout — and open
descriptors only fd 0 on the pipe and fds 1 and 2 on the tty. **There is no
output file and there never was.** The output exists only in that terminal's
scrollback. His "Dunno about the test you had me run" is what that looks like
from his side.

The packet caused it: the copy label read "run this in a terminal, in the
repository" over a value with an absolute executable path and a **relative**
`tee` path. Fixed at the authoring site — `AGENTS.md` now requires every path
in an attended candidate's command to be absolute, and requires one question
per candidate criterion, which is the other defect this round exposed. The
corrected command, absolute throughout, is on [[BPR-0746]]:

    QT_LOGGING_RULES='qt.qpa.input.mouse*=true' \
      /Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview \
      2>&1 | tee /Users/jaeger/Documents/GitHub/BetterPullReview/build/logs/bpr-0746-cursor-probe.log

### Two empty notes, read as narrowly as they deserve

**BPR-0734.** An empty-note accept answers no question; it accepts the
candidate's four criteria as written. Two of those criteria named a
consequence and asked him to refuse it, and no refusal came, so **Review shows
one pane too** and the one-pane gutter has **no whole-file overview**. But the
page did not show either: direction v7 has four states and **none of them is
Review**, its own prose offers the alternative reading with the ask, and
neither a pure insertion nor a pure deletion is drawn, so the "one ribbon
rather than two" case is words too. Accepted-as-written, not
accepted-as-shown; [[BPR-0729]]'s rewritten scope carries all three as work
that still needs a drawing or an ask.

**BPR-0744.** The costed question that rode the candidate — whether every
identifier inside `#if`, `#ifdef`, `#ifndef`, `#elif`, `#undef` and
`defined()` should take the macro purple rather than only ALL_CAPS ones — came
back with an empty note for the second round running. It is **not** read into
the accept and **not** implemented. [[BPR-0748]] carries it with its costing,
and records that the next packet must ask it as its own candidate with its own
criterion and with two pictures.

### `application_composition_contract` was not run, and why

The predecessor expected a free profile. It is not free: the owner's own probe
process, pid 72017, has held it since 16:06:37Z. The block is mechanical and
now established rather than assumed — `assert_release_self_test`
(`tests/end_to_end/composition_contract_tests.py:500-531`) runs the freshly
built `BUILD_TESTING=OFF` Release executable with `os.environ.copy()` and
requires a stdout match, and with the lease busy that executable prints only
`PROFILE_ACTIVATION_QUEUED` or `PROFILE_ALREADY_OPEN` and exits
(`src/app/main.cpp:8005-8021`). There is no environment variable for the data
root, and `--test-application-data-root` is compiled out of that build, being
inside `#if defined(BPR_BUILD_TESTING)` (`src/app/main.cpp:7261`,
`:7285-7291`) — which the test itself asserts the absence of. It was **not**
worked around by pointing `HOME` at a scratch directory: that would be a green
under a different environment than the criterion names. Pid 72017 was **not**
killed: it is the owner's process on his own desktop.

### The two asks for the owner, exactly

1. **Quit the BetterPullReview running from the probe terminal** (pid 72017).
   That is one command away from closing [[BPR-0744]]'s AC3 and this
   milestone's exit criterion 2:
   `ctest --preset dev --output-on-failure -R '^application_composition_contract$'`,
   about two to five minutes.
2. **Before quitting it, save that terminal's scrollback** to
   `/Users/jaeger/Documents/GitHub/BetterPullReview/build/logs/bpr-0746-cursor-probe.log`,
   or re-run the corrected command above with the gesture [[BPR-0746]] asks
   for: pointer onto the seam, **held still with only slight motion for about
   three seconds**, then off, then quit. Resting is the half that fails and is
   the variable under test.

Neither is urgent and neither blocks [[BPR-0743]].

### Attribution: the dispatch's premise was wrong, and no history was rewritten

d22 was told that `Co-Authored-By: Claude Opus 5 (1M context)` on `5846630e`
and `cdc00c79` was an anomaly against a repo that otherwise uses
`Claude Opus 5`. The census says otherwise: **533** commits carry
`(1M context)` and **100** carry the plain form, alongside 343 `Claude Fable
5.1`, 255 `Claude Fable 5` and 960 with no trailer. `(1M context)` is the
dominant Opus 5 form and goes back to at least 2026-09-03. d22 followed its
explicit instruction and used the plain form for its own commits, rewrote
nothing, and records the correction here so the owner can rule on which he
wants rather than on a false premise.

### Next, in order

1. **[[BPR-0743]]**, the epoch-identity design pass — Ready, P1, the largest
   piece in the pack and the only route to [[BPR-0646]]. Nothing in d22
   touched it and it is not blocked by either ask above. Take it in a fresh
   context: it is a design pass over `core::review_epoch_id`'s two
   machine-local parts, not a bounded edit.
2. `application_composition_contract` the moment the profile frees, then
   finish [[BPR-0744]].
3. [[BPR-0741]] and [[BPR-0742]].
4. No v010 packet was prepared and none is forced. The next one, when a card
   forces it, must fix the two defects `AGENTS.md` now names: absolute paths
   in every command, and one question per criterion. [[BPR-0746]]'s AC4 and
   [[BPR-0739]]'s AC3 both come back on a bundle carrying whatever the log
   leads to; [[BPR-0748]] needs two pictures; [[BPR-0646]] waits on BPR-0743.

Neither RFC became closable this round. Nothing touched RFC-BPR-0018, and
RFC-BPR-0028 still holds BPR-0743, BPR-0729, BPR-0653, BPR-0736, BPR-0739 and
BPR-0746. Nothing is pushed. d22 added eight commits on `main`, on top of the two
`cdc00c79` and `5846630e` it inherited unpushed.

d21's frontier, still authoritative on everything it is not corrected on
above, follows.

## Frontier, 2026-09-12 (d21, mandate r12): the v009 session ran

Dispatch `attended-prep-2026-09-10` d21 (mandate r12), same milestone
`attended-preparation-pack-head`. The owner asked to launch the attended
review. The host ran, he decided all four candidates in ten minutes and handed
back at `2026-09-12T16:13:55Z`, sequence 362, nothing pending. **The verdicts
are recorded and not consumed:** no card is finished, no acceptance entry is
written and nothing is interpreted here. That is the next dispatch's work. No
card was begun and no product source was touched.

### The four verdicts, verbatim

Every one is `actor: human`, carries no attachment, and its decision fingerprint
equals the prepared candidate's, so each is bound to exactly what was shown.

| Candidate | Revision | Outcome | Note |
|---|---|---|---|
| `BPR-0734-read-one-pane` | `design-v7-2026-09-12` | **accept** | *(empty)* |
| `BPR-0739-seam-hover` | `app-release-c2381f32-2026-09-12.5` | **select**: "Drop it: draw the rule at one constant weight, as the asset does" | "We don't need it anymore now that the cursor is fixed, it wasn't drawn in the right location anyways." |
| `BPR-0744-directive-ranges` | `app-release-c2381f32-2026-09-12.5` | **accept** | *(empty)* |
| `BPR-0746-cursor-probe` | `app-release-c2381f32-2026-09-12.5` | **accept** | "The pointer stuff works correctly. Dunno about the test you had me run." |

Decision ids, in that order: `31462e78-51ea-4ef5-ba85-231a18098b68`,
`9298ad8d-c3a2-4b4d-b58e-ba18a3daa3f5`,
`4f1d1354-c3c1-43d9-af11-358347147453`,
`3e06fb9d-e8d8-4abd-901e-b5dab8cc4d1c`.

### Three things the next dispatch must resolve before finishing any of them

**1. BPR-0746's log does not exist, and the accept cannot stand on its stated
criterion.** That criterion is "the bundle is run from a terminal with the
logging line, the pointer rests on the seam for about three seconds and moves
off, and `build/logs/bpr-0746-cursor-probe.log` exists". He did run the probe —
pid 72017 with `QT_LOGGING_RULES=qt.qpa.input.mouse*=true` confirmed on the
process — but he ran it from `/Users/jaeger/Documents/GitHub`, one directory
above the repository, and the candidate's copied command uses a **relative**
tee path. `/Users/jaeger/Documents/GitHub/build/logs` does not exist, so `tee`
(pid 72018) never opened an output file: `lsof` showed it holding only fds 1
and 2 on `/dev/ttys001`. No `bpr-0746-cursor-probe.log` exists anywhere on
disk. His own note, "Dunno about the test you had me run", is consistent with
that failure. **The objective half of the criterion was never satisfied**, and
his accept was recorded against a candidate whose evidence is missing. The
packet's own wording is at fault: prepare that command with an **absolute** tee
path next time, not a relative one behind a "run this in the repository" label.

**2. He states twice that the cursor works, which contradicts why BPR-0746 and
BPR-0739 exist.** "the cursor is fixed" on BPR-0739 and "The pointer stuff
works correctly" on BPR-0746. Nothing about cursor delivery changed between the
v008 and v009 bundles, so on the recorded state this should not be true. One
untested explanation, offered as a lead and **not** as a finding: no one had
ever run this bundle with a free profile and a real pointer, because the v008
build held the profile until he quit it this morning. That is a hypothesis, not
a diagnosis, and it is exactly what the missing log would have settled.

**3. BPR-0744's third question went unanswered.** The accept carries an empty
note, so the costed question the candidate rode — whether every identifier
inside `#if`, `#ifdef`, `#ifndef`, `#elif`, `#undef` and `defined()` should take
the macro purple, rather than only ALL_CAPS — is still open. An accept of the
implemented behaviour is not an answer to it.

**The renewal was bookkeeping, confirmed rather than assumed.** d20's
`coordinator_handoff.py check` returned reconcile because `BPR-0646:*` moved
under an unchanged milestone definition fingerprint. Read against `6b76f744`,
the whole change to that card is an appended "Owner answer" section plus one
link-path fix (BPR-0743 `10-backlog` → `20-ready`). Frontmatter, `status:
"Parked"`, `parked_on: human-review` and every entry of the `acceptance` array
are byte-identical, and the section says in its own words that the criterion is
**not reworded**. Exit criterion 4 of this milestone already admits "**or** the
exact remaining ask is recorded on the card", which is exactly what landed. No
requirement moved. Rebound at obligation fingerprint
`sha256:b18b2581a41bfffba81a70ac6fc84881c0de0e69bbf015e429e695682d6709a8`,
definition fingerprint `sha256:201de49a…`.

**The packet was verified intact before the host was started**, not taken on
trust: executable sha256 `c2381f32…` recomputed and equal, `codesign --verify
--deep --strict` exit 0, offline page digest `sha256:51da0dd4…` recomputed and
equal, and all four candidates `ready` + `protected` at sequence 357 with
fingerprints equal to
[session-identity.json](../../verification/reviews/rfc-bpr-0028-attended-v009/session-identity.json)
and no decision recorded against any of them. Each candidate's launch and copy
targets were resolved on disk: the design asset and its `read-one-pane` anchor,
the bundle, and a writable `build/logs` with
`build/logs/bpr-0746-cursor-probe.log` **not** yet present, so its existence is
real evidence when the probe runs.

**Host:** ran as pid 70931 on loopback `127.0.0.1:60308`, and was **stopped
through the contract** once he handed back. All four candidates keep their
protection of the Qt, application, SQLite, design and RFC paths, exactly as the
v008 set did, until an explicit `resume-development` precedes the next source
edit — and three of the four verdicts imply source edits, so that release is
the next dispatch's first move, not an afterthought. Reopen with `serve` if
anything needs showing again; the attach information comes back with it.

One practical note for whoever runs the next session: `wait` had to be re-armed
by hand after every single verdict, because a backgrounded shell command on
this harness is capped well below the length of a review. Arm it as a
persistent watch instead, or expect one wake per click.

**Correction to d20's environment note, which is now wrong.** The v008 build,
pid 30378, **is gone** — the owner quit it himself before this dispatch (the
profile directory was last touched 06:47; his `3afdeae7` landed 07:12). No
BetterPullReview process is running and nothing holds the profile. So the
packet's first instruction, and the "quit the running BetterPullReview first"
wording inside BPR-0739's and BPR-0744's launch labels, are **already
satisfied** — stale, but harmless, and not worth invalidating a protected
candidate's fingerprint to reword under his hands.

`application_composition_contract`'s
`test_tests_off_release_graph_omits_public_path_fixture` was **deliberately not
rerun,** and still has not been. It should go green now that the profile is
free, but `assert_release_self_test` launches the executable against the real
environment, so taking that evidence during the session would have opened the
profile and blocked his own launch of the v009 bundle. He may still have the
bundle open from the probe; check for a running process first, then run it. It
is one command and it closes exit criterion 2.

Next, in order. **Release the protections** with `resume-development` before any
source edit. Then consume the four verdicts against their own cards — BPR-0734
and BPR-0744 accepted, BPR-0739 selecting the constant-weight seam, BPR-0746
accepted on a criterion whose log never existed, which is the one that needs a
decision rather than a transcription: either re-take the probe with an absolute
tee path, or rewrite the criterion around what he actually observed. Then run
the composition contract, then [[BPR-0743]], still the largest piece of work in
the pack and the only route to BPR-0646. Neither RFC became closable this
round: nothing here touched RFC-BPR-0018, and RFC-BPR-0028 still holds
BPR-0743, BPR-0729, BPR-0653, BPR-0736 and the four cards these verdicts land
on. d20's frontier, still authoritative on everything it is not corrected on
above, follows.

## Frontier, 2026-09-12 (d20, mandate r11)

Dispatch `attended-prep-2026-09-10` d20 (mandate r11), milestone
`attended-preparation-pack-head` of plan `ux-redesign-pack-0028`. The owner
answered all six outstanding asks in chat. Five close a question; the sixth
reopens the epoch model and is the one that matters.

**The epoch answer.** Asked to choose between importing a pull request's
earlier updates and rewording [[BPR-0646]]'s handle criterion, he answered
neither: "An Epoch is an update to the PR, epochs identity should be stable and
repeatable if I look at the PR on different machines. A purely observed epoch
is not useful and terribly confusing." So the criterion is right as written and
is **not reworded**, and the fix is larger than a missing producer:
`core::review_epoch_id` is `{Authority, Incarnation, Ordinal}`
(`epoch_model.hpp:444-450`) and both variable parts are machine-local — a
random per-catalogue incarnation ([[BPR-0044]]) and an ordinal handed out by the
SQLite scope row under compare-and-swap (`epoch_catalog.cpp:3030-3045`,
`:3147-3156`) in observation order. [[BPR-0743]] is rewritten and promoted to
**Ready** (P1) with the derivation question, the ordinal's coupling points (git
retained refs encode it as sixteen hex digits, the ledger and window
restoration store it, the MCP context surfaces it, history paging is pinned on
it by [[BPR-0056]] and [[BPR-0066]]; the amber last-review tick is *not* on that
list, it matches by head OID), the migration-by-rebuild finding, and the
signed-out problem that every GraphQL-routed operation is refused anonymously
(`github_anonymous_read_admission.hpp:99-108`) so a timeline read has to be
REST. **Correction to d19's diagnosis:** `list_pull_request_commits` does exist
end to end and is admitted anonymously, but a force push rewrites a commit list
rather than appending to it, so commits cannot reconstruct the updates that
matter. RFC-BPR-0003 records the direction without reopening its baseline; if
the design pass finds the change exceeds what an owner direction on a closed
RFC can carry, it comes back for the owner rather than reopening it from inside
a card. **BPR-0646 is not in the v009 packet** and is not asked again on any
bundle until the line carries the pull request's own updates.

**The other five.** Block order is unified-diff order ("don't care"); the
one-pane choice lives in the View menu "like line numbers", which was read
against the code to mean per window, seeded from a profile default, with no
modality of its own, **so Review takes one pane too** — the opposite of the
position BPR-0734 drew, offered with its flip. The alias italic does not matter
("no") and `CE8AD4` stands as the macro purple ("yes it's acceptable"), so
RFC-BPR-0006's floor holds and neither is repeated. The seam's hover highlight
is **not settled**: "I'm not sure what this is, but leaning towards keep it" is
a leaning and its first clause is the operative one, so nothing changed and
v009 shows him the element in a picture instead of naming it again.

Landed on main: the answers and the redraw (`6b76f744`); BPR-0746's record
(`c0d3c5da`); [[BPR-0745]] finished human-pending (`16eaefb4`) with
`docs/design/RFC-BPR-0028-workbench-direction-v7.html`, state 2b redrawn so the
gutter carries the link map at its limit — each change's struck and added rows
converge on a spine, an empty side draws one ribbon, the whole-file overview
and the strip toggle are gone and a View menu is drawn instead; and
[[BPR-0744]] finished human-pending (`aff9fc64`) with a directive's arguments
taking their own roles. [[BPR-0747]] carded from that slice (P3, Backlog): a
`//` comment after a null directive paints `syntax_error` salmon, upstream
grammar behaviour, identical count at HEAD.

**BPR-0744's one non-obvious invariant, so it is not undone:** the
`syntax_other` → `syntax_macro` substitution is gated on `is_macro_case`, and
that condition is load-bearing. `dsOthers` has one user in `isocpp.xml` and
none in `c.xml`, but the format stream also carries `dsOthers` from `gcc.xml`,
`doxygen.xml` and `modelines.xml` through includes, so a blanket substitution
paints `&amp;` inside a Doxygen comment and `on`/`off` inside a kate modeline the
macro purple. Probed, not reasoned. The card carries the evidence and the two
recorded limits (`__clang__` grey, `__cplusplus` near-grey), both pinned by the
contract so they cannot drift, and the costed context-aware rule that would
remove them, which is asked in v009 rather than taken.

**BPR-0746 made no product change, deliberately.** Its three hypotheses differ
only in what AppKit does on the next real pointer motion after Qt's enter, and
nothing separates them without a real pointer on the seam. Synthesising that
motion is forbidden here and is exactly what made
`build/logs/bpr-0739-cocoa-diag-1.log` worthless. The check is prepared rather
than deferred: one terminal line with `QT_LOGGING_RULES`, riding its own v009
candidate so one trip to the seam yields both the log and the hover verdict.
The category and the four literals were read off the shipped
`libqcocoa.dylib`.

Bundle and packet. The tests-disabled signed bundle is rebuilt at `aff9fc64`
(executable sha256 `c2381f32…`, `build/logs/app-release-aff9fc64.log`,
`codesign --verify --deep --strict` exit 0, `BUILD_TESTING=OFF`). **No store
moves schema this round**, so there is no second palette reset. The v009 packet
is prepared under
[rfc-bpr-0028-attended-v009](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md):
four candidates ready and protected at **sequence 357**, revision
`app-release-c2381f32-2026-09-12.5` (`BPR-0734-read-one-pane` carries
`design-v7-2026-09-12`) — `BPR-0744-directive-ranges` (carrying BPR-0720's and
BPR-0738's shared criterion as one ask rather than two),
`BPR-0734-read-one-pane`, `BPR-0739-seam-hover` and `BPR-0746-cursor-probe`. No
host started, no wait armed: `attended_review.py serve`, then `wait --after 357`.
The v008 candidates were released with `resume-development` at sequence 349
before any source edit; their verdicts stay bound to their own revisions.

**Environment, not a regression.** `application_composition_contract`'s subtest
`test_tests_off_release_graph_omits_public_path_fixture` fails
`PROFILE_ALREADY_OPEN`. Verified at HEAD by restoring HEAD content over all six
changed files: identical failure. `assert_release_self_test`
(`composition_contract_tests.py:508`) launches the executable with the real
environment so it opens the real profile, and `src/app/main.cpp:8019` prints
that for `owner_busy`; the owner's v008 bundle has been running since 21:18
(pid 30378) and holds it. It was not killed. Everything else in the
integration-focused pass is green: 25 of 26, including
`qt_composed_github_scale_100`, `qt_workbench_window_scale_{100,125,200}`,
`qt_settings_window*`, `qt_diff_workbench_scale_*`, `qt_appearance_*`,
`syntax_classification_contract`, `architecture_boundaries` and
`application_review_round_trip_composition_contract`. The documentation gate
and `git diff --check` are clean. The check goes green when he quits that
build, which is the packet's first instruction.

Closeout state: unchanged, both RFCs stay open. **RFC-BPR-0018 is not
closable**: 17 of 20 cards Done, this card still waits on [[BPR-0730]] and
[[BPR-0731]] (the Windows and Linux packaged inventories for AC-11, on their own
stations), and the checklist below still holds AC-01, AC-07, AC-08 and AC-11
open. Nothing in d20 touched RFC-BPR-0018. **RFC-BPR-0028 is not closable**: 41
of 51 Done, with BPR-0646, BPR-0734, BPR-0739, BPR-0745 and BPR-0746 parked on
human-review, BPR-0743 Ready and carrying the epoch work, BPR-0729 waiting on
BPR-0734, BPR-0653 on the Windows station and BPR-0736 in Inbox.

Not reached this dispatch, unchanged: [[BPR-0741]], [[BPR-0742]], [[BPR-0735]]
(clangd), [[BPR-0736]], [[BPR-0729]]. Next, in order: run the v009 session and
consume its four verdicts, including the cursor probe's log, which is what
unblocks BPR-0746's diagnosis; then [[BPR-0743]], the largest piece of work in
the pack and the only route to BPR-0646; then BPR-0741 and BPR-0742. d19's
frontier follows.

## Frontier, 2026-09-11 (d19, mandate r10)

Dispatch `attended-prep-2026-09-10` d19 (mandate r10). The owner ran the
v008 session and handed back at sequence 348 with all six candidates
decided; every decision matches its candidate's fingerprint in
[the v008 identity file](../../verification/reviews/rfc-bpr-0028-attended-v008/session-identity.json),
so no verdict is stale. His four screenshots are in that packet's `captures`
directory (the fifth attachment was the v007 Visual Studio reference
re-attached byte-identical). Landed on main: [[BPR-0737]] finished
(`24e1fef8`), the epoch row accepted; the five rejections recorded on their
own cards with the four follow-ups they become (`d0ae88fa`); RFC-BPR-0028
and RFC-BPR-0006 updated (`9a63931c`); IDs BPR-0743..BPR-0746 reserved
(`d960f945`). No product source changed in this dispatch, so the v008
bundle and both composition digests still stand at `35438244` /
`e4fdb0ea…`.

What the rejections mean, one line each. **BPR-0646**: the handle criterion
is unjudgeable on any pull request's first open, because a review epoch is
this install's own observation and nothing enumerates a pull request's
earlier updates — the local catalogue held one epoch for NVIDIA/stdexec#1846,
minted five minutes before the verdict. [[BPR-0743]] (Backlog) holds the
owner's decision between importing history under RFC-BPR-0003 and rewording
the criterion; **this is the ask that has to reach the owner before another
bundle is prepared for BPR-0646.** **BPR-0720 and BPR-0738**: one
requirement in two notes, a directive line paints grey end to end where the
reference greys only the directive; [[BPR-0744]] (Ready). **BPR-0734**: the
one-pane map is the link map at its limit, not a new element; the RFC's
non-goal is relaxed and [[BPR-0745]] (Ready) redraws the state. **BPR-0739**:
the seam is accepted, the cursor is not, and the lit highlight refutes the
d17 cause — the defect is cursor delivery on macOS after Qt's enter;
[[BPR-0746]] (Ready) carries the hypotheses and the discriminating check.
BPR-0739's AC1 evidence is corrected there and on its own card: the cocoa
probe log it cited has the pointer 1164 pixels from the handle and prints
`FAILED`.

Four owner answers are still outstanding and none was synthesised: BPR-0734's
block order and where the one-pane choice lives; BPR-0738's alias italic and
the `CE8AD4`-for-`BD63C5` macro purple ("most things are good now" names
neither); BPR-0739's hover highlight; and BPR-0743's import-or-reword
question.

Closeout state: unchanged by this round and both RFCs stay open.
**RFC-BPR-0018 is not closable**: 17 of its 20 cards are Done, this card
waits on [[BPR-0730]] and [[BPR-0731]] (the Windows and Linux packaged
inventories for AC-11, deferred to their stations, "after we sort all the
MacOS and platform agnostic stuff out"), and the checklist below still holds
AC-01, AC-07, AC-08 and AC-11 open for target-native, screen-reader and
packaged evidence. Nothing in the v008 round touched RFC-BPR-0018;
[[BPR-0683]] remains its only attended criterion and it was accepted back in
v006. **RFC-BPR-0028 is not closable**: 40 of 50 Done, with BPR-0646,
BPR-0734 and BPR-0739 parked on human-review, BPR-0729 waiting on BPR-0734,
BPR-0743 in Backlog behind the owner, BPR-0653 on the Windows station,
BPR-0736 in Inbox, and BPR-0745 and BPR-0746 Ready. BPR-0646 is the one that
changed character: its criterion can no longer be closed by preparing another
bundle.

The v008 candidates keep their protection of `src/adapters/qt`,
`src/application`, `src/adapters/sqlite` and the design and RFC paths until
an explicit `resume-development` precedes the next source edit; this
dispatch made no source edit and released nothing. The host (pid 29635) was
stopped through the contract after the decisions were consumed. Next, in
order: [[BPR-0744]], [[BPR-0745]] and [[BPR-0746]] on main (all three need
`resume-development` first, since all three touch protected paths or design
assets); then [[BPR-0741]], [[BPR-0742]]; then a rebuilt bundle and a v009
packet re-asking BPR-0720, BPR-0734, BPR-0738 and BPR-0739 with the four
open questions attached. BPR-0646 is not in that packet until BPR-0743 has
its answer. Outside the pack: [[BPR-0735]] (clangd), Ready. d18's frontier
follows.

## Frontier, 2026-09-11 (d18, mandate r9)

Dispatch `attended-prep-2026-09-10` d18 (mandate r9). Landed on main:
[[BPR-0740]] finished (`35438244`): finding 2 settled by instrumentation
as course composition, not a product regression (window B's landing
sign-in enrolled a second grant whose identity change retired every
window's session; A had read anonymously because `PublicPreviewSignedOut`
is false before the first paste activates the signed-out session); the
course signs in the first window of each phase before its first paste,
and its broken phase now pins BPR-0718's composed-defaults fallback for an
unreadable layout store, the lost user-visible report carded as
[[BPR-0742]] (Ready, P3). No product change; `github_application.cpp` and
both digests unchanged since the v007 bundle head `2cdc0e57`. The
tests-disabled signed bundle rebuilt at `35438244` (executable sha256
`e4fdb0ea…`, `build/logs/app-release-35438244.log`, codesign clean) and
the v008 packet prepared under
`docs/verification/reviews/rfc-bpr-0028-attended-v008/` (six candidates
at revision `app-release-e4fdb0ea-2026-09-11.4`, ready and protected at
sequence 341: BPR-0646 and BPR-0720 re-asked on NVIDIA/stdexec#1846,
BPR-0734, BPR-0737, BPR-0738, BPR-0739). No host started, no wait armed:
`attended_review.py serve`, then `wait --after 341`. Next after the
verdicts: [[BPR-0741]] (the multi-window course; its `:236` holds on the
account path once the course signs in first), BPR-0729 from the accepted
one-pane state, BPR-0742 when a slot allows. d17's frontier follows.

## Frontier, 2026-09-11 (d17, mandate r9)

Dispatch `attended-prep-2026-09-10` d17 (mandate r9). Landed on main:
[[BPR-0739]] diagnosed and fixed (causes 1, 2 and 4 excluded by a Qt
test that passed at HEAD and by a cocoa probe; the cause is the invisible
seven-pixel handle beside the comparison's scroll bar, so the central
splitter handles now draw the asset's one-pixel drawer edge rule in `Mid`,
`Highlight` under the pointer; parked human-review, asked in v008);
the harness-regenerated `native-roles.lock.json` recorded (claude role
hashes match the installed `.claude/agents` files); [[BPR-0741]] carded
(the multi-window course red at HEAD on macOS, same cause as BPR-0740).
[[BPR-0740]] is In Progress with its course converted to the account path
(the stale sign-in stop was BPR-0732's; confirmed) and one open finding:
after window B opens pull 18, window A's review is closed
(`invalidate_now` in the workbench host); course composition or a
product regression in the window-session lifecycle, undetermined, with
the deciding instrumentation named on the card. The course is committed
in that state and fails at "last accepted layout change belongs only to
A" with A's status in the failure output. Not done: the tests-disabled
bundle and the v008 packet, held because BPR-0740 may still change
`github_application.cpp`; a manifest generator and packet draft for v008
are in d17's return (six candidates: BPR-0646 and BPR-0720 re-asked, and
BPR-0734, BPR-0737, BPR-0738, BPR-0739, on NVIDIA/stdexec#1846; the run
notes say the appearance store moves to schema 5 and older builds refuse
it). No host started, no wait armed. Next: settle BPR-0740's finding 2
(instrument `github_window_read_host::retire`/`release` and
`WorkbenchInvalidations` at the moment B opens), or park it to a named
blocker so the bundle can be built; then the bundle, then the packet.
d16's frontier follows.

## Frontier, 2026-09-11 (d16, mandate r9)

Dispatch `attended-prep-2026-09-10` d16 (mandate r9). The owner answered
d15's two asks: NVIDIA/stdexec#1846 is the multi-update pull request for
the BPR-0646 and BPR-0737 asks (recorded on both cards), and BPR-0738's
reading is corrected, functions orange `FF8000`, class/struct types gold
`FFD700`, macros purple `BD63C5` with a distinct macro role "necessary"
(per-token resample on BPR-0738; BPR-0720 and RFC-BPR-0006 point at it).
Landed in d16, all on main: [[BPR-0734]]'s round (workbench states v6,
state 2b "Read, one pane", the drafted Modalities and AC-05 amendments and
three open questions on the card; parked human-review); the ten v007
candidates released through `resume-development` (sequence 323, no
candidate protected); [[BPR-0738]] (a `syntax_macro` role, the dark
values types `FFD700`, functions `FF8000`, variables `BDB76B`, attributes
`4EC9B0`, macros `CE8AD4`, the appearance store at schema 5 with no second
reset; parked human-review); [[BPR-0737]] (the epoch row is the review
surface's first row above the splitter, full client width in every
modality; parked human-review; RFC-BPR-0028's chrome paragraph amended).
Found red at HEAD and not any card's: the three round-trip composition
pins that BPR-0733 left stale (repinned under BPR-0738's commit) and
`application_profile_window_layout_public_path` ([[BPR-0740]], Ready).
Still to do, in order: [[BPR-0739]] (the resize cursor; diagnosis first);
the rebuilt tests-disabled bundle; the v008 packet written last, on
NVIDIA/stdexec#1846, re-asking BPR-0646 and BPR-0720 and asking BPR-0734,
BPR-0737, BPR-0738 and BPR-0739 (BPR-0738's ask also carries the include
path colour and the alias italic; BPR-0734's carries its three
questions). No host is started and no wait armed. d15's frontier follows.

Dispatch `attended-prep-2026-09-10` d15 (mandate r8). The owner reviewed
all ten v007 candidates on the 2cdc0e57 bundle (executable sha256
`64d9e46a…`, revision `app-release-64d9e46a-2026-09-11.3`) and handed back
at sequence 322; every decision was consumed against its own candidate and
fingerprint (all ten match the identity file of
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md);
no stale verdict).
Accepted and Done: [[BPR-0663]], [[BPR-0676]], [[BPR-0725]], [[BPR-0726]],
[[BPR-0727]] (with d11's judgement), [[BPR-0728]] (its note is a new
defect, [[BPR-0739]]), [[BPR-0732]] and [[BPR-0733]] (with d13's change);
[[BPR-0716]] finished on BPR-0732's verdict. Rejected and still parked:
[[BPR-0646]] (the epoch line's placement in the centre pane, [[BPR-0737]];
the handle criterion needs a pull request with several updates) and
[[BPR-0720]] (macro and function colours against the Visual Assist
reference, [[BPR-0738]]). The owner's five screenshots are in the v007
packet's `captures` directory. The host (pid 55182) was stopped through
the contract; the ten candidates keep their protection of `src/core`,
`src/application`, `src/adapters/qt`, `src/adapters/sqlite`,
`src/adapters/syntax` and `src/app` until an explicit
`resume-development` precedes the next source edit.

Closeout state: RFC-BPR-0018 is not closable; this card still depends on
[[BPR-0730]] and [[BPR-0731]] (Windows and Linux packaged inventories,
"after we sort all the MacOS and platform agnostic stuff out"). RFC-BPR-0028
is not closable: BPR-0646 keeps one criterion parked, BPR-0729 waits on
[[BPR-0734]], BPR-0653 waits for the Windows station, and BPR-0737 and
BPR-0739 are new Ready work. Next, in order: [[BPR-0734]] (design only, no
product source, needs no release); then `resume-development`, [[BPR-0737]]
and [[BPR-0739]] under RFC-BPR-0028 and [[BPR-0738]] under RFC-BPR-0006 on
main; a rebuilt tests-disabled bundle; a v008 packet re-asking BPR-0646
(on a public pull request with more than one update) and BPR-0720 plus the
three new asks. Outside the pack: [[BPR-0735]] (clangd noise), Ready, not
started. Detail below under "Continuation after the v007 hand-back".

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0018](../../rfcs/RFC-BPR-0018-robust-settings-center.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 1 of 4.
Planned cards: BPR-0592, BPR-0593. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

The 2026-09-04 registration was subsequently activated by the owner's
2026-09-07 implementation direction in the canonical index. This closeout stays
in Backlog until its required acceptance is complete. The immutable base remains
the pre-registration commit; registration commits are planning history.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: The shipped application exposes the profile-identified singleton Settings window with explicit per-workbench Ctrl/Command-comma binding, verified by the composed profile and four-scale key courses and the complete macOS native menu/shortcut/window-lifetime course; remaining Windows and Linux native courses are preserved as unaccepted standalone BPR-0974 and BPR-0975. | Four-scale exact-binding/two-workbench key tests; actual profile course; complete native Mac menu and lifetime course; explicit owner r2 transfer | Pass for the amended scope: explicit binding passes four scales, prior actual profile course passes (78.15 s), and current Mac tests-OFF native course passes all 11 observations and exits 0. Windows' corrected prefix before quarantine is not its full course; Linux full focus/lifetime still fails. Those unchanged obligations remain Blocked on BPR-0974/0975, not passing evidence. Exact failures and isolation remain in the [continuation report](../../verification/BPR-0593-native-settings-continuation.md). | 68e0a40f, a881d9f9, BPR-0705; prior 138dd1d9; product 2d3bdbdf; observer/test upkeep e7843405; owner r2 amendment in this slice |
| AC-02: All seven initial categories are present when their capabilities are composed, with stable sidebar/page/row identities and truthful unavailable states. | Seven-page composed minimum-size/four-appearance checks; native inventory cards | Pass for the seven fixed categories and truthful unavailable rows. Current four-scale Settings course passes; later General updater is owned by RFC-0035 and identified as a platform capability. | 51c24f6b, 6c0ba02b; 19a3dfd7 r1 rerun |
| AC-03: Appearance is migrated from the embedded workbench surface to the settings center with no duplicate authority; existing mode, typography, palette, import/preview/commit/cancel/reset, and persistence behavior remains intact. | Appearance host/store/panel evidence; full application_appearance_theme_customization_contract | Pass. Required public import/preview/cancel/commit/export/restart/FIFO course passes after its restart uses the existing composition-only probe instead of unrelated obsolete CLI setup. Every original palette, zero-write, persistence and comparison assertion remains; final contract 24.09 s. | 03a8a02e, a881d9f9, ed2b3f14; product 19a3dfd7; test upkeep 138dd1d9 |
| AC-04: Every setting gesture becomes one typed domain intent, reaches only its owning host, and updates the control only from an accepted authoritative projection. | Domain refusal/late-result checks, Settings effect boundary, workbench composition | Focused paths pass and existing medium-review disposition is retained. Broader `qt_composed_github_scale_100` still stops at the existing BPR-0940 CLI-card assertion; the rest of that suite is not accepted by this rerun. | dd64c098, 51c24f6b, a881d9f9; 19a3dfd7 r1 rerun |
| AC-05: Existing domain stores migrate and reopen from prior schemas; future, malformed, interrupted, and unavailable cases fail at their exact scope without corrupting another domain. | SQLite appearance/workbench migrations; saved-server restart; full appearance restart contract | Pass for executed migration/reopen/future/malformed isolation, saved-server persistence and exact stored-palette restart. BPR-0686 pre-write validation and RFC-0034 per-theme migration retained. The unrelated CLI entry no longer prevents the required appearance persistence check. | 03a8a02e, dd64c098, 22e30024, 867e4bed; product 19a3dfd7; test upkeep 138dd1d9 |
| AC-06: Credential, privacy, cache, notification, and agent actions retain their existing capability and secrecy boundaries. | Device/notification/agent/maintenance fixture evidence; architecture and effect-boundary checks | Existing capability/secrecy acceptance retained; both architecture gates and Settings effect instrumentation pass at the current source. No live account, credential, GitHub or GHES mutation was used. | 01a3ada4, dd64c098, 51c24f6b; 19a3dfd7 r1 rerun |
| AC-07: Keyboard-only and screen-reader courses reach every sidebar item, row control, reset, confirmation, status, and close action in a natural order. | Qt focus/scroll/close checks and digest-bound native VoiceOver decisions | Accepted: BPR-0683 v006 decision f2fb9b06 covers the original Settings walk. Later Accounts/Home VoiceOver 506a1c44 and Appearance VoiceOver 6825fbc6 plus native picker 1b80dbd8 cover their revised pages at ed2b3f14. Current Settings keyboard checks pass. These decisions do not certify later RFC-0035 update controls. | d1ba791f (cc2afa7f bundle), ed2b3f14 (d500ef7a bundle); 19a3dfd7 r1 rerun |
| AC-08: Layout and rendered evidence pass at 100%, 125%, 150%, and 200% scale in Light, Dark, High-contrast Light, and High-contrast Dark, including long text and minimum window size. | Four `qt_settings_window` scales, four appearances, seven pages, long metadata at 760x480; accepted native visuals | Current 112 rendered layout combinations pass. Original native Settings visual verdict f2fb9b06 is accepted; revised Accounts d0bb147d and Appearance c5349059 verdicts supplement their changed pages. Deterministic offscreen geometry is not a new native or three-platform visual verdict. | 6c0ba02b, d1ba791f, ed2b3f14; 19a3dfd7 r1 rerun |
| AC-09: One-setting updates preserve sidebar selection, page scroll position, focus, unaffected persistent indexes, and all unrelated projections without routine resets. | Stable page/scroll/focus checks; accepted host and incremental workbench courses | Pass for Settings reopen, one-setting preservation and current workbench composition. Profile preferences and per-window layout retain their separate owners under RFC-0024. | 68e0a40f, dd64c098, a881d9f9; 19a3dfd7 r1 rerun |
| AC-10: Hostile catalog, profile, rule, and row floods stop at declared limits with typed refusal; one window materializes at most one page per category; rapid previews coalesce; cancelled or superseded loads publish nothing late; and instrumentation observes zero database, vault, filesystem, network, process, or blocking-wait work on the Qt thread. | Bounded collection/coalescing tests and `qt_settings_effect_boundary`; architecture gates | Pass for declared collection limits, one page per category, coalesced preview and cancellation. Current recorded capability ports execute on worker threads and publish on the decision lane. This is the specified focused instrumentation; exhaustive native syscall/chooser/vault tracing was never supplied or claimed. | 143abbc9, e354b45e, 29bb2980, a881d9f9, 6c0ba02b; 19a3dfd7 r1 rerun |
| AC-11: The recorded tests-disabled macOS, Windows and Linux baseline inventories match except for documented platform capability rows, and the current macOS/Linux inventories refresh later updater, CLI and maintenance rows; the current Windows refresh remains unaccepted on standalone BPR-0974. | Tests-OFF native inventories, exact candidate binding, normalized comparison with negative controls; explicit owner r2 transfer | Pass for the amended scope: e93d9215 Mac/Windows and 9bbe3604 Linux seven-page baseline matches; 19a3dfd7 Mac dev/release equality retained. Current 2d3bdbdf Mac/Linux inventories match six pages and differ only by the explicit macOS-only CLI entry; updater and seven RFC-0014 maintenance rows are refreshed. Eleven comparison tests pass. Linux supplies the same Release/tests-OFF graph inventory scope accepted on BPR-0731, not full distribution qualification. Current Windows refresh is unaccepted on Blocked BPR-0974 due to Defender, not inferred from the old baseline. | d1ba791f; e93d9215/fc77eac3; 9bbe3604/987d7766; prior 138dd1d9; product 2d3bdbdf; evidence e7843405; scope clarification 56458c84; owner r2 amendment in this slice |
| AC-12: Sign in starts supported device authorization directly and shows its code and browser action without Details, another Get code gesture, or pane scrolling; cancellation, expiry, and supported manual-token alternatives remain reachable. | Existing device/Home/direct setup courses and accepted subsequent shared sign-in design | Supported direct code/browser/cancel/expiry/manual flow evidence retained. Later onboarding uses the dedicated shared dialog; the old inline-row description is superseded, not a reason to restore a second code gesture. Broader legacy CLI assertions remain BPR-0940. | 01a3ada4, 1210eecc; accepted Accounts revision ed2b3f14 |
| AC-13: User-configured servers and account selections survive restart through typed host persistence, while remembered grants reuse the existing vault path and session-only manual credentials are never presented as durable. | Saved-server GitHub.com/GHES seed/reopen public contract and device-vault binding evidence | Current four-process saved-server course passes with exact hostname/account metadata kept and offered after reopen. Existing device vault tests retain grant behavior; manual tokens remain session-only. No production credential was used. | 01a3ada4; 19a3dfd7 r1 rerun (92.67 s) |
| AC-14: Configuration leaves the primary review surface compact, preserves tuned diff/appearance behavior, and exposes direct reconnect plus an exact pending destination handoff without changing credential or provider authority. | Compact workbench/setup/appearance evidence; current workbench and theme composition contracts | Current workbench (20.90 s) and full appearance customization/restart compositions pass. BPR-0689 exact destination/cancel/stale-stage acceptance and subsequent dedicated sign-in flow are retained. This continuation corrects only Settings shortcut binding/context; it changes no credential/provider authority, stores or compact review layout. | 554564f8, 01a3ada4, a881d9f9; prior 138dd1d9; current product 2d3bdbdf and Qt 6.11 test-call upkeep |

## Completion rules

- Owner amendment r2 governs the two explicitly revised native rows above.
  Standalone BPR-0974/0975 retain the transferred original obligations and do
  not gate this amended closeout. No other acceptance or canonical dependency
  is waived; prior checkpoints remain historical evidence.
- Replace planned checks with exact executed commands/courses, results, and
  producing commits as implementation completes; retain not-run rows honestly.
- Inspect all cards linked to this RFC, not just the initial planned pair.
  Every required card must be done and every canonical criterion satisfied.
- Exercise the selected public path and check critical changed-surface findings.
  Use focused evidence; do not infer a full matrix requirement from this card.
- Record target-native or attended evidence in the project's existing workflow.
  Park pending attended work and continue independent work; no approval or live
  GitHub authority comes from registration.
- Run the documentation structure gate and `git diff --check`, reconcile RFC
  and index status, and close only when canonical order and all dependencies
  permit Auto-Verified. No status changes are authorized by this registration.

## Resume

The owner activated the broader UX work. BPR-0678 through BPR-0682, BPR-0684
through BPR-0688 are implemented. BPR-0689 retains exact setup destinations
and stages authentication without disturbing an existing review. BPR-0683 is
parked for required native/attended evidence, with its automated passes retained.
Continue RFC-0024 planning after the automated Settings integration boundary,
while preserving pending target-native/human criteria and canonical closeout order.
The current authorized workstream continuation is retained on [[BPR-0595]].

## Resumed implementation, 2026-09-07

The owner activated the broader UX frontier. [The authority inventory](../../development/settings-authority-inventory.md)
reconciles the integrated 0028 work and maps every criterion to BPR-0678 through
BPR-0683. These join BPR-0592 and BPR-0593 as the planned cards. Existing
partial implementation is not full criterion acceptance; not-run rows above
are superseded by the criterion ledger as each selected path is exercised.

BPR-0678: default-profile shell now uses stable per-page scroll containers,
retains panel identity and category scroll, identifies its default profile and
handles unconsumed Escape. Composed Qt checks pass at 100/125/200%; actual
760x480 offscreen Accounts/Reviews/Appearance captures inspected. This is
partial AC-01/02/07/08/09/10 evidence, not a full platform/accessibility verdict.
Two affected attended candidates (chrome and widths) were released by the
owner's resume-development authorization and need preparation after integration;
public-preview was subsequently released for BPR-0679 and also needs preparation.

BPR-0679: Home and the signed-out account action now start the existing device
flow directly. The executable panel course exercises Home, and the GHES
negative retains its no-request refusal. Running-flow duplicate, cancellation,
expiry and selected-host retry checks pass. Saved server authority/store and
session-only manual-token evidence from BPR-0660 remain unchanged. The
isolated tests-disabled build passed; one digest assertion was updated for
the reviewed course change and its bypass test passed separately. See the
card for commands and revision. Next: BPR-0680 appearance export/reset.

BPR-0684 repairs the Settings-close preview/import lifecycle discovered during integration preparation.

## Current integration evidence

At `3d36ce70` plus bookkeeping, both architecture checks and
`qt_composed_github_scale_100` passed (116.96s total). The actual application and
composed Qt target built successfully. Reused unchanged evidence: full core-only
125/125 at BPR-0681, selected domain host/store checks, device public course,
queue restart/pause course, theme customization 39.16s and maintenance 66.12s.
BPR-0685 adds all four scales/appearances with zero horizontal page scrolling;
112 deterministic offscreen rasters were generated and representative corrected
surfaces inspected. This does not stand in for screen-reader or target-native
packaged-app evidence.

One independent review was initially dispatched with the stale native role's
Astra high setting. It was interrupted on the owner's correction, and the same
bounded review continued with explicit Astra medium routing. The finding was
preserved: appearance legacy migration must validate before writing (BPR-0686).
The medium review reported no additional material finding in queue preference
merges, scoped resets, exports or close/import cancellation. No second review
loop is required; the repair receives its direct regression checks.

BPR-0686 fixed the migration finding (22e30024); malformed and future legacy
stores are refused before mutation. BPR-0687 bounds queue collections and keeps
accepted models and removal controls usable after refusal (e354b45e). Host/store
checks and Qt 100/125/200 passed, including maximum admitted collections.
BPR-0688 coalesces a burst to the latest preview and invalidates queued work on
close/reset (29bb2980); the panel and actual theme course passed. Documentation
verifier regression suite: 62 tests passed during this integration.

## Attended preparation, 2026-09-10

BPR-0683's three remaining macOS asks (AC-07 VoiceOver order, AC-08 the four
appearances at the real window, AC-11 the tests-disabled packaged inventory)
are one candidate in
[rfc-bpr-0028-attended-v002](../../verification/reviews/rfc-bpr-0028-attended-v002/operator-packet.md),
prepared under the adopted pack plan `ux-redesign-pack-0028`. Windows and
Linux packaged inventory stay pending on their stations. This closeout
remains in Backlog until BPR-0683 is Done.

## Attended preparation, 2026-09-10 (v003)

Prepared again, unchanged in substance, in
[rfc-bpr-0028-attended-v003](../../verification/reviews/rfc-bpr-0028-attended-v003/operator-packet.md)
on the bundle built at the BPR-0715 fix head (executable sha256
`9334b13a…`); the v002 packet is historical.

## Continuation after the v003 hand-back, 2026-09-10

Session `cbd48b91…` was handed back at sequence 141 with one decision, the
rejection of `BPR-0646-workbench-chrome` (decision `65f3f881`; `b1348a9e`
withdrawn and superseded). It is consumed on
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
and on RFC-BPR-0028, and became four cards: BPR-0719 (comparison work limit
refuses a file), BPR-0720 (dark palette comment and include colours, held
for the owner's confirmation), BPR-0721 (setting to hide the drawer controls
and the stray heading row above each rail) and BPR-0722 (identity hidden in
the strip below 1,400 pixels). `BPR-0683-settings-native-macos`, this
closeout's own candidate, and the eight other candidates carry no decision;
they stay ready and protected in the session, and their bundle inputs
(`src/adapters/qt`, `src/app`, the app-release bundle) stay guarded until
the owner says to resume development. Observations without verdicts are
recorded on BPR-0663 (no acquire stage in the restored run) and BPR-0715
(files and epoch line reached; log half pending).

Next, in order: ask the owner the three open questions (what was wrong in
Read mode; the extent of the BPR-0720 palette change; whether to resume
development now, releasing the nine undecided candidates for a rebuilt
bundle, or to review them on the current bundle first). Then, once released,
BPR-0719, BPR-0722 and BPR-0721 in that order as one milestone, a fresh
tests-disabled bundle, and a v004 packet regenerated last with the same ten
candidates plus the BPR-0719/0721/0722 asks; the packet is written only
after no active card can rebuild the binary.

## Continuation after the v003 follow-ups, first milestone, 2026-09-10

Dispatch `attended-prep-2026-09-10` d5 (mandate r2, "stop the attended
review session and fix the issues raised"). Session `cbd48b91…` was
stopped through the contract: `resume-development --all` released the
ten candidates (the nine undecided ones are `waiting`, unprotected, and
still pending on their cards; the BPR-0646 rejection `65f3f881` stays
recorded) and `stop` ended the loopback host (pid 24325). No watch is
armed.

Landed on main, in the recommended order:

- [BPR-0719](BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md)
  (`901a9c06`): the core sheds the common prefix and suffix, charges the
  exact matrix for the changed region only, and degrades through the
  ordered-unique alignment and patience anchors instead of refusing;
  `computed_file_diff::AlignmentApproximate` names a degraded alignment
  and the Qt status line says "(approximate alignment)". Parked for the
  owner's check on ScreenBufferTests.cpp in microsoft/terminal#20607.
- [BPR-0722](BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md)
  (`035b5322`): the strip identity yields title, owner, modality
  shortcuts and positions before its number, elided to the room the
  other items leave; the compact modality holds to 1100. Measured fact
  the card did not have: the floor is 900, not 860, and at 100% scale the
  other strip items alone lay out about 900 pixels, so below about 960
  the number has no room and the identity yields whole. Parked for the
  owner's check at the default size and at 1280x820.
- [BPR-0723](BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md)
  (`6de81ee8`): `application_uiforetw_selected_diff_contract` prints
  `STARTUP_FAILED` 0.5 s into its first capture at `8ff3b664`,
  reproduced on a clean worktree build of that head. Not attributed to
  the follow-ups; it blocks re-running the 1280 compact-row and rail
  assertions that BPR-0722 and BPR-0721 touch.

Not started: [BPR-0721](BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md).
Scouted facts for it: the heading row is a nested `QHBoxLayout`
(`workbench_drawer.cpp:399-419`) that `adopt()` (`:501-534`) never hides,
so `Heading` and `Collapse` must be toggled explicitly; the settings store
is already schema version 5 (`workbench_settings_database.hpp:31`), so the
flag is a version 6 column; a profile-level flag fits
`review_behaviour_settings` (`workbench_runtime_ports.hpp:478-488`) and
rides `combined_layout` unchanged, with the hand-built literal at
`src/app/main.cpp:6452` to extend; the Reviews & queue page is built by
`build_behaviour_editor` (`workbench_window.cpp:3614-3740`); the drawer
shortcuts are Alt+Shift+1/2/3, not ⌘1-4 as the card says; the UIforETW
contract asserts the 18-pixel rails, so a zero-width collapsed drawer by
default amends that contract as well as RFC-BPR-0028 AC-03 (`:86`,
`:90-91`, `:129-130`, `:168-169`, `:290-293`).

Next, in order: BPR-0721 as one milestone (defect first, then the setting
through store, projection, window and Settings page, then the RFC and
contract amendments); a fresh tests-disabled bundle; the v004 packet
regenerated last with the same ten candidates plus the BPR-0719/0721/0722
asks, written only after no active card can rebuild the binary. Owner
questions still open and carried forward verbatim: what specifically was
wrong in Read mode ("Even in read mode viewing experience is not great");
for BPR-0720, whether the whole dark syntax palette moves to the Visual
Assist scheme or only comments and includes, and whether the saved custom
palette is reset or left; and, new from BPR-0722, which strip control
should give way below about 960 pixels so the identity's number can stay
to the floor, or whether the floor moves to 960.

## Continuation after the v003 follow-ups, second milestone, 2026-09-10

Dispatch `attended-prep-2026-09-10` d6 (mandate r2). Landed on main:
[BPR-0721](BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md):
the heading row no longer stays above a rail, and
`review_behaviour_settings::DrawerControls` (hidden by default, the owner's
direction) removes the rails and Hide buttons so the strip toggles are the
drawers' only controls; schema version 6; RFC-BPR-0028 AC-03 amended. Parked
for the owner's acceptance of both states. Measured facts: a panel that
leaves and rejoins a `QSplitter` is granted the room it last had regardless
of its fixed width, so the window re-applies `central_sizes()` (which now
works before a layout is published); BPR-0675's attended criterion now
needs the setting on. `application_workbench_composition_contract` stays
red at head (BPR-0718) and `application_uiforetw_selected_diff_contract`
was not rerun (BPR-0723); neither is attributed to this milestone.

Next: a fresh tests-disabled bundle and the v004 packet with the same ten
candidates plus the BPR-0719/0721/0722 asks, written only after no active
card can rebuild the binary. The three owner questions stand as recorded
above.

## Attended preparation, 2026-09-10 (v004)

The bundle was rebuilt at 3c9a3f1c (`cmake --preset app-release`, tests
disabled, executable sha256 `9261509a…`, signature verified) after
BPR-0721 was parked, so no active card could rebuild it. The session
`cbd48b91…` carries the ten v003 candidates at revision
`app-release-9261509a-2026-09-10.3` plus `BPR-0719-large-file-diff`,
`BPR-0721-drawer-controls` and `BPR-0722-strip-identity`: thirteen ready
and protected at sequence 182, none decided, in
[rfc-bpr-0028-attended-v004](../../verification/reviews/rfc-bpr-0028-attended-v004/operator-packet.md).
No host is running and no watch is armed; the packet says how to start
both. BPR-0683's candidate is this closeout's, unchanged in substance.

Next: on the owner's say-so, `serve` and a persistent `wait --after 182`;
then consume the verdicts card by card. Until then the only independent
work is BPR-0723 (the UIforETW startup failure), BPR-0718 (the red
workbench composition contract) and BPR-0720 once the owner answers; the
bundle inputs (`src/adapters/qt`, `src/app`, `src/core`, `src/adapters/sqlite`,
`src/application`) are protected by the thirteen candidates until the
owner says to resume development.

## Continuation after the owner's answers, 2026-09-10 (v005)

Dispatch `attended-prep-2026-09-10` d7 (mandate r3: the owner answered the
three open questions). The thirteen v004 candidates were released through
`resume-development --paths src/adapters/qt src/adapters/sqlite
--resources install:…` (all thirteen protect those paths; sequence 183)
before any source edit. Landed on main:

- [BPR-0724](BPR-0724-read-mode-leaves-too-little-horizontal-room-for-the-code.md)
  (`25e1525d`): "it was difficult to read the code because of a lack of
  horizontal area for the text display". Read collapses the repositories
  drawer as well as the files drawer (300 pixels back to the comparison);
  what Read does with the conversations drawer (375-pixel minimum) or a
  single-pane layout is an options candidate, because the RFC's Read
  definition and AC-05 settle it the other way today. Parked.
- [BPR-0720](../40-parked/BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md)
  (`c5bd59f8`): "the whole palette, yes reset". The dark syntax roles
  follow Visual Studio dark / Visual Assist; three values lightened to the
  4.5:1 floor on the diff fills (the audit and ratchet pass); the
  appearance store moves to schema 4 and clears `palette_stored` once on
  the way through (the owner's store holds a light-bound customisation at
  schema 3, so that is what the reset removes). Parked.
- [BPR-0722](BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md):
  "leave it be for now" recorded as the decision to keep the 900-pixel
  floor and the whole-identity yield below about 960; its attended
  criterion is unchanged.
- [BPR-0723](BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md):
  `application_appearance_theme_customization_contract` shares the
  `STARTUP_FAILED` signature at cc31119b on a clean worktree build, before
  this milestone's changes; recorded there, not attributed here.

The bundle was rebuilt after both cards were parked (executable sha256
`d238842d…`, signed, tests disabled; a rebuild at 25e1525d is a no-op) and
the session `cbd48b91…` carries the thirteen v004 candidates at revision
`app-release-d238842d-2026-09-11.1` plus `BPR-0720-dark-palette` and the
options group `BPR-0724-read-width`: fifteen ready and protected at
sequence 228, none decided, in
[rfc-bpr-0028-attended-v005](../../verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md).
No host is running and no watch is armed; the packet says how to start
both.

Next: on the owner's say-so, `serve` and a persistent `wait --after 228`;
then consume the verdicts card by card (a BPR-0724 selection other than
"leave Read as it is" is a new bounded slice; the first option amends
AC-05). Until then the only independent work is BPR-0723 (now also the
theme customization contract) and BPR-0718; the bundle inputs
(`src/adapters/qt`, `src/adapters/sqlite`, `src/app`, `src/core`,
`src/application`) are protected by the fifteen candidates until the
owner says to resume development.

## Continuation after the two red contracts, 2026-09-11 (v006)

Dispatch `attended-prep-2026-09-10` d8 (mandate r4: "Fix the 2 failing
open work before we do the next round of reviews"). The fifteen v005
candidates were released through `resume-development --paths src/app`
(sequence 229) before the composition edit. Landed on main:

- [BPR-0718](BPR-0718-the-workbench-composition-contract-is-red-at-the-pack-head.md)
  (`7c636594`): not a bisect; the probe failed its session-fallback check
  because BPR-0708's desktop path marked workbench persistence available
  without opening the store. Composition now opens it once and falls back
  to session defaults; per-window durable layouts are installed only when
  it opened. `application_workbench_composition_contract` passes.
- [BPR-0723](BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md)
  (`d1ba791f`): the one word was the registry refusing a pre-created
  0755 root, since BPR-0695 made `--epoch-fixture-root` the registry root;
  eight contract scripts now narrow their roots (the theme, overview,
  saved-server, review-behaviour, epoch-maintenance and native-notification
  scripts had the same omission). Behind it, BPR-0390's displayed-comparison
  flag read empty after shutdown since BPR-0697/BPR-0705 release the read
  host and its comparison; the automated-run status is captured at the
  shutdown transition. The BPR-0646/0651 rail block was amended for
  BPR-0721's no-rails default. Both digests of
  `application_review_round_trip_composition_contract` repinned.

Re-run for the earlier dispatches: the UIforETW compact row at 1280 passes
(BPR-0722), the rail assertions could not be re-run as written because the
default has no rails and are amended (recorded on BPR-0721), and the theme
customization contract passes for BPR-0720. Also seen:
`application_native_notification_attended_contract` gets past startup now
and refuses at `platform_tray_not_composed` under the cocoa platform; not
attributed, not chased, no card. Gates at d1ba791f: composition contract
(140 s), both boundary scripts, strict tidy on the two touched TUs,
clang-format, `git diff --check`, documentation gate.

The bundle was rebuilt at d1ba791f (executable sha256 `cc2afa7f…`, signed,
tests disabled) and the session `cbd48b91…` carries the same fifteen
candidates at revision `app-release-cc2afa7f-2026-09-11.2`: fifteen ready
and protected at sequence 259, none decided, in
[rfc-bpr-0028-attended-v006](../../verification/reviews/rfc-bpr-0028-attended-v006/operator-packet.md).
No host is running and no watch is armed; the packet says how to start
both (`serve`, then `wait --after 259`).

Next: on the owner's say-so, `serve` and a persistent `wait --after 259`;
then consume the verdicts card by card. No independent unattended work
remains in this scope; the bundle inputs (`src/adapters/qt`,
`src/adapters/sqlite`, `src/app`, `src/core`, `src/application`) are
protected by the fifteen candidates until the owner says to resume
development.

## Continuation after the v006 hand-back, 2026-09-11

Dispatch `attended-prep-2026-09-10` d9 (mandate r5). The owner reviewed
all fifteen v006 candidates and handed back at sequence 278; events 262
and 263 were a pause and a review start with no decision. Each decision
was consumed against its own candidate and fingerprint (all fifteen
fingerprints match the prepared candidates; no stale verdict):

- Accepted and finished Done: [BPR-0662](BPR-0662-drawer-widths-survive-home-and-a-squeezed-splitter.md),
  [BPR-0667](BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md)
  (the log half rests on the 2026-09-07 20:56 live acquire; the v006
  launches were cache hits, session observation at sequence 280),
  [BPR-0674](BPR-0674-overview-is-starved-by-the-drawers.md)
  (headings accepted; policy chosen),
  [BPR-0675](BPR-0675-a-drawer-cannot-be-collapsed-by-anyone-who-has-not-read-the-code.md)
  (the owner retracted the rail finding from BPR-0646),
  [BPR-0677](BPR-0677-the-comparison-stays-unavailable-after-the-history-is-ready.md),
  [BPR-0683](BPR-0683-settings-integration-and-native-acceptance.md)
  (AC-07, AC-08 and the macOS half of AC-11; AC-10 recorded on the
  focused effect-boundary fixture, the ledger's "exhaustive" wording
  unchanged),
  [BPR-0719](BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md),
  [BPR-0721](BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md),
  [BPR-0722](BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md),
  [BPR-0724](BPR-0724-read-mode-leaves-too-little-horizontal-room-for-the-code.md)
  (two options chosen).
- Rejected, parked, follow-up carded: [BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
  (strip accepted; epoch line not judgeable; rails retracted; sign-in
  "technically yes"), [BPR-0663](BPR-0663-public-preview-refuses-the-epoch-smart-acquire.md)
  and [BPR-0676](BPR-0676-the-epoch-line-does-not-show-whether-it-is-working.md)
  (all three the empty epoch line on a cache hit, [BPR-0725](BPR-0725-the-epoch-line-shows-no-updates-when-the-catalogue-already-holds-the-head.md));
  [BPR-0720](../40-parked/BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md)
  (roles never emitted for C++, [BPR-0726](BPR-0726-functions-types-and-macros-are-uncoloured-in-c-and-c-plus-plus.md)).
- Selections: [BPR-0727](BPR-0727-overview-refuses-a-drawer-opened-by-hand.md),
  [BPR-0728](BPR-0728-the-conversations-drawer-goes-narrower-than-375-pixels-in-read.md),
  [BPR-0729](BPR-0729-a-single-pane-read-layout-in-the-manner-of-p4merge.md).
- Platform split of AC-11: [BPR-0730](BPR-0730-windows-packaged-settings-inventory.md),
  [BPR-0731](BPR-0731-linux-packaged-settings-inventory.md); this closeout
  depends on both.

Root causes were scouted, not fixed: on a cache hit the core prepares
the revision with no mutation and starts the comparison without emitting
an accepted epoch or a history page, and nothing else requests the
newest page (`src/core/src/epoch_transition.cpp:1302-1309`); the C++
grammar KSyntaxHighlighting ships emits no function role and leaves user
types and macros normal, which the classifier drops. The historical v004
and v005 packets had three links repaired to the moved cards (paths
only). No product source changed in this dispatch.

Next, in order, on main: BPR-0725 (with its application-host and Qt
pins and the composed restore course), BPR-0726, then BPR-0727 and
BPR-0728 as one drawer milestone; a rebuilt tests-disabled bundle; a
v007 packet with the four parked candidates (BPR-0646 narrowed to "a
handle on the epoch line changes the comparison"), written last. BPR-0729
waits for an RFC-BPR-0028 design round. Open to the owner: what on Home
is "very messy" (BPR-0716).

## Continuation after the owner's answers, 2026-09-11 (d10)

The previous frontier paragraph (d9, after the v006 hand-back) is
retained in substance under "Continuation after the v006 hand-back".
The owner's answers, verbatim on the cards:

1. What on Home is "very messy" (on BPR-0716): the queue and its filters
   have no use signed out; open by link does not work without an account;
   "Read without signing in" leads to another confusing page; open the
   link anonymously from the first paste field; the slow automatic
   restore of the last pull request is disorienting, a recently viewed
   list would be better. Two screenshots of the signed-out dark Home were
   attached; their visible text, transcribed: (A) the "Connect a GitHub
   account" card ("BetterPullReview reviews pull requests from your GitHub
   accounts. Sign in once with a device code; the session is remembered
   on this Mac. To open a public pull request without signing in, paste
   its link in the bar below."), buttons "Sign in to GitHub…", "Read
   without signing in", "Accounts & hosts settings"; "Open a pull request"
   with the field "Paste a pull request link, or write owner/name#123" and
   "Open"; "Review queue" with "Review queue is empty. Local rules never
   change GitHub assignments." over an empty list; combos "Role: any",
   "Observer: any reason", "Bucket: any", "Repository: any", "State: any",
   "Sort: core priority"; status "No pull request is open in the
   workbench." (B) after "Read without signing in": the card reads "Open a
   pull request / Paste a pull request link in the bar below, or pick one
   from the review queue." with "Choose from the queue" and "Paste a link";
   a centred "Open a pull request / Enter an owner, repository, and pull
   request number above." with "Copy message"; the same field, focused;
   the same empty queue and combos ("Role: any" disabled); the same status.
   Carded as [BPR-0732](BPR-0732-a-signed-out-home-is-one-paste-field-that-opens-a-public-pull-request.md)
   and [BPR-0733](BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md).
   The "open by link doesn't work" claim was verified in code before
   carding: the Home bar resolves through `core::resolve_setup_destination`,
   which is `ready` only for an authenticated profile, and a public-preview
   session has no credential authority, so the bar stops at "Sign in and
   continue" before and after "Read without signing in"; the Settings
   session panel is the only working signed-out open. BPR-0716's earlier
   "the bar does open a pasted link" is corrected there.
2. BPR-0729's design round: "Card it, can happen after the verdict" —
   [BPR-0734](BPR-0734-a-design-round-for-the-single-pane-read-layout.md),
   after the v007 verdicts; BPR-0729 depends on it.
3. Windows/Linux station time for BPR-0730/0731: "After we sort all the
   MacOS and platform agnostic stuff out" — recorded on both cards; they
   follow every macOS and platform-agnostic card above.

Home cards are placed after the drawer milestone and before the bundle
because BPR-0732 changes the Home the four parked candidates are reached
through and BPR-0733 changes what the bundle does at launch; both belong
in the v007 bundle so the owner sees one Home. IDs BPR-0732 to BPR-0734
were reserved on main (`b1d4e14b`).

BPR-0725 landed in this dispatch (see its verification record); the
maintenance public path was the one course it disturbed, repaired in the
probe. Mandate r7 added one card, [BPR-0735](BPR-0735-clangd-cannot-find-the-compile-database-and-reports-every-header-missing.md):
clangd reads no compile database because the project's lands in
`build/dev` and there is no `.clangd`; filed, not started. Next: BPR-0726.

## Landing record, 2026-09-11 (d13, BPR-0733 landed)

Dispatch `attended-prep-2026-09-10` d13 (mandate r7). Landed on main at
`91383cc4`, parked on human-review for the v007 verdict: [[BPR-0733]]. The
app no longer opens the last pull request at startup: `window_review_restoration::load`
selects the startup authority only, and a window's `open_recent` gesture
runs the same exact-position course the automatic restore used to. The
core keeps a per-profile `Recent` list (ten, most recent first, title once a
snapshot was seen, the saved position tracked from `accepted_position`);
the host persists it beside the per-window records (`recent_reviews`,
sqlite `user_version` 3, migrated from 2 with the old rows seeded newest
window first); the composition publishes a per-window
`recent_reviews_projection` filtered for the window's session (signed out:
public entries only; an account: its host's entries; Resume only when the
session matches) and routes "open" through the Home bar's setup path and
"resume" through `window_review_restoration_host::open_recent`; Qt shows
"Recently viewed" between the setup row and the queue heading, hidden when
empty, with Return/activation opening and a "Resume reading" action.
Invariants to know: a refused startup *authority* (missing grant, unavailable
capability) is now silent because nothing was pending; a refused
*destination* or store still speaks. Verified at `91383cc4`: the three
restoration selectors, `qt_composed_github_scale_*`,
`application_profile_window_review_restoration_public_path` (about 9.5
minutes now; timeout 1800 s), full dev build, `check_boundaries`, strict tidy
and clang-format on touched files, DOCUMENTATION_OK. Follow-up carded:
[[BPR-0736]] (queue rows ignore Return on macOS; pre-existing, Inbox).
The d11/d12 landings ([[BPR-0726]], [[BPR-0727]], [[BPR-0728]],
[[BPR-0732]]) are unchanged and still parked; [[BPR-0716]] stays in Backlog
on BPR-0732. Next, in order: a rebuilt tests-disabled bundle, then the v007
packet written last, covering BPR-0646 narrowed to the epoch line, BPR-0663,
BPR-0676 and BPR-0720, plus the asks for BPR-0725, 0726, 0727, 0728, 0732 and
0733 (0733's ask: restart the signed bundle with a review saved, see Home at
once with the entry, open it by hand), the ask for BPR-0732 (paste a public
link while signed out and it opens in one gesture), and a visible ask for
d11's stale-pin judgement that `infer_modality_from_drawers` no longer
infers Overview from the files-only shape. After the v007 verdicts:
[[BPR-0734]]; on their stations later: [[BPR-0730]] and [[BPR-0731]], on
which this closeout depends. Outside the pack: [[BPR-0735]] (clangd noise),
Ready, not started. No host is running and no watch is armed. Detail below
under "Continuation after the owner's answers".

## Continuation after the v007 hand-back, 2026-09-11 (d15)

The d14 frontier (v007 prepared at sequence 311) is retained in substance
by the packet. Decisions, events 312 to 321, then the hand-back at 322:

- Accepted and finished Done from their own acceptance: [[BPR-0663]]
  (the cache-hit launch showed the updates; the `smart_acquire` half rests
  on the 2026-09-07 live acquire recorded on BPR-0667), [[BPR-0676]] (the
  packet's side question about naming the default pair drew no answer;
  nothing carded), [[BPR-0725]], [[BPR-0726]], [[BPR-0727]], [[BPR-0728]],
  [[BPR-0732]], [[BPR-0733]]; [[BPR-0716]] promoted, claimed and finished
  on BPR-0732's verdict, since BPR-0732 took its scope.
- Rejected, parked, follow-up carded: [[BPR-0646]] became [[BPR-0737]]
  (the epoch control is the centre pane's first widget, so its labels
  move with the drawer widths and modality; the owner wants it and the
  "Comparing …" menu on one full-width row under the strip); [[BPR-0720]]
  became [[BPR-0738]] (macros are classified as the preprocessor role and
  paint its grey; functions paint Visual Studio's `DCDCAA`; the owner's
  reference is Visual Assist's purple and gold, sampled on the card).
- Accepted with a note: [[BPR-0728]]'s cursor finding became [[BPR-0739]]
  (no cursor or handle code exists on the central splitter; four
  candidate causes are listed, none confirmed).

No product source changed in this dispatch. The v007 packet's BPR-0733
link was repaired to Done (path only). RFC-BPR-0028 and RFC-BPR-0006 carry
the verdict paragraphs.

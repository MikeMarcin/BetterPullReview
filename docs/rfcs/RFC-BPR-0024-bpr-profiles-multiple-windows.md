---
id: "RFC-BPR-0024"
title: "BPR Profiles and Multiple Windows"
status: "Auto-Verified"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0002","RFC-BPR-0003","RFC-BPR-0007","RFC-BPR-0008","RFC-BPR-0018"]
pack_version: "post-v2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# RFC-BPR-0024: BPR Profiles and Multiple Windows

## Status and scope

**Design accepted; implementation complete; closed for the owner-amended r3
scope on 2026-09-22.** The [owner amendment r3](#owner-amendment-r3--2026-09-22)
below records the one deferred native observation. The
[owner amendment r4](#owner-amendment-r4--2026-09-23) of 2026-09-23 keeps
AC-07 closed over a recorded evidence gap; the course passes again and needs
no follow-up. On 2026-09-23 standalone [[BPR-0980]] took the native Windows
desktop observation r3 deferred. Before the correction in `f73aa135` a
repeated launch left the owner's most recently used window behind the
launching shell; at `2c9dc0c7` that window comes to the front with keyboard
focus, and `--new-window` opens another window in the one running process.
That is an observation on the standalone card; neither amendment's text or
this RFC's accepted scope changes.
The product owner approved this daily-workbench design and its Chrome-like
profile amendment on 2026-09-04. Registration grants no implementation,
credential, remote-mutation, platform-evidence, or closeout exception. This RFC
remains outside the fourteen-RFC pack and cannot close before its dependencies.
The requested workstream is RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025,
RFC-BPR-0026, in that order. The canonical index remains the order authority.

Registration baseline: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Entry: [[BPR-0594]] is Done. Closeout: [[BPR-0595]] is Done under owner
amendment r3 with every criterion row carrying its direct check, result and
producing commit.
The owner's ordered-pack implementation mandate and planning amendment are
recorded in the [2026-09-21 reconciliation](plans/ux-redesign-pack-0028/rfc-0024-planning-reconciliation.md).
The prior registration-only restriction is superseded by that mandate, not by
an inferred waiver. The immutable baseline, all nine criteria, remote-mutation
limits and native/human acceptance requirements are unchanged. Required
slices [[BPR-0976]], [[BPR-0977]], [[BPR-0978]] and [[BPR-0979]] are Done;
[[BPR-0715]] is Done on its exact native signed-out acceptance.

## Owner amendment r3 — 2026-09-22

After about eight hours with no verdict on the served Windows candidate, the
owner explicitly directed that observation deferred and this RFC closed
against the amended accepted scope. The
[bounded mandate](plans/ux-redesign-pack-0028/owner-closeout-r3.md) and
[[BPR-0595]] record the distinction between this acceptance change and
actual observations. It is not a native Windows verdict, a Windows release
approval or a waiver of any other criterion.

[[BPR-0980]] remains Blocked for the native Windows desktop observation of
AC-06's repeated ordinary launch activating the owner's MRU window and
explicit `--new-window` opening another window without a second owner. It
preserves the exact unjudged attended candidate
`BPR-0979-windows-activation-v001`, the station build at `1cd7f2f1` and the
passing automated Windows rows; it is standalone (`rfcs: []`) and not a
dependency of this closeout. RFC-0018's [[BPR-0974]]/[[BPR-0975]] are
unchanged.

Only AC-06's native Windows desktop terminal scope changes. All other
acceptance keeps its scope and evidence: the owner accepts on
`BPR-0715-signed-out-open-v004` (decision `b1eb8efb`) and
`BPR-0977-native-geometry-v001` (decision `ec0c5993`) bind to bundle
`6c830b41…` from `d5a2426c`; the two-account, multi-window, startup,
activation and window-layout courses, the Windows station rows at
`1cd7f2f1` and the retained BPR-0691–0714 evidence keep their producing
commits on BPR-0595.

**Status: Auto-Verified for the r3 accepted scope.** The closeout ledger on
BPR-0595 records all nine criterion/check/result/commit rows. At the closeout
head the macOS activation, owner-admission and window-layout courses pass
3/3, the documentation gate, board validation and `git diff --check` pass.
This status does not close BPR-0980 or imply native Windows desktop
acceptance or RFC-0038 release qualification.

## Owner amendment r4 — 2026-09-23

On 2026-09-23 the owner directed, verbatim: "for RFC-0024, criterion AC-07
just make an amendment and file a follow-up to fix that test". The owner gave
this in the coordinating chat, and the coordinator relayed it verbatim to the
implementer that recorded it. It changes acceptance. It is not an observation
and passes nothing at the closeout revision.

AC-07 closed on [[BPR-0595]] at `eb73df8b` (2026-09-22) on retained
[[BPR-0709]]/[[BPR-0710]] evidence. Their public course,
`application_profile_window_review_restoration_public_path`, was not run for
that closeout, and it was red at `eb73df8b`: no `src/` or `tests/` change
separates that revision from `e2f6eb1d`, where [[BPR-0597]] found it failing.
[[BPR-0985]] traced the failing reading phases to `cc038748` (2026-09-19), a
product regression. Across a restart, a window lost its retained semantic
reading position, and an unavailable semantic reading was not reported
explicitly. Both behaviors are within AC-07.

**Accepted gap.** AC-07 stays closed even though no run of its restoration
course passed at the closeout revision, and the product at that revision had
the restoration regression above. The AC-07 row on BPR-0595 stays as written.
It is not a pass of those two behaviors at `eb73df8b`.

Nothing else changes. RFC-BPR-0024 remains Auto-Verified. Amendment r3 and the
other eight criteria keep their scope and evidence. So does the rest of
AC-07's evidence: [[BPR-0707]]'s restart course, [[BPR-0977]]'s layout course
and native macOS geometry accept, and [[BPR-0715]]'s signed-out accept.

[[BPR-0985]] repaired the regression at `c9c48800`, and the course has passed
since: 570.40 s at `c9c48800`, 591.97 s at `17e40dc8`, 563.89 s at `0d56cee5`
and 598.25 s at `0ddf929b` (2026-09-23). Those runs are observations,
recorded on BPR-0595. They are not written into its historical acceptance
record. The 2026-09-23 run is bound on BPR-0595 as AC-07's current evidence,
beside the historical row. No follow-up card is open: the course already
passes, so there was nothing left to fix.

## Problem and outcome

A daily reviewer needs separate Work and Personal environments and may use
several windows within either environment. The current composition uses one
application-data root, and the layout store holds one singleton layout. Starting
another window must not accidentally share another account's private data or
overwrite the first window's reading position.

A BPR profile is a complete local work environment, distinct from an existing
GitHub host profile, an appearance profile, or an agent profile. The first
profile is **Default**. Users can create and name profiles such as Work and
Personal; a new profile begins empty with standard defaults.

## Accepted interaction and ownership

| State | Owner and sharing |
|---|---|
| Accounts and configured servers | BPR profile; one account per server per profile |
| Saved repositories, queue membership, recents, followed reviews | BPR profile; shared by its windows |
| Credentials, caches, retained epochs, drafts, notes, receipts, and other private review data | BPR profile plus existing account/repository/PR authority; never borrowed by another profile |
| Appearance and other durable preferences | BPR profile; accepted changes reach its windows |
| Open PR collection, active PR, per-PR tabs, comparison selections, reading positions | Individual window |
| Drawer widths/visibility, active filters, window geometry | Individual window; profile preferences supply defaults |

Different BPR profiles may use different GitHub.com accounts at the same time.
Two windows in one profile may show the same PR at different epochs or positions.
Shared review updates appear in both without moving either window's view.

Window chrome and the profile menu identify the current profile. The menu
offers **New Window**, **Open Profile**, and **Manage Profiles**. Management
supports creating and renaming profiles; renaming changes a label, not identity.
Opening another profile leaves existing windows intact. New Window stays in
the invoking profile. Repeated ordinary launches activate the profile's most
recent window; an explicit New Window request always creates another window.

There is one runtime owner per profile, with a host-owned admission/activation
boundary for repeated launches. Independent processes must not become competing
owners of the same profile. Distinct profiles may run concurrently. The launch
contract accepts stable profile identity and an explicit activate/new-window
intent; it is not a general remote command or credential transport.

Opening a profile restores its previously retained windows. After RFC-BPR-0026
is composed, the last-focused window opens Home with **Resume review** pointing
to its retained active PR/tab; other restored windows retain their previous
views. Newly created windows open Home. This RFC owns restoration records and
the landing destination; RFC-BPR-0026 owns Home's content and Resume gesture.
Before that composition, restore the existing landing/review surface without
claiming the later Home experience has shipped.

Explicitly closing a window removes it from the next automatic window restore;
quitting the profile/application preserves its open window set. Closing the last
window retains durable profile data. Closing one window does not cancel another
window's work or terminate shared profile services it still uses.

## Settings and shared edits

RFC-BPR-0018 first implements Settings in the existing default environment;
this RFC extends its binding to one Settings window per BPR profile. Invoking
Settings from any window raises that profile's existing Settings window.
The profile being edited is visible, and accepted preferences propagate only
within it. There is no backward dependency from RFC-BPR-0018 to this RFC.

Shared draft and note edits carry expected record revisions. A stale edit
preserves the user's candidate text and reports a conflict; it never silently
overwrites another window's accepted edit. Publication remains a separate
RFC-BPR-0008 action under the exact acting identity, target, and idempotency
contract. Profile/window creation does not create publication authority.
Signing out affects the selected server in all windows of this profile and
invalidates its pending authenticated work, while other profiles are unaffected.

## Architecture and interface changes

The core decides. The host performs. Qt presents.

- Add distinct typed BPR profile and window identities, lifecycle intents,
  restoration destinations, and revision-aware shared-edit outcomes. Do not
  reuse `host_profile_id`, file IDs, labels, or filesystem paths as identities.
- Application hosts own profile catalog operations, runtime admission, shared
  services, per-window sessions, persistence, credential lifecycle, and shutdown.
  Each completion is bound to its profile, account generation, and applicable
  window/request identity before publication. Qt is never the inter-window bus.
- Keep existing domain stores and narrow capability ports. Scope their roots
  and vault keys through profile identity; do not introduce a global settings
  dictionary or a second review ledger. RFC-BPR-0025 defines the tab payload.
- Qt owns menus, window presentation, focus, and incremental projections. The
  composition root explicitly supplies each profile/window's capabilities and
  lifetime; widgets cannot acquire stores, vaults, or process control.

## Persistence, migration, and failure handling

An immutable Default profile identity adopts the existing environment. Preserve
tuned appearance, settings, valid remembered sign-in, private records, cached
objects, and epoch identities. Existing device credentials remain in the vault;
tokens never enter profile or window records. Only Default may adopt legacy
storage/credential bindings, and restart validation remains mandatory.

Profile registry and window-session records are versioned and bounded. Migrate
with durable progress and restart recovery so interruption cannot create two
Defaults, lose the legacy environment, or enroll its credentials into a new
profile. Account replacement retains existing authority separation. Separate
profiles do not deduplicate private caches or share credential slots.

Failed or future-schema storage does not fall back to another profile. Report
the affected environment and keep unrelated profiles usable. Optional window
restoration failure may open a fresh window with an explicit restoration error;
it must not claim discarded state was restored. Unavailable retained epochs or
removed files remain unavailable and never silently become another comparison.
Runtime-owner contention reports or routes to the existing owner; it does not
bypass the ownership boundary after a timeout.

## Non-goals

Cloud profile sync, profile export/deletion, simultaneous accounts on one server
inside one profile, cross-profile data sharing, cross-profile tab moves, and
new publishing permissions are outside this RFC. This RFC does not build Home,
URL parsing, file tabs, or a second settings center. Existing domain authority
and supported platform capability limits remain in force.

## Acceptance criteria

- AC-01: Default, profile creation/rename, profile identification, Open Profile, and New Window are reachable through the shipped application without replacing another profile's windows.
- AC-02: Two profiles can concurrently use different accounts on the same server with isolated credentials, repositories, caches, epochs, private review data, and preferences.
- AC-03: Windows in one profile share accepted review data and preferences while retaining independent open PRs, navigation, layout, and restoration records.
- AC-04: Shared draft/note edit conflicts retain the rejected candidate text and never silently overwrite accepted work; publishing still uses the existing exact-authority mutation path.
- AC-05: Default migration preserves existing appearance, sign-in bindings, private data, and retained history through interruption and restart without duplicate adoption or cross-profile fallback.
- AC-06: Repeated launches admit one runtime owner per profile; explicit new windows and concurrent different profiles work, and closing one window leaves other consumers usable.
- AC-07: Window restoration preserves independent sessions, distinguishes explicit window close from application quit, and exposes the retained Resume destination without fabricating unavailable content.
- AC-08: Settings and sign-out are scoped to the invoking profile, with one Settings window per profile and no effects or late completions reaching another profile.
- AC-09: Typed core/host/Qt boundaries, bounded persistence, stale-result rejection, and failure isolation hold through the composed multi-window application path.

## Direct verification scenarios

| Criteria | Direct surface and discriminating check |
|---|---|
| AC-01, AC-06 | Packaged application: open Work and Personal, launch each again, request additional windows, rename a profile, close one window, and verify runtime/window identities and remaining consumers. |
| AC-02, AC-08 | Two fixture accounts on the same server in two profiles: change settings and sign out in one; inspect the other profile's vault/store partitions and unchanged accepted UI. No live credential or mutation is required. |
| AC-03, AC-04 | Two windows on the same fixture PR: select different views, change a receipt, race revisions of one draft, and verify shared projection, independent navigation, and preserved conflicting text. |
| AC-05 | Prior-schema stores and fake vault: seed non-default colors, grant metadata, receipts, and epochs; interrupt migration at durable boundaries and reopen twice, checking exact identities and single adoption. |
| AC-07 | Public window lifecycle and session store: quit with two windows, restore, explicitly close one, restore again, and inject a missing comparison and broken optional session store. Home rendering is completed by RFC-BPR-0026. |
| AC-09 | Core replay, manual-executor host tests, storage failures, and composed Qt gestures: switch/close windows with jobs pending, deliver stale completions, and prove no wrong-target projection or Qt-owned effects. |

Record exact focused commands and producing commits on the closeout card when
implemented; no product verification has run for this registration. Native
window behavior needs target-native evidence. Subjective interaction evidence
uses the existing attended-review workflow, without blocking independent work.

## Implementation entry and documentation impact

[[BPR-0594]] inventories roots, credential keys, shared records, runtime ownership,
and existing singleton assumptions before allocating bounded implementation
cards. [[BPR-0595]] retains the immutable base and acceptance checklist. Later
work updates architecture, storage/privacy, startup, and settings documentation
only as behavior becomes implemented. The index and linked cards remain the
live execution authority; this decomposition is not a second issue board.

## Remembered connection ownership amendment — 2026-10-12

RFC-BPR-0071 extends existing per-profile/per-server ownership to the selected resumable transport and exact account. Retained window destinations/focus still choose one startup host; its saved connection chooses the transport/account. With no retained review destination, use the most recently selected enabled resumable connection. Closing the last window does not erase it. Sign out durably disables automatic reuse for that server, including an older device grant. Current exact-account acceptance still gates private-state restoration. Successful replacement is atomic; failed or cancelled replacement and late completions cannot replace the durable choice.

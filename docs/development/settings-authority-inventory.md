# Settings authority inventory

BPR-0592, audited 2026-09-07 at `8c66a42c`. This inventories the default
application environment before RFC-BPR-0024 introduces profile isolation.
RFC-BPR-0028 owns the current three-page presentation. RFC-BPR-0018 owns the
remaining Settings work, RFC-BPR-0032 the Accounts & hosts page and RFC-BPR-0034
the Appearance page. The table maps ownership; actual criterion evidence belongs to BPR-0593.
The [Settings guide](settings.md) describes the current user-facing behavior.
The 2026-09-20 reconciliation retains this inventory's original baseline while
updating the later profile, sign-in and updater ownership below.

## Existing adjustable values

| Values | Classification and authority | Persistence, reset and failure scope |
|---|---|---|
| Appearance mode (six choices); UI family, size, weight, line height; code family, size, weight, tab width, ligatures, syntax bold/italic | Durable; `appearance_model.hpp` validates typography, `appearance_application_host` accepts intents and publishes `appearance_projection` | `appearance_settings_store`, schema 7, migrates versions 1 to 6 and preserves the absence of palette overrides; unavailable storage permits labelled session changes. Reset must apply domain defaults through this host, never delete the database. |
| Palette role colors, per-variant stored-palette flags; import, preview, keep, cancel | Durable committed palette per variant, session preview; same appearance host, bounded theme document port; 47 closed roles ([[BPR-0738]] added `syntax_macro`) | Same store; one palette document and one stored flag per variant since schema 7 (RFC-BPR-0034), so commit, reset, import and export are scoped to the variant the composition supplies and no other variant's colours are touched. Preview/cancel changes no durable record; import is bounded and rejects nonregular files. BPR-0680 adds host-owned atomic export of the accepted palette and independent mode/font/palette resets. |
| Files/comments/repositories expanded state and widths; line numbers | Durable default-environment layout; `workbench_interaction` owns semantic state, `workbench_application_host` owns accepted layout | `workbench_settings_store`, schema 8, migrates older layouts; session fallback when unavailable. Reset only layout fields, preserving behavior. Profile/window partitioning belongs to 0024; schema 8 adds one window's Home view to the per-window table only ([[BPR-0783]]). |
| Open modality, Continue modality, mark-read behavior, return comparison, initial comment filter | Durable `review_behaviour_settings`; workbench host validates and publishes accepted layout | Same schema-8 store, which these preferences joined at schema 4; defaults Overview/Read/by hand/whole PR/all. Current ownership validation resides in application types; any expanded policy belongs in core. No new automatic transitions are implied. |
| Comment labels: label marks ([[BPR-1083]]), label helper, Overview counts and ordered Overview chips ([[BPR-1088]]) | The same `review_behaviour_settings`, so the same profile preferences host, window fan-out, refusal and failure path; `core::feedback_label_columns` and its checks own the chip list's validity | Workbench store profile row: marks at schema 12, the other three at schema 13, each defaulted (Icons, helper off, Open, no chips) when absent, with every other field kept. The chips are stored as stable catalog IDs; a stored list naming an unknown or repeated ID loads without it and is reported (`LabelColumnsRecovery`) instead of failing the profile, and is rewritten only by the reviewer's next save. Reset covers these four only. The active Overview count mode is that view's state, not this record. |
| Queue role, observer reason, attention bucket, repository filter, quiet/current filter, sort | Durable `queue_presentation_preferences`; queue enrollment host accepts, Qt presents explicit order above core attention | `queue_enrollment_store`, schema 6, older schemas migrate; failed persistence must not leave optimistic controls. Reset these six fields only. Window filter ownership changes under 0024/0026. |
| Observer rule kind/value and removal | Durable local workflow; `observer_rule` core vocabulary, `queue_rule_intent`, enrollment host | Queue store transaction retains exact enrollment reasons. Rule deletion is local and reconciles only its memberships; never changes GitHub subscriptions. Bounded rule count and string admission remain host/store contracts. |
| Saved repository host/account/repository; save/forget | Durable local workflow, `saved_repository_intent`, queue enrollment host | Queue schema 6; does not currently discover authored/requested PRs merely by saving. Identity remains exact; failed store does not silently enroll. 0026 adds discovery. |
| Remembered enterprise hostname/account; add/forget | Durable noncredential configuration, `saved_server_intent`, queue host; composition creates explicit configured host profiles | Queue schema 6; hostname/account are metadata, never credential authority. Forget removes this server record only. Edit/reconnect must preserve exact authority and use existing host invalidation. |
| Selected GitHub host/transport; manual token entry; device begin/cancel; sign out | Session selection and sensitive actions, core GitHub/device policies and their application hosts | Device binding SQLite metadata plus platform vault support remembered device sessions. Manual credentials remain session-only. Only accepted `/user` identity selects a vault account. Sign out removes exact binding and token slots. Secrets never enter Settings rows. |
| Current account name, grant kind/expiry, connection/capability/rate status | Observed facts, GitHub/device safe projections | Not settings. Do not persist copies or use displayed login as authentication authority. |
| Native notification permission and delivery availability | Platform-managed/observed, `notification_delivery` and platform sink | BPR-0681 persists core enablement/pause choices in queue preferences, separately from platform permission. Preview uses metadata only and existing exact-target activation; ambiguous activation retains its platform refusal. |
| Agent profile choice; host allow/require policy; selected private context | Session selection and compiled policy (`built_in_agent_profiles`, `built_in_agent_policy`); agent invocation/chat hosts | No general profile editor/store currently exists. Four compiled profiles, exact host restrictions, private note/draft opt-in are separate from agent credentials. Settings may expose accepted available choices, never broaden corporate policy or persist private content as preferences. |
| Cache roots, catalog/ledger locations, capacity limits, retained epochs | Observed locations and compiled limits; epoch/cache/ledger hosts | Existing stores remain authorities. Prune/delete are exact-scope destructive operations, not preference resets; reuse confirmation and tombstone paths. No relocation or automatic eviction control exists. |
| Diagnostic availability/detail/export and profile maintenance | RFC-0014 typed release-maintenance host, profile lifecycle and storage ports; `diagnostic_log_port`/Quill remain the local logging authority | Bounded read-only metadata preview and exact-byte local export are implemented. Cleanup/purge first stop the selected profile and prepare an exact inventory, then require confirmation; their commit boundary is independent of Settings closure. RFC-0027 advanced verbosity remains deferred. Native vault/crash/removal qualification is RFC-0038, not an implied Settings permission. |
| Review details disclosure, modality selection, drawer filters/tree order, focused view/selection | Local interaction or retained layout according to its owner; Qt presentation plus workbench intents | Not a universal settings record. Preserve current selection and projections. Independent windows and restoration are 0024/0025 work. |
| Startup landing and session restoration | Desktop lifecycle behavior, not an editable Settings preference | RFC-0024 supplies profile/window identity and restoration. New windows start on Home; no Settings toggle controls startup or restoration. |
| Release account, automatic update checks and skipped version | Application-wide preferences; dedicated RFC-0035 release-update host | Separate update store and exact native GitHub.com release authority. General mounts the same panel/service as the native update action. Unsupported or managed installations are explained; Settings does not grant installation authority. See [application updates](application-updates.md). |

## Baseline gaps (before BPR-0678)

`workbench_window` owned one `settings_window`, adopted the existing appearance
panel and Accounts/Reviews panels, and routed the platform Preferences action.
The shell held seven fixed pages and, at that baseline, provided no scroll
container, Escape close behavior, or visible default-profile identity. The pages
are stable objects; they were retained while bounded scrolling and focus
behavior were added.

At that baseline the three pages reused domain dispatch. General, Notifications,
Agents & context, Storage & privacy were notices. Several notices still named
completed issue IDs. Direct Sign in opened Accounts with Get a code focused,
which did not satisfy RFC-0018's single-gesture begin criterion; sign-in now
uses the dedicated onboarding surface opened from Home, a PR link or the
server card. Supported device authorization starts directly; the same typed
host owns code, cancellation, browser handoff and terminal state.
Appearance was already migrated, so rebuilding a second editor would have
violated its authority.

Core/host generations remain domain-specific: appearance request context and
preview supersession, workbench request/session context, queue session identity,
and GitHub/device authority. The Settings shell must not replace those with a
generic setting bag or accept a late completion merely because a row is visible.

## Server card rows and their authorities

RFC-BPR-0032 makes Accounts & hosts one card per configured server, replacing
the host-profile picker, the locator, the status banner and the servers combo
editor. The card is presentation: every row below reaches an authority that
already existed, and the card creates no credential authority of its own.

| Row or control | What the gesture sends | Authority that performs it |
|---|---|---|
| Header connection state | nothing; it is a projection | GitHub application host session status, said as one word (Connected, Connecting, Signing in, Signed out, Could not connect). The only coloured thing on the page. |
| Account row: avatar, login, consequence sentence | nothing; it is a projection | Accepted session identity and grant provenance from the GitHub and device hosts. The displayed login is not authentication authority. |
| Sign in | the dedicated onboarding flow for the selected server | Existing device-authorization/GitHub hosts through typed gesture dispatch; supported device sign-in starts immediately, and manual/CLI alternatives keep their existing capability gates. No second credential authority is created. |
| Sign out | `deactivate_github_identity_intent` | GitHub application host. It removes the exact binding and both token slots; the button performs no vault effect. |
| Connect | host-profile selection, then the existing connect intent | The composition's configured host profiles and the GitHub application host. This is what replaced selecting a host in a picker. |
| Access | nothing; it is a projection | Provider capability observation. |
| Rate limit | nothing; it is a projection | Provider rate-limit observation. In public preview the same row carries the budget ring, whose only gesture is sign-in. |
| Details | a Qt-local disclosure; Copy details writes the clipboard | Capability and rate-limit observations and the existing bounded diagnostic text. No secret enters it. |
| Remember this server | `saved_server_intent` (add) | Queue enrollment host and queue store, schema 6. Hostname and account name are noncredential metadata and the token is still asked for. The switch shows the record the store keeps rather than a wish: it returns to that record at once and the gesture asks the window. |
| Forget server | `saved_server_intent` (remove), behind a confirmation naming exactly what is removed | Queue enrollment host. It removes this server record only; nothing on the server changes and no sign-in is removed. |
| Add enterprise server | the same saved-server add intent, from a card in its edit state (hostname, optional account, Add, Cancel) | Queue enrollment host. It configures a server to offer, never an account to be. |

## Bounded implementation and criterion map

| Slice | Owned change and criteria | Focused direct check |
|---|---|---|
| BPR-0678 | Settings shell: scrolling, profile label, Escape/focus, stable pages, stale notices; AC-01/02/07/08/09/10 | `qt_settings_window`, `qt_composed_github_scale_100`, rendered real workbench Settings at minimum size; focus lower rows and switch pages without replacing controls |
| BPR-0679 | Direct sign-in/reconnect and exact server setup; AC-04/06/12/13/14 | composed GitHub tests, device authorization public-path course, `application_saved_server_contract`; cancel/expired/manual/refused and wrong-authority cases |
| BPR-0680 | Appearance export and exact local reset through owning host; AC-03/04/05/06/09/10 | `application_appearance_host`, `sqlite_appearance_settings_storage`, `qt_appearance_settings_panel`, theme customization public contract; rejected/import/cancel/restart checks |
| BPR-0681 | General/Reviews/Notifications accepted preferences and honest capabilities; AC-02/04/05/06/09/10 | workbench and queue host/store tests, `application_review_behaviour_contract`, queue composition; metadata-only notification refusal and settings persistence |
| BPR-0682 | Agents/Context and Storage/Privacy capability pages with existing exact operations; AC-02/04/06/09/10 | agent policy/launch/context tests, epoch maintenance public course, composed Settings; no credential/context leakage and Cancel performs no destructive effect |
| BPR-0683 | Final Settings integration, migration/failure isolation and platform/attended evidence; AC-05/07/08/10/11/14 | one focused composed integration pass; relevant store migrations; keyboard/render at 100/125/150/200% and four appearances; tests-disabled platform inventory |

Existing passing records on BPR-0652/0659/0660/0661 are reusable only for
unchanged claims and selected paths. Full RFC-0018 acceptance is not established
by those partial cards. BPR-0593 remains the canonical closeout and owns the
actual criterion/check/result/revision ledger. Required native platform or human
observations remain pending until obtained; this inventory invents no verdict.

## Native inventory and platform rows, 2026-09-21

The [macOS inventory comparison](../verification/BPR-0593-macos-settings-inventory.md)
now supplies a tests-disabled capture at the exact Windows source revision,
equal actionable inventory across all seven pages against the Windows/Linux
captures, and identical current macOS dev/release trees. Native AX wrapper and
combo-name differences are normalized explicitly, not treated as missing rows.

| Later surface | Owner and capability distinction |
|---|---|
| General's nine updater controls | RFC-0035, added after the Windows/Linux inventory baseline. The panel is composed on all platforms; Windows/Linux report unsupported platform. macOS also requires a trusted, writable production installation. Current local dev and ad-hoc release captures both report untrusted installation, disabling all controls except View Release. No update action was invoked. |
| Accounts page-level Use GitHub CLI | BPR-0909/0910/0913/0915 and RFC-0015. Replaces the older per-server Connect entry and opens discovered-account setup through the shared sign-in dialog. Native configured CLI reads remain macOS-only. The current Linux composition omits this button because `cli_read_offered_here()` is false; it has three account controls where Mac has four. This moves presentation, not credential/provider authority. BPR-0933's trust-dialog wording remains separate. |
| Storage & Privacy profile diagnostics/removal | Merged RFC-0014 foundation, now accepted at b7eb3722/61907e20. Seven buttons add bounded preview/export, two prepare/commit pairs and cancellation. Their names and enabled states match the current Mac/Linux capture; no maintenance action was invoked. Exact lifecycle, confirmation and residual rules are in the [storage guide](storage-and-diagnostics.md). |

The [current continuation](../verification/BPR-0593-native-settings-continuation.md)
refreshes all seven Mac/Linux pages at `2d3bdbdf`: updater controls match,
Accounts differs only by the explicit CLI capability above, and the other five
pages match. Six current Mac pages retain their previous controls; Storage adds
the seven RFC-0014 buttons. GObject AT-SPI's `button`/`spin button` names are
explicit aliases of the older `push button`/`spin box` roles, not omitted rows.

Windows refresh remains blocked by Defender quarantine. Linux's inventory-only
course is not its failed native focus/lifetime course or a qualified distribution
package. Owner amendment r2 transfers Windows' security disposition/full course/
current inventory to Blocked BPR-0974 and Linux's full focus/lifetime course to
Blocked BPR-0975. BPR-0593 closes only the amended accepted scope. The original
observations retain these precise limits; old comparable evidence remains valid
only for its original source, not a new security verdict or native course pass.

Since then, on 2026-09-25: BPR-0974 is Ready with the owner's disposition
(lower the image's static signals and go ahead). BPR-0975 is Done by owner
amendment: WSLg's refusal of application activation is accepted with no
product change, and BPR-1122 (Blocked, `upstream-wslg`) runs the full course
again once WSLg ships the upstream fix.

## Integrated Settings surfaces

BPR-0678 through BPR-0682 now compose all seven categories in the Default
environment. The shell scrolls each stable page, identifies the edited profile,
returns focus on close and uses measured category widths in small windows
(BPR-0685). Appearance export/scoped resets and notification preferences use
their existing owning hosts. Agent/context rows share offered identities and
per-request opt-ins; Storage actions share exact existing maintenance commands.
BPR-0684 closes visible previews and retires pending imports while allowing
accepted durable work to finish. The standalone shell's General notice is
replaced in the desktop composition by RFC-0035's update panel. Profile/window
restoration belongs to RFC-0024's lifecycle, not a General preference.

The source inventory above remains domain-specific. This is not a second status
board: BPR-0593 holds the integration criterion ledger, BPR-0683 the native/human
handoff, and BPR-0686 the migration-preflight defect found by independent review.

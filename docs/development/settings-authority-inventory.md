# Settings authority inventory

BPR-0592, audited 2026-09-07 at `8c66a42c`. This inventories the default
application environment before RFC-BPR-0024 introduces profile isolation.
RFC-BPR-0028 owns the current three-page presentation. RFC-BPR-0018 owns the
remaining Settings work. Planned checks below are not completed evidence.

## Existing adjustable values

| Values | Classification and authority | Persistence, reset and failure scope |
|---|---|---|
| Appearance mode (six choices); UI family, size, weight, line height; code family, size, weight, tab width, ligatures, syntax bold/italic | Durable; `appearance_model.hpp` validates typography, `appearance_application_host` accepts intents and publishes `appearance_projection` | `appearance_settings_store`, schema 3, migrates v1/v2 and preserves the absence of palette overrides; unavailable storage permits labelled session changes. Reset must apply domain defaults through this host, never delete the database. |
| Palette role colors, stored-palette flag and variant; import, preview, keep, cancel | Durable committed palette, session preview; same appearance host, bounded theme document port; 46 closed roles | Same store; preview/cancel changes no durable record; import is bounded and rejects nonregular files. BPR-0680 adds host-owned atomic export of the accepted palette and independent mode/font/palette resets. |
| Files/comments/repositories expanded state and widths; line numbers | Durable default-environment layout; `workbench_interaction` owns semantic state, `workbench_application_host` owns accepted layout | `workbench_settings_store`, schema 4, migrates older layouts; session fallback when unavailable. Reset only layout fields, preserving behavior. Profile/window partitioning belongs to 0024. |
| Open modality, Continue modality, mark-read behavior, return comparison, initial comment filter | Durable `review_behaviour_settings`; workbench host validates and publishes accepted layout | Same schema-4 store; defaults Overview/Read/by hand/whole PR/all. Current ownership validation resides in application types; any expanded policy belongs in core. No new automatic transitions are implied. |
| Queue role, observer reason, attention bucket, repository filter, quiet/current filter, sort | Durable `queue_presentation_preferences`; queue enrollment host accepts, Qt presents explicit order above core attention | `queue_enrollment_store`, schema 5, older schemas migrate; failed persistence must not leave optimistic controls. Reset these six fields only. Window filter ownership changes under 0024/0026. |
| Observer rule kind/value and removal | Durable local workflow; `observer_rule` core vocabulary, `queue_rule_intent`, enrollment host | Queue store transaction retains exact enrollment reasons. Rule deletion is local and reconciles only its memberships; never changes GitHub subscriptions. Bounded rule count and string admission remain host/store contracts. |
| Saved repository host/account/repository; save/forget | Durable local workflow, `saved_repository_intent`, queue enrollment host | Queue schema 5; does not currently discover authored/requested PRs merely by saving. Identity remains exact; failed store does not silently enroll. 0026 adds discovery. |
| Remembered enterprise hostname/account; add/forget | Durable noncredential configuration, `saved_server_intent`, queue host; composition creates explicit configured host profiles | Queue schema 5; hostname/account are metadata, never credential authority. Forget removes this server record only. Edit/reconnect must preserve exact authority and use existing host invalidation. |
| Selected GitHub host/transport; manual token entry; device begin/cancel; sign out | Session selection and sensitive actions, core GitHub/device policies and their application hosts | Device binding SQLite metadata plus platform vault support remembered device sessions. Manual credentials remain session-only. Only accepted `/user` identity selects a vault account. Sign out removes exact binding and token slots. Secrets never enter Settings rows. |
| Current account name, grant kind/expiry, connection/capability/rate status | Observed facts, GitHub/device safe projections | Not settings. Do not persist copies or use displayed login as authentication authority. |
| Native notification permission and delivery availability | Platform-managed/observed, `notification_delivery` and platform sink | No user-editable preference store currently exists. Settings must explain unknown permission honestly; enablement/quiet choices need bounded domain settings before controls are offered. Preview must use metadata only and existing exact-target activation. |
| Agent profile choice; host allow/require policy; selected private context | Session selection and compiled policy (`built_in_agent_profiles`, `built_in_agent_policy`); agent invocation/chat hosts | No general profile editor/store currently exists. Four compiled profiles, exact host restrictions, private note/draft opt-in are separate from agent credentials. Settings may expose accepted available choices, never broaden corporate policy or persist private content as preferences. |
| Cache roots, catalog/ledger locations, capacity limits, retained epochs | Observed locations and compiled limits; epoch/cache/ledger hosts | Existing stores remain authorities. Prune/delete are exact-scope destructive operations, not preference resets; reuse confirmation and tombstone paths. No relocation or automatic eviction control exists. |
| Diagnostic availability/detail/export | Observed basic logging; `diagnostic_log_port`, Quill adapter/composition | RFC-0027 advanced verbosity/export is deferred and not an existing editable preference. Present capability honestly; do not invent an export containing source, credentials, or database bytes. |
| Review details disclosure, modality selection, drawer filters/tree order, focused view/selection | Local interaction or retained layout according to its owner; Qt presentation plus workbench intents | Not a universal settings record. Preserve current selection and projections. Independent windows and restoration are 0024/0025 work. |
| Startup landing and session restoration | No composed user preference yet | General must describe present behavior until 0024 provides typed profile/window restoration. Automatic update controls are outside scope. |

## Selected paths and gaps

`workbench_window` owns one `settings_window`, adopts the existing appearance
panel and Accounts/Reviews panels, and routes the platform Preferences action.
The shell holds seven fixed pages, but currently provides no scroll container,
Escape close behavior, or visible default-profile identity. The pages are stable
objects; retain them while adding bounded scrolling and focus behavior.

The current three pages reuse domain dispatch. General, Notifications,
Agents & context, Storage & privacy are notices. Several notices still name
completed issue IDs. Direct Sign in currently opens Accounts with Get a code
focused; it does not satisfy RFC-0018's single-gesture begin criterion. Appearance
is already migrated, so rebuilding a second editor would violate its authority.

Core/host generations remain domain-specific: appearance request context and
preview supersession, workbench request/session context, queue session identity,
and GitHub/device authority. The Settings shell must not replace those with a
generic setting bag or accept a late completion merely because a row is visible.

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

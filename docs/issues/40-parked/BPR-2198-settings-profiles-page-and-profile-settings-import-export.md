---
schema: "implement-rfc.card.v1"
id: "BPR-2198"
title: "Settings › Profiles, and Profile Settings import and export"
status: "Parked"
rfcs: ["RFC-BPR-0107", "RFC-BPR-0033", "RFC-BPR-0112"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2198--BPR-2198--7acef9c46352.json","follows":"BPR-2032","import_preview":"8af4a4501"}
depends_on: ["BPR-2032"]
parked_on: "human-review"
---

# BPR-2198: Settings › Profiles, and Profile Settings import and export

## Scope and acceptance

The owner's direction of 2026-11-15, given while the import preview
(BPR-2032, design `workflow-import-v1` B, built at `8af4a4501`) was being
built. His words, verbatim, in the order given:

> Yeah i would expect it in settings, and maybe that's the right place for
> this import/export

(he meant profiles)

> I think the manage profiles window is probably kinda booty, maybe it just
> goes away

> And so this feature becomes import profile settings export profile
> settings and identity is one of the options

> Repositories should also be in that list

And: the configuration file has never been released (no v0.4.x tag contains
`agent_configuration`), so it keeps no backward compatibility.

### What to build

1. **Settings › Profiles**, a new page right after General, designed fresh
   in the style of the other Settings pages (load `frontend-design`). It is
   the only place profiles are managed: the profile list with the old
   window's model and actions (create, rename, open or switch, new window,
   reset data, delete, Try again on a failure), not its look.
2. **Retire `profile_management_window`** and its tests once nothing uses
   it; move its coverage to the page. The Profiles menu's Manage Profiles…
   opens Settings on Profiles; with no window open it opens Settings for the
   current or last profile.
3. **Share configuration** on the page: **Import Profile Settings…** and
   **Export Profile Settings…**, with one line under them: "A file with this
   profile's agents, prompts and workflows, to move to another computer or
   share." Import lands in the current profile and the preview names that
   profile (already built: the preview's first sentence names it). The
   buttons leave Settings › Agents and are not added to General. Theme
   import and export stay on Appearance. Dialogs and help use the same names.
4. **Sections.** Export lists the profile's parts as sections to tick:
   Agents, Prompts, Workflows, **Identity** and **Repositories**. Import shows
   the same sections in the list-and-detail preview.
   - Identity: the profile's name and which accounts and hosts it uses
     (GitHub.com and GHES hosts, the account login for each). Starts
     **unticked** on export. An imported identity names the accounts, each
     saying "Sign in to use" and reusing the existing sign-in flow.
   - Repositories: the saved and followed repositories (host, owner/name)
     and the per-repository settings that are safe to carry (per-repository
     prompt layers; the bot list if it lives there). No local paths: no
     worktree or checkout location. Starts **ticked** on export. A
     repository whose host or account is not signed in says "Sign in to use".
5. **Never a credential in the file**: no token, keychain item, `gh` auth,
   cookie or secret of any kind. A test exports a profile with a live
   sign-in and asserts no secret is in the file, Identity and Repositories
   included.
6. **One format, no compatibility.** One Profile Settings file format with
   the sections Agents, Prompts, Workflows, Identity and Repositories and a
   single version marker so a later format refuses cleanly. Delete the
   older-format reading (versions 1 and 2 of `bpr.agent-configuration`), its
   migration and its tests.
7. **Architecture.** The core decides each section's items, standing and
   starting choice (`core::plan_configuration_import` grows the two
   sections); the host reads the profile's accounts and repositories and
   applies; Qt presents.

### To check

- Whether the Settings window is per profile, and that the page behaves
  across profiles (it lists every profile; Import lands in the window's own
  profile). Record anything that does not here.
- What the old window does that has no obvious home on the page: it is also
  RFC-BPR-0033's profile chooser, shown when no window is open and on a
  refusal with no live window. Its replacement must be named before the
  window is deleted, not dropped.

### Open questions, with recommended defaults (not built)

- **Appearance** as a section: recommended out for now; theme import and
  export already live on Appearance.
- **Notifications, Home and queue, Review and Comments preferences**:
  recommended out; they are personal and small, and nothing asked for them.
- **Per-repository local paths** (worktree roots): out, by the owner's rule.

## Status

Built at `304a77a08` on `claude/v0-5-0-prerelease-targets-52265d`; parked
on the owner's look `profiles-v001`.

- **Settings › Profiles** (after General): the retired window's list,
  gestures and failure notice, with Share profile settings (Import Profile
  Settings…, Export Profile Settings…, the owner's line). The buttons left
  Settings › Agents. Manage Profiles… opens the window's own Settings on
  Profiles. Delete and Reset data are not on it: the old window had neither,
  no delete intent exists (RFC-BPR-0033 lists deletion as a non-goal), and
  Reset data stays under Troubleshooting on Storage & privacy.
- **The chooser's home** (recommended and built): the desktop's own Settings
  window with Profiles alone, no sidebar and no sharing (no profile is open
  to import into). With no window open it is what Manage Profiles… opens,
  rather than Settings "for the current or last profile": no runtime is
  open then. `profile_management_window` and its tests are deleted; the
  seven end-to-end courses, `qt_profiles_settings_panel` (was
  `qt_profile_management_window`) and the capture, fit and quiet-button
  courses use the page.
- **Profile Settings**: `bpr.profile-settings` version 1, one marker; the
  version 2 workflow file, the retired prompt aliases and the optional agent
  fields are deleted with their tests. Sections tick as a whole or item by
  item; `core::exported_by_default` (all but Identity, so Workflows now
  starts ticked), `core::plan_configuration_import` (Identity unticked,
  Sign in to use), `core::profile_changes`. Identity: profile name and
  accounts by host and login (signed-in session plus remembered servers).
  Repositories: saved (host, owner/name) and followed (observer repository
  rules, which have no host). There are no per-repository prompt layers or
  bot list in a profile today, so nothing more is carried. Import applies
  through the profile's own intents (rename, remember server, save
  repository, follow); nothing signs in.
- **No credential**: `application_profile_settings_export_public_path` signs
  in through the composed device flow, exports with Identity ticked, and
  finds no token, device code or secret.
- **Settings window per profile**: each window's Settings is its profile's
  (Import lands there); the page lists every profile.

Checks at `304a77a08`: core_agent_configuration, core_workflow,
qt_agent_configuration_dialogs, qt_settings_window (4 scales),
qt_surface_capture_course, qt_home_card_fit, qt_quiet_buttons,
qt_button_faces, sqlite_agent_configuration_storage, the seven profile
courses and personal discovery, the new export course, local draft
workflow, architecture_boundaries, test_network_isolation_manifest, the app
pin, tidy on the changed units and DOCUMENTATION_OK. Not passing, and not
caused here: `qt_profiles_settings_panel`'s last case (the desktop bar's
Pull Request lacks Mark New Commits Seen and What Changed Since My Review,
since 5bee3dd30; the page's own cases run first and pass);
`application_profile_management_public_path` stops at its Dif catalog
check (the window's Dif is null here; with those four checks set aside the
migrated course passes); `architecture_boundary_contract` (its fixture
lacks `src/app/CMakeLists.txt`, since 3a224ad64). The native menu bar course
needs the owner's Aqua session and was not run.

### Owner look profiles-v001 (2026-11-15)

Session `9548f96f68d753d4e887ba9f3e444eb6`, state
`/Users/jaeger/.attended-review-state/profiles-v001-2026-11-15/state` (URL in
that directory's `serve.log`), cursor 12, host PID 72549. Frozen root
`/Users/jaeger/profiles-v001.xsxjloz8` from `304a77a08` (executable SHA-256
`0df8680285ec…`; the launcher's check reached
`ATTENDED_PROFILES_SHOWN:profiles=2:page=profiles:share=shown` with the
Fixture agent and no real agent). Four Accept/Reject questions:
`BPR-2198-profiles-page`, `BPR-2198-export-sections`,
`BPR-2198-import-identity-repositories`, `BPR-2198-chooser-home`.

### Open questions for the owner (recommended out, not built)

- **Appearance** as a section: out; theme import and export stay on
  Appearance.
- **Notifications, Home & queue, Review and Comments preferences**: out.
- **Per-repository local paths** (clone folders, linked clones): out, by
  the owner's rule.
- **Observer rules by author or path**: out; only repository follows are
  "followed repositories".

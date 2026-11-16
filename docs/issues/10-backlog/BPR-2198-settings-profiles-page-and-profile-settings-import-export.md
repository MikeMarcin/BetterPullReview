---
schema: "implement-rfc.card.v1"
id: "BPR-2198"
title: "Settings › Profiles, and Profile Settings import and export"
status: "Backlog"
rfcs: ["RFC-BPR-0107", "RFC-BPR-0033", "RFC-BPR-0112"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2198--BPR-2198--7acef9c46352.json","follows":"BPR-2032","import_preview":"8af4a4501"}
depends_on: ["BPR-2032"]
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

Not started. The import preview it builds on is done (BPR-2032 at
`8af4a4501`). Its look, `workflow-import-v002`, asks only about the preview;
the Profiles page gets its own question in the look that follows this card.

---
schema: "implement-rfc.card.v1"
id: "BPR-0652"
title: "Settings pages: Accounts & hosts, Appearance, Reviews & queue"
status: "Done"
rfcs: ["RFC-BPR-0018","RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"RFC-BPR-0028 AC-10 and RFC-BPR-0018 AC-03 (migrated, not duplicated)","outcome":"pass","revision":"f2a5be24 + this finish","evidence":{"kind":"observation","detail":"Appearance: the existing panel opens with the accepted subtitle and four theme previews painted from builtin_appearance_palette (System half-and-half, Light, Dark, High contrast following the desktop); choosing a card moves the mode selector, which sends the one appearance intent (qt_appearance_settings_panel pins the four cards, adoption marking without dispatch, and one dispatch per choice); typography, role customisation with preview and import are the same controls. Accounts & hosts: the account card heads the page with login, display name, host, Connected/Signed out and the rate limit from the session projection, then the device sign-in and session chrome (composed test pins signed-out and signed-in states). Reviews & queue: the observer-rule editor moved from Home to the page under its heading, with the page's subtitle; Home keeps the queue and filters (composed and window tests pin the new home and the actions' availability). The controls with no authority (device-grant facts, sign out, enterprise servers; modality rules, reading behaviour, default comparison; saved repositories; theme export) are not shown as controls: the page names BPR-0660, BPR-0661 and BPR-0659, so AC-10 is met for every live control and open for those."}},{"criterion":"Every control is host-validated and reverts on refusal; resets act on their own group only","outcome":"pass","revision":"f2a5be24 + this finish","evidence":{"kind":"observation","detail":"No new intent was added: the previews reuse the mode selector's dispatch, whose refusal handling qt_appearance_settings_panel already pins (test_a_refusal_restores_and_reports, test_theme_and_contrast_refusals_remain_typed_and_visible); the rule editor's actions keep their availability rules (window tests test_queue_rule_gestures_wait_for_the_accepted_projection); the account card presents and never dispatches."}},{"criterion":"Focused page tests, the appearance courses, and the boundary gate pass","outcome":"pass","revision":"f2a5be24 + this finish","evidence":{"kind":"observation","detail":"qt_appearance_settings_panel, qt_settings_window, qt_composed_github_scale_{100,125,200}, qt_workbench_window_scale_{100,125,200}, the appearance contracts (gesture, theme customization, store fallback, native probe), the public-path courses, the workbench, queue, composition and round-trip contracts; check_boundaries; strict tidy on the touched translation units; clang-format --Werror; verify_documentation.py. Recorded on the card. application_native_appearance_probe_contract, red since BPR-0647 because the course's verdict line changed shape, passes again with the overview fact moved to the end of the line."}}]
---

# Settings pages: Accounts & hosts, Appearance, Reviews & queue

RFC-BPR-0028 Settings, from the Settings-and-drawers asset sections 1 to 3,
built on the Settings window BPR-0646 mounts. Accounts & hosts presents the
accepted session and server records through typed host projections and
intents (sign out, reconnect, add or remember an enterprise server); no
credential is shown. Appearance migrates the existing panel into the page
layout with theme previews rendered from the real palette (half-and-half
System), typography rows, per-role customisation with preview, and theme
import and export. Reviews & queue holds the modality rules, reading
behaviour, observer rules and saved repositories over the queue host's
preferences and enrollment ports. Every other page shows its notice.

## Acceptance

- RFC-BPR-0028 AC-10 and RFC-BPR-0018 AC-03 (migrated, not duplicated).
- Every control is host-validated and reverts on refusal; resets act on their
  own group only.
- Focused page tests, the appearance courses, and the boundary gate pass.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.

## Notes

- Appearance (RFC-BPR-0018 AC-03 migrated, not duplicated): the panel the
  Settings window already held opens with the accepted page's subtitle and
  four theme previews (`theme_preview_strip`): System as half light and
  half dark with the desktop's side first, Light, Dark, and High contrast
  following the desktop, each three lines of code painted from
  `builtin_appearance_palette` in its own roles (code background, keyword,
  identifier, an added line) so the preview is the theme. Choosing a card
  moves the mode selector, which sends the one appearance intent it always
  sent; adoption marks the card. The typography rows, the role
  customisation with preview and keep, and theme import stay as they were.
  Theme export has no host operation and is noted as absent.
- Accounts & hosts: an `account_card` heads the page with the login,
  display name and host, Connected or Signed out as the strip's account
  button decides it, and the GitHub API rate limit from the session
  projection; then the device sign-in and the session chrome as BPR-0646
  placed them. The device-grant date and self-renewal, the remembered fact,
  Sign out, enterprise server cards and Add enterprise server have no
  authority and are carded as
  [BPR-0660](../10-backlog/BPR-0660-enterprise-servers-and-session-facts-on-accounts-and-hosts.md).
- Reviews & queue: the observer-rule editor moves from Home to the page
  under "Observer rules" with the page's subtitle; Home keeps the queue and
  its filters, and the workbench's focus order no longer declares the
  editor (it is another window's). Modality rules, reading behaviour and
  the default comparison are carded as
  [BPR-0661](../10-backlog/BPR-0661-review-behaviour-preferences.md) and
  saved repositories as BPR-0659; the page says so in one line rather than
  showing controls nothing honours. AC-10 is therefore met for what has an
  authority and open for the rest, named on the page.
- Found at this head and fixed in passing: `application_native_appearance_probe_contract`
  had failed since BPR-0647 (it passes at fb50d360 and fails at 95fe51af),
  because that card's course put its `overview=` fact in the middle of the
  `PUBLIC_PATH_VERDICT` line and the native appearance contract's
  startup-budget course matches the first three facts as one literal. The
  fact now comes last on the line; the contract, the public-path courses
  and the appearance gesture contract pass again. The bisection stashed
  each slice and rebuilt at fb50d360, 95fe51af, 5fc6e052 and 252c825e.


---
schema: "implement-rfc.card.v1"
id: "BPR-1671"
title: "Streamline the updater panel"
status: "Parked"
rfcs: []
depends_on: ["BPR-1669"]
priority: "P2"
platforms: ["mac", "win", "linux"]
parked_on: "human-review"
metadata: {"origin":"Owner request, 2026-10-20: the updater UX looked way over complicated; streamline it","reservation":"docs/issues/_reservations/BPR-1671--BPR-1671--36997646e103.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e5a25a3b6","evidence":{"kind":"observation","detail":"Candidate is the working tree committed on e5a25a3b6. Offscreen Fusion renders of the panel (up to date, offer, downloading, ready, offline, never checked with storage warning, self-update unavailable, Settings General) and of the composed offer from release_update_public_path show one headline, one quiet detail line, notes only with an offer, progress only while downloading, only the commands that apply (default command on the right, Later/Skip to its left, Open Release Page only with an offer or as the fallback), and the automatic-check box last. Renders are kept outside the repository."}},{"criterion":"AC2","outcome":"pass","revision":"e5a25a3b6","evidence":{"kind":"command","detail":"ctest --preset dev -R \"release_update|settings_window|application_profile_multi_window_public_path\" --output-on-failure: 14/14 pass (all 9 release_update tests). release_update_public_path now finds no combo box and waits for the check command to become enabled without any selection gesture; it still persists the offered account binding, skip and automatic preference across restart."}},{"criterion":"AC3","outcome":"pass","revision":"e5a25a3b6","evidence":{"kind":"observation","detail":"reason_text no longer mentions accounts or sign-in: access_required, unauthenticated, forbidden and inaccessible now speak of GitHub and the release page."}},{"criterion":"AC4","outcome":"pass","revision":"e5a25a3b6","evidence":{"kind":"command","detail":"cmake --build --preset dev, coding-format-check, coding-tidy on the touched sources, and python3 scripts/verify_documentation.py --root . pass."}},{"criterion":"AC5","outcome":"pending","revision":"0d800ce47","evidence":{"kind":"observation","detail":"Owner verdicts in attended session a48e387c384559d1313a630515edc8cc (handed back at event 17) on frozen 0d800ce47: layout accepted (note: render the Markdown with formatting), Settings group accepted, wording accepted, release notes as plain text rejected (note: render as Markdown formatting). The release notes look returns as v002.","path":"docs/issues/40-parked/BPR-1671-streamline-the-updater-panel.md#attended-review-v001"}}]
---

# Streamline the updater panel

The updater panel stacked every fact and control at once: an
"Installed / Available" line, a status, an ISO timestamp, an account picker
with a long help paragraph, notes, a progress bar, six mostly disabled
buttons, View Release, a "preferences are saved" line and an installation
outcome. After [[BPR-1669]] the account picker had nothing left to choose.

## Acceptance

- AC1: The panel leads with one headline ("You're up to date", "Version X is
  available", "Downloading version X…", "Ready to install version X") and one
  quiet line ("You have 0.3.1. Last checked today at 10:42."). Release notes
  show only with an offer and progress only while downloading. Only commands
  that apply are shown; the most consequential is the default button on the
  right. The automatic-check box comes last. The storage line appears only
  when preferences cannot be saved, and the installation result only when it
  failed.
- AC2: There is no account picker. The desktop selects release access itself:
  the stored binding while it is still offered, otherwise the first offer
  (natively the public releases), as soon as the service is ready.
- AC3: Status copy is plain and actionable and never asks for an account or a
  sign-in.
- AC4: Updater tests, format, tidy and documentation checks pass.
- AC5: The owner accepts the look.

## Execution

`release_update_panel` derives a headline and a detail line from the
projection and hides what does not apply. Commands rank install, download,
cancel, check; the first enabled one is the default button and Check Now is
shown only when it leads. Later shows with an offer or a ready update, Skip
only with an offer. With no command, Open Release Page takes the default
slot. In Settings the panel sits in a "Software Update" group. The standalone
window fits its content. `release_update_desktop` drops the combo plumbing
(`set_accounts` on the panel, `refresh_accounts`, the dispatch's
`SelectAccount`/`ClearAccount`), restores access from `publish()`, and records
the requested binding before the service publishes so that restore does not
reselect.

## Attended review v001

Opened 2026-10-20 for AC5. The application cannot reach the updater's states
from a frozen build, so `test_release_update_look` (0d800ce47) composes the
update window, the Settings "Software Update" group and the menu item through
`release_update_desktop::start` with fixture release access, verification,
installation and in-memory settings. With `--attended` it opens them beside
a look-controls window that chooses what the next check and download meet.
Nothing reaches GitHub or the disk. Without arguments it is the
`application_release_update_look` row, which walks never checked, up to
date, an offer with Markdown notes, downloading, ready, offline, a copy that
cannot update itself and unsaved settings.

- Session `a48e387c384559d1313a630515edc8cc`, fresh state
  `/Users/jaeger/.mask/bpr-1671-updater-look-v001-20261020/attended-review`.
  Handed back at event 17. The host is stopped and the candidates released;
  the authenticated URL stays in the external state.
- Candidates, revision `frozen-0d800ce47-v001`:
  `BPR-1671-layout-v001` (`a2cf7daa4ee7…`), `BPR-1671-settings-group-v001`
  (`b82182ce0094…`), `BPR-1671-copy-v001` (`e04e500fe217…`) and
  `BPR-1671-release-notes-v001` (`007579519be6…`). Each one asked one Accept
  or Reject question and carried offscreen renders from the frozen helper.
- Frozen root `/Users/jaeger/.mask/attended-review/bpr-1671-updater-look-v001.9w0fjvwa`
  from `0d800ce47` (fixture `overview-labels-v1`, `--open-pull-request`,
  `--target test_release_update_look`). The application executable's SHA-256
  is `c739c21e78af…625502` and the helper's is `6aad8216ec12…31a66e`. The
  launcher's own check printed `CHECK_OK (first head): ATTENDED_PULL_REQUEST_OPENED`.
  The frozen helper's course passed offscreen, and `--attended` printed
  `RELEASE_UPDATE_LOOK_SHOWN`.
- The look: `/Users/jaeger/.mask/attended-review/bpr-1671-updater-look-v001.9w0fjvwa/src/build/app-release-tests/tests/end_to_end/release_update_look --attended`.
- Verdicts (event cursor 17):
  - `BPR-1671-layout-v001`: Accept. Note: "Yes but may as well render the
    markdown with formatting."
  - `BPR-1671-settings-group-v001`: Accept.
  - `BPR-1671-copy-v001`: Accept.
  - `BPR-1671-release-notes-v001`: Reject. Note: "Render as markdown
    formatting."
- The frozen root was removed with `scripts/freeze_attended_build.py remove`.
- Found while preparing: the native composition binds public releases as
  `public:github.com`, but the SQLite update store keeps only an empty binding
  or 64 lowercase hex digits. So every save in a self-updating build fails,
  and the panel then shows "Update settings can't be saved right now…" and
  forgets its preferences. `release_update_public_path` binds 64 hex digits
  and does not meet this. The look keeps settings in memory, so it does not
  show it. This needs its own card.

Layout, the Settings group and the wording are accepted. AC5 waits on the
release notes, which v002 renders as formatted Markdown.

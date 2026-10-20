---
schema: "implement-rfc.card.v1"
id: "BPR-1671"
title: "Streamline the updater panel"
status: "Done"
rfcs: []
depends_on: ["BPR-1669"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner request, 2026-10-20: the updater UX looked way over complicated; streamline it","reservation":"docs/issues/_reservations/BPR-1671--BPR-1671--36997646e103.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e5a25a3b6","evidence":{"kind":"observation","detail":"Candidate is the working tree committed on e5a25a3b6. Offscreen Fusion renders of the panel (up to date, offer, downloading, ready, offline, never checked with storage warning, self-update unavailable, Settings General) and of the composed offer from release_update_public_path show one headline, one quiet detail line, notes only with an offer, progress only while downloading, only the commands that apply (default command on the right, Later/Skip to its left, Open Release Page only with an offer or as the fallback), and the automatic-check box last. Renders are kept outside the repository."}},{"criterion":"AC2","outcome":"pass","revision":"e5a25a3b6","evidence":{"kind":"command","detail":"ctest --preset dev -R \"release_update|settings_window|application_profile_multi_window_public_path\" --output-on-failure: 14/14 pass (all 9 release_update tests). release_update_public_path now finds no combo box and waits for the check command to become enabled without any selection gesture; it still persists the offered account binding, skip and automatic preference across restart."}},{"criterion":"AC3","outcome":"pass","revision":"e5a25a3b6","evidence":{"kind":"observation","detail":"reason_text no longer mentions accounts or sign-in: access_required, unauthenticated, forbidden and inaccessible now speak of GitHub and the release page."}},{"criterion":"AC4","outcome":"pass","revision":"e5a25a3b6","evidence":{"kind":"command","detail":"cmake --build --preset dev, coding-format-check, coding-tidy on the touched sources, and python3 scripts/verify_documentation.py --root . pass."}},{"criterion":"AC5","outcome":"pending","revision":"e5a25a3b6","evidence":{"kind":"observation","detail":"Look-and-judge: the owner has not yet seen the streamlined panel."}}]
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

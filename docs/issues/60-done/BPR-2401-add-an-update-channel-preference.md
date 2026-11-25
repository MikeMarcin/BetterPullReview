---
schema: "implement-rfc.card.v1"
id: "BPR-2401"
title: "Choose whether to receive prerelease updates"
status: "Done"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-2395"]
metadata: {"milestone_plan":{"id":"bpr-2401-update-channel","path":"docs/rfcs/plans/bpr-2401-update-channel/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"2794d85ed9b912d2a8809958932de024a32789ca","evidence":{"kind":"observation","detail":"The offscreen Settings keyboard course starts on Stable only, changes to Include prereleases, reopens the desktop and storage to restore the choice, then persists Stable only again. sqlite_private_store_open proves the previous v1 row defaults to Stable only and the previous writer can update it without erasing opting in."}},{"criterion":"AC2","outcome":"pass","revision":"2794d85ed9b912d2a8809958932de024a32789ca","evidence":{"kind":"command","detail":"core_release_update, release_update_transport_contract, release_update_native_https and application_release_update_public_path pass. Fixtures cover the exact stable versus opt-in routes, draft exclusion, numeric version and build order, malformed numbered tags and ZIP mismatches, and progression from RC 103 to stable 104. The release-list TLS fixture transfers 1,212,810 bytes."}},{"criterion":"AC3","outcome":"pass","revision":"2794d85ed9b912d2a8809958932de024a32789ca","evidence":{"kind":"observation","detail":"The public course discards the staged RC on a channel switch and requires Install and Restart before the selected-channel refresh and one exact handoff. The core rejects a stale check, the helper verifies the reconstructed RC and receipt, and native staging accepts the actual signed RC 99 ZIP but refuses the same bytes under build 100."}}]
---

The owner asked for an app setting to choose prerelease builds or stable builds
only. Add the choice to the existing application-wide Updates panel.

- AC1. Stable only is the default for fresh and existing settings. The user can
  opt in to prerelease updates, and both choices survive restart.
- AC2. Stable only retains the stable lookup. Including prereleases offers a
  compatible newer published build, whether prerelease or stable. Discovery
  excludes drafts and refuses downgrades, malformed numbered tags, and assets
  that do not match the release identity.
- AC3. An in-flight check, offer or staged bundle belongs to the selected
  channel. Changing channels invalidates obsolete work, and download and install
  refresh use the same channel and exact candidate. The signed native build
  and explicit install confirmation remain required.

---
schema: "implement-rfc.card.v1"
id: "BPR-2401"
title: "Choose whether to receive prerelease updates"
status: "Ready"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-2395"]
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

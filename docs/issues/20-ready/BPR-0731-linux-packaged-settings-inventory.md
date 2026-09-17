---
schema: "implement-rfc.card.v1"
id: "BPR-0731"
title: "Linux packaged Settings inventory (RFC-BPR-0018 AC-11)"
status: "Ready"
rfcs: ["RFC-BPR-0018"]
depends_on: []
priority: "P2"
platforms: ["linux"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"f2fb9b06-0968-41f0-895f-c2ef23c05acd"}
---

# Linux packaged Settings inventory (RFC-BPR-0018 AC-11)

The owner accepted the macOS half of RFC-BPR-0018 AC-11 on
[BPR-0683](../60-done/BPR-0683-settings-integration-and-native-acceptance.md)
in the v006 attended session (decision `f2fb9b06`, 18:38:18Z). The
criterion reads: tests-disabled packaged applications on macOS, Windows
and Linux expose the same setting inventory except for explicitly
documented platform capability rows. This card is the Linux half; it is
platform work and stays in Backlog until the Linux station takes it
([remote stations](../../development/remote-stations.md)).

## Scope

- Build the tests-disabled package on the Linux station at a head at or
  after d1ba791f, open Settings and record the inventory: all seven
  categories (General, Accounts & hosts, Appearance, Reviews & queue,
  Notifications, Agents & context, Storage & privacy) and every row,
  against the dev build's rows on the same head.
- Any missing row is a platform capability row that the page explains;
  anything else is a defect to card.
- Owned paths: the Linux packaging and the Settings inventory evidence
  under `docs/verification`; no product change in this card.

## Acceptance

- The packaged inventory on Linux matches the dev build's, with each
  difference named as a documented capability row.
- The captures or the recorded inventory are linked from the
  [BPR-0593](../10-backlog/BPR-0593-close-rfc-0018.md) closeout ledger's AC-11 row.
- Where a person must look at it, an attended candidate on that station.

## Owner direction, 2026-09-11

Asked when the station takes this: "After we sort all the MacOS and
platform agnostic stuff out". It stays in Backlog behind the macOS and
platform-agnostic cards of the pack (BPR-0725 through BPR-0729, BPR-0734
and the Home cards BPR-0732 and BPR-0733); the [BPR-0593](../10-backlog/BPR-0593-close-rfc-0018.md)
closeout records the order.

## Linux station inventory, 2026-09-17

Station `bpr-linux` recorded the inventory during task
`task-20260917-lnx-evidence` (`dsp-001-r2`), in
[BPR-0731's Linux Settings inventory](../../verification/BPR-0731-linux-settings-inventory.md),
with captures
[`linux-app-release.json`](../../verification/BPR-0731-linux-settings-inventory/linux-app-release.json)
and [`linux-dev.json`](../../verification/BPR-0731-linux-settings-inventory/linux-dev.json).

- **Builds.** Product source `9bbe3604` (at or after `d1ba791f`), GCC 15.2.
  - The tests-disabled `app-release` image's SHA-256 is `da30fc69…f058`.
  - The `dev` image's SHA-256 is `09c815c4…a563`.
- **Capture.** An agent read the AT-SPI tree of each image on the WSLg display
  (Qt Wayland), one verified sidebar click per category. All seven categories
  are present in the same order in both builds.
- **Result.** Per-category visible node counts are 6, 28, 66, 334, 12, 15 and
  13 in both builds, with **zero differing nodes**. The full walks including
  hidden nodes are identical too.
  - No platform capability row is needed, and no defect was found.
  - Against BPR-0730's Windows capture, the only differences are values: Qt on
    Unix names combo boxes by their current text, and the Appearance page shows
    Light here and Dark there.

The first acceptance point is met on Linux. Linking this inventory from
BPR-0593's AC-11 ledger row is still to do. It is a BPR-0593 edit, which this
dispatch leaves to main. No person needed to look, so no attended candidate was
prepared.

The card was not moved on the lane: a move rewrites links in BPR-0593 and
BPR-0683.

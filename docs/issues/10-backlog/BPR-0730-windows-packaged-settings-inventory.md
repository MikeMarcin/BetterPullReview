---
schema: "implement-rfc.card.v1"
id: "BPR-0730"
title: "Windows packaged Settings inventory (RFC-BPR-0018 AC-11)"
status: "Backlog"
rfcs: ["RFC-BPR-0018"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"f2fb9b06-0968-41f0-895f-c2ef23c05acd"}
---

# Windows packaged Settings inventory (RFC-BPR-0018 AC-11)

The owner accepted the macOS half of RFC-BPR-0018 AC-11 on
[BPR-0683](../60-done/BPR-0683-settings-integration-and-native-acceptance.md)
in the v006 attended session (decision `f2fb9b06`, 18:38:18Z). The
criterion reads: tests-disabled packaged applications on macOS, Windows
and Linux expose the same setting inventory except for explicitly
documented platform capability rows. This card is the Windows half; it is
platform work and stays in Backlog until the Windows station takes it
([remote stations](../../development/remote-stations.md)).

## Scope

- Build the tests-disabled package on the Windows station at a head at or
  after d1ba791f, open Settings and record the inventory: all seven
  categories (General, Accounts & hosts, Appearance, Reviews & queue,
  Notifications, Agents & context, Storage & privacy) and every row,
  against the dev build's rows on the same head.
- Any missing row is a platform capability row that the page explains;
  anything else is a defect to card.
- Owned paths: the Windows packaging and the Settings inventory evidence
  under `docs/verification`; no product change in this card.

## Acceptance

- The packaged inventory on Windows matches the dev build's, with each
  difference named as a documented capability row.
- The captures or the recorded inventory are linked from the
  [BPR-0593](BPR-0593-close-rfc-0018.md) closeout ledger's AC-11 row.
- Where a person must look at it, an attended candidate on that station.

## Owner direction, 2026-09-11

Asked when the station takes this: "After we sort all the MacOS and
platform agnostic stuff out". It stays in Backlog behind the macOS and
platform-agnostic cards of the pack (BPR-0725 through BPR-0729, BPR-0734
and the Home cards BPR-0732 and BPR-0733); the [BPR-0593](BPR-0593-close-rfc-0018.md)
closeout records the order.

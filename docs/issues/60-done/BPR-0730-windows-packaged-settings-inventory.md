---
schema: "implement-rfc.card.v1"
id: "BPR-0730"
title: "Windows packaged Settings inventory (RFC-BPR-0018 AC-11)"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"f2fb9b06-0968-41f0-895f-c2ef23c05acd"}
acceptance: [{"criterion":"AC-11 Windows: the packaged inventory matches the dev build's, each difference a documented capability row","outcome":"pass","revision":"e93d9215045f06063b7cbdcbeadd61a25bda3e76","evidence":{"kind":"artifact","detail":"docs/verification/BPR-0730-windows-settings-inventory.md with normalized UI Automation captures windows-app-release.json and windows-dev.json. windows-app-release (BUILD_TESTING=OFF) sha256 46619a531e8ffd51d6cd25b3c3431f7d4e15e43cc728b3986d781bc9919eca51 and windows-dev Debug sha256 0be2793135918e59f691604366c1d99eaf9caa8f634a493cd1d6854948cdb7ce, each on an empty default root. Seven categories (General, Accounts & hosts, Reviews & queue, Appearance, Notifications, Agents & context, Storage & privacy), each page verified by clicking its sidebar row. Node counts 6/28/66/334/12/15/13 in both, 0 differing nodes by control type, accessible name, enabled and default toggle state. No capability row to document, no defect. Default state only; collapsed disclosures absent in both."}},{"criterion":"The captures or recorded inventory are linked from the BPR-0593 closeout ledger's AC-11 row","outcome":"pass","revision":"4b56435e3148fff8124fe80e369c55e9589ef6ac","evidence":{"kind":"observation","detail":"Integrator-owned ledger edit on main: commit 4b56435e 'Link BPR-0730's Windows packaged Settings inventory from the AC-11 ledger row', reachable from windows-lane after the 327c9a05 fast-forward."}},{"criterion":"Where a person must look at it, an attended candidate on that station","outcome":"pass","revision":"e93d9215045f06063b7cbdcbeadd61a25bda3e76","evidence":{"kind":"observation","detail":"Not required: the criterion is an inventory match, settled objectively by the agent-captured UI Automation tree with zero differences; no human judgment was needed, so no attended candidate was prepared."}}]
---

# Windows packaged Settings inventory (RFC-BPR-0018 AC-11)

The owner accepted the macOS half of RFC-BPR-0018 AC-11 on
[BPR-0683](BPR-0683-settings-integration-and-native-acceptance.md)
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
  [BPR-0593](../10-backlog/BPR-0593-close-rfc-0018.md) closeout ledger's AC-11 row.
- Where a person must look at it, an attended candidate on that station.

## Evidence

### Windows packaged inventory at `e93d9215`

Station `bpr-win` recorded the inventory at `e93d9215` for task
`task-20260917-win-evidence`. The record is
[BPR-0730 Windows packaged Settings inventory](../../verification/BPR-0730-windows-settings-inventory.md),
with its normalized captures.

**The two builds.** Both were built at the same head, which is after
`d1ba791f`:

- Packaged: `windows-app-release`, `BUILD_TESTING=OFF`, SHA-256
  `46619a531e8ffd51d6cd25b3c3431f7d4e15e43cc728b3986d781bc9919eca51`.
- Developer: `windows-dev` Debug, SHA-256
  `0be2793135918e59f691604366c1d99eaf9caa8f634a493cd1d6854948cdb7ce`.

**How it was captured.** An agent read the UI Automation tree from each build
on an empty default root. The capture clicked each sidebar category and checked
that the page shown matched it.

**Result.**

- Both builds show the seven categories in the same order: General,
  Accounts & hosts, Reviews & queue, Appearance, Notifications,
  Agents & context, Storage & privacy.
- Every exposed row is the same in both, compared by control type, accessible
  name, enabled state and default toggle state. Node counts are 6, 28, 66, 334,
  12, 15 and 13, with **0 differing nodes**.
- So no platform capability row needs documenting, and there is no defect.

**The ledger link.** The AC-11 row in the [BPR-0593](../10-backlog/BPR-0593-close-rfc-0018.md)
ledger links this inventory. The integrator added that link in main commit
`4b56435e`.

**Attended review.** None was needed, because the criterion is an inventory
match that the agent-captured tree settles.

**Limitations.**

- The rows are those of the default state, not signed in.
- Collapsed disclosures are absent from the tree in both builds.
- A visual check would be a separate attended candidate.

## Owner direction, 2026-09-11

Asked when the station takes this: "After we sort all the MacOS and
platform agnostic stuff out". It stays in Backlog behind the macOS and
platform-agnostic cards of the pack (BPR-0725 through BPR-0729, BPR-0734
and the Home cards BPR-0732 and BPR-0733); the [BPR-0593](../10-backlog/BPR-0593-close-rfc-0018.md)
closeout records the order.

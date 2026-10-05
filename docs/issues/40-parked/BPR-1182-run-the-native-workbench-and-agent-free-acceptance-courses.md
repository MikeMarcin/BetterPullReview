---
schema: "implement-rfc.card.v1"
id: "BPR-1182"
title: "Run the native workbench and agent-free acceptance courses"
status: "Parked"
rfcs: ["RFC-BPR-0055", "RFC-BPR-0060", "RFC-BPR-0061"]
depends_on: ["BPR-1166", "BPR-1171", "BPR-1177", "BPR-1181"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "qualification", "acceptance": ["RC55-07", "RC60-02", "RC60-10", "RC61-09"], "pack_alias": "PACK-CYCLE-40", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC55-07","outcome":"pass","revision":"7a25821b1","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1182's share: the owner accepted the editor gestures, IME included, as BPR-1156-editor-gestures-v004 (event 790) and BPR-1157's later keys as native-keyboard-v002; qt_suggestion_editor_1_0/_2_0 pass at this revision (build/dev, macOS arm64). Native cocoa runs of the editor rows stopped at their first click because macOS kept the keyboard with the person's application; recorded, not a product failure."}},{"criterion":"RC60-02","outcome":"pending","revision":"7a25821b1","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Native cocoa two-round course in the agent-free graph passes (F1 2,675 bytes/8 items, F2 1,530/3, no child, no canary); agent-free graph 13/13; installed full, agent-free and automation-off packages pass RELEASE_PACKAGE_OK and installed_cycle_package_course.py natively (cycle reads back byte for byte across swaps). The installed GUI round trip waits on the owner's BPR-1182-agent-free-loop-v001 (session cbd48b91, ready at event 946)."}},{"criterion":"RC60-10","outcome":"pass","revision":"7a25821b1","evidence":{"kind":"freeform","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"BPR-1182's share for macOS: the three tests-disabled packages audited (verify_release_package, verify_composed_hosts with family flags) and started natively from outside: no child process, no network socket, no PATH canary, activation endpoint bound, owner command endpoint exactly with local automation (from the process's bound sockets), variant-over-variant replacement keeping the cycle; negative controls fail. Every result names its producer; nothing cross-compiled. Linux and Windows are station requests."}},{"criterion":"RC61-09","outcome":"pending","revision":"7a25821b1","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Fixed and checked: the cycle bar keeps every control at 900 px (sweep 1,600 to 900, cocoa 875 px widest, offscreen 889 after two yields; negative control folds at 963), the Original line drops a CRLF line's CR, the Send box's words keep their height (fails 21 of 45 px without the fix). The two looks wait on BPR-1182-cycle-bar-floor-v001 and BPR-1182-send-box-v001."}},{"criterion":"BPR-1182:boundary","outcome":"pass","revision":"7a25821b1","evidence":{"kind":"command","detail":"build/dev 11/11 then 7/7 after the Send fix (view, editor, desktop, send, publish, workbench, round-trip pin unchanged, architecture_boundaries); agent-free build/dev-tour-cli 13/13 including test_network_isolation_manifest; clang-format clean; clang-tidy only a QStringLiteral branch-clone report on a changed line."}}]
---

Pack alias: **PACK-CYCLE-40** (live `BPR-1182`, allocated at import).

# BPR-1182: Run the native workbench and agent-free acceptance courses

## Outcome and bounded work

Exercise actual Windows/macOS/Linux builds on the supported native environments; compare against 0052 chrome, keyboard/focus/IME/tab behavior and no-agent full loop. Obtain the required bounded owner look through existing attended-review, preserving unanswered gates.

## Acceptance and direct evidence

Native screenshots/measurements and gestures with build SHA, width floors and exact editor operations; no cross-compile-as-native claims. Record human decisions verbatim through the established ledger.

Criteria: RC55-07, RC60-02, RC60-10, RC61-09.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Input from BPR-1166 (2026-10-03): Share notes at the width floor

At the workbench window's 900-pixel minimum width, once a local cycle has a
second capture, Capture revision gains its menu arrow and the review bar folds
Share notes behind its » overflow. It is still reachable there, but the round's
primary action is no longer on the bar. [[BPR-1166]]'s course first met it on
the offscreen platform, whose 800-pixel screen holds the restored window at its
minimum; at the default 1280 pixels everything fits. Whether that is acceptable
at the floor is this card's width-floor judgment (RC61-09), not a change made
by BPR-1166.

## Result (2026-10-04): unattended work done; three owner looks pending

Promoted and claimed (landed as `b7b2b9bdb` and `d7678c9de`), verified as
`a6e2229f9` and `7a25821b1`, landed as `3ac3d0f1f` and `f57c828ac` after a
rebase that changed no path under `src/`. The full record, with commands and hashes, is the ledger's
"BPR-1182" section in
[RFC-BPR-0053-0061.md](../../verification/RFC-BPR-0053-0061.md).

- **Fixed at the source.** The cycle's review bar folded Share notes and three
  more controls at the 900-pixel floor (967 pixels wanted on cocoa, 1,055
  offscreen). It now takes the pull request bar's forms (one button naming the
  view, the refresh glyph, the private-progress tick) and RFC-BPR-0052's
  yields; its widest state needs 875 pixels on cocoa. BPR-1255's doubled-font
  question is untouched. The Original line drops a CRLF line's CR. The Send
  box's words, cut to 21 of 45 pixels, keep their height.
- **Native macOS.** The complete two-round course passes on cocoa in an
  agent-free graph; the send course passes on cocoa. The full, agent-free and
  automation-off packages pass their audits and a native installed course:
  no child, no network socket, no canary, the owner command endpoint exactly
  with automation, and a cycle that reads back byte for byte across the swaps.
- **Owner session** `cbd48b91…` (items ready at event 946), frozen root
  `/Users/jaeger/bpr1182-native-review-v001.u0WnaM`:
  `BPR-1182-cycle-bar-floor-v001` (RC61-09), `BPR-1182-send-box-v001`
  (RC61-09), `BPR-1182-agent-free-loop-v001` (RC60-02).
- **Stations.** Linux and Windows commands are in the ledger section.

Checkpoint: candidate `7a25821b1`; next action is to read the three verdicts,
record them verbatim, read the agent-free profile's store after the loop
(`home-agent-free/` in the frozen root) for its objective record, and finish
the card or revise the rejected candidate.

**Note: Linux station run at `bf8da83c7` (holds `bf8da83c7`; station
`bpr-main-linux`, Ubuntu 26.04 under WSL2, GCC 15.2.0, offscreen; no remote
mutation, no model).**

1. `dev`: 6 of 6 passed: `qt_review_cycle_view_1_0` and `_2_0`, which sweep
   the cycle bar from 1,600 to 900 px; `application_review_cycle_desktop_public_path`;
   `application_review_cycle_send_public_path`;
   `application_review_round_trip_composition_contract`; and
   `architecture_boundaries`. No control folded at Linux metrics, so no
   `bar … hint …` line was printed.
2. `build/bpr1182-free` (`BPR_BUILD_TOUR_CLI=ON`,
   `BPR_BUILD_AGENT_INTEGRATIONS=OFF`) built with no warnings, and 5 of 5
   passed: `application_review_cycle_two_round_public_path`,
   `application_review_cycle_parity_public_path` (36.7 s),
   `cycle_cli_public_path`, `composed_application_hosts` and
   `architecture_boundaries`.
3. Not run: the native `QT_QPA_PLATFORM=xcb` two-round course. This station's
   WSLg desktop is shared and cleared only by the owner, and the integrator
   said to skip it. It is owed until the owner clears the desktop.

The DEB is owed by an Ubuntu 24.04 station; this one is 26.04.

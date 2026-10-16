---
schema: "implement-rfc.card.v1"
id: "BPR-1528"
title: "Give Home's My pull requests section its own header and rail colour"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"Can we add a header and rail color for my reviews on the home","governing_rfc":"RFC-BPR-0066","related_cards":["BPR-1388","BPR-0776","BPR-0783"],"reservation":"docs/issues/_reservations/BPR-1528--BPR-1528--cdb07d8484c2.json","requested_skill":"frontend-design"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4773c9ceac11259944168313df0daa9e57e2a0cd","evidence":{"kind":"command","detail":"qt_home_list_scale_{100,125,200}: test_my_pull_requests_has_its_own_header_and_rail probes the plum header tint and rail pixels and the Repository-grouped band tone; negative control with the neutral mapping fails."}},{"criterion":"AC2","outcome":"pass","revision":"4773c9ceac11259944168313df0daa9e57e2a0cd","evidence":{"kind":"command","detail":"qt_appearance_profile: four distinct Home tones per variant, plum at 5.65/5.27 Light, 7.41/6.40 Dark, 10.84 HC Light, 10.53 HC Dark on base/surface; profile tone follows resolved variant."}},{"criterion":"AC3","outcome":"pass","revision":"4773c9ceac11259944168313df0daa9e57e2a0cd","evidence":{"kind":"observation","detail":"Authored row keeps its amber state; application_personal_discovery_public_path and application_home_waiting_public_path pass; composed renders in all four modes change only the My pull requests panel."}},{"criterion":"AC4","outcome":"pending","revision":"4773c9ceac11259944168313df0daa9e57e2a0cd","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in fresh session c752a6404866ca55ac7600c163aec391, candidate BPR-1528-mine-colour-v001, fingerprint c64bd48ed0f200e40b5de4a8233f8139100d2f4707016d46b4cb537a9ccfb141, cursor 3; frozen root bpr1528-mine-colour-v001.7cmsoidr."}}]
---

# BPR-1528: Give Home's My pull requests section its own header and rail colour

The owner asked on 2026-10-15: "Can we add a header and rail color for my
reviews on the home". "My reviews" is Home's **My pull requests** section: the
owner's own words when RFC-BPR-0066 created it were "the reviews where i am
the author". It is the only state section on Home with a neutral header and
no rail. Needs me (amber), Waiting on others (blue) and Following (green)
each draw a tinted header and a rail in their state's colour.

Governing design: [RFC-BPR-0066](../../rfcs/RFC-BPR-0066-home-authored-pull-requests.md),
which chose the neutral header "because its rows can have different states".
This card is the owner's amendment of that one choice, recorded on the RFC and
in the [Home specification](../../ux/workbench-specification.md#home). Row
anatomy, row state colours, section membership, counts, collapse and order are
unchanged.

Home's section colours are built-in per appearance variant and are deliberately
neither stored nor customisable (`builtin_attention_color`, [[BPR-0776]]); the
Appearance page exposes none of them. The new colour follows the same pattern:
a fourth built-in Home tone per variant, with no new setting or storage.

## Acceptance

- AC1: With State headers on, the My pull requests panel draws a tinted
  header and a rail in its own colour, at the same strengths as the other
  state panels; inside an Organization, Repository or Account group its band
  carries the same rail. Name, count, collapsed summary and keyboard behaviour
  are unchanged.
- AC2: The colour is a fourth built-in Home tone for each of Light, Dark,
  High Contrast Light and High Contrast Dark. It differs from the other three
  Home tones and reads as small text at 4.5:1 or better on its variant's base
  and surface, like them. It follows the resolved appearance variant exactly
  as the other tones do.
- AC3: Row reason words and marks keep their existing state colours; Group
  None with State headers off, the other sections and Recently viewed are
  unchanged.
- AC4 (human): The owner accepts the exact frozen candidate's Home, shown
  before and after in offscreen Light and Dark renders.

## Implementation

Load frontend-design:frontend-design before choosing the colour. Qt presents:
the change is the Qt adapter's built-in tone table and the panel's tone for
the My pull requests group; no core, host, provider or storage change. Never
drive or capture the owner's desktop; renders are offscreen. Visual judgement
goes through a fresh short attended-review session and a frozen build.

## Checkpoint — candidate v001, 2026-10-15

Product candidate `4773c9ceac11259944168313df0daa9e57e2a0cd` on branch
`bpr-1528-mine-colour` (worktree
`/Users/jaeger/.codex/worktrees/bpr1528-mine-colour/BetterPullReview`),
baseline `46077aaa1`. Not merged to main until the owner accepts AC4.
Exact paths, commands and identities are in
[the checkpoint](../_checkpoints/BPR-1528/handoff.json).

Design (frontend-design loaded first): `attention_tone::my_pull_requests`,
a fourth built-in Home tone, plum, so the section reads as a sibling of
amber, blue and green without echoing the defect red or Waiting's blue
(about 65 degrees of hue apart). Light `#9A3FB0`, Dark `#CF86E8`, High
Contrast Light `#6B0080`, High Contrast Dark `#E59CFF`; on base/surface
5.65/5.27, 7.41/6.40, 10.84 and 10.53. The panel draws it at the
existing state strengths (header 0.18, rule 0.35, rail 0.6), and a
group's band takes the same rail. No Appearance setting: Home tones are
deliberately built-in and unstored ([[BPR-0776]]), so nothing persists.

| Acceptance | Evidence at `4773c9cea` | Result |
|---|---|---|
| AC1 | `qt_home_list_scale_{100,125,200}`: new `test_my_pull_requests_has_its_own_header_and_rail` probes the header tint and rail pixels and the Repository-grouped band's tone; negative control with the old neutral mapping fails | Pass |
| AC2 | `qt_appearance_profile`: four tones distinct per variant, each at 4.5:1 or better on base and surface; the profile's tone is its resolved variant's | Pass |
| AC3 | The same Home test keeps the authored row's own (amber) state; `application_personal_discovery_public_path` and `application_home_waiting_public_path` pass; composed renders of all four modes show only the My pull requests panel changed | Pass |
| AC4 human | Fresh session `c752a6404866ca55ac7600c163aec391`, candidate `BPR-1528-mine-colour-v001`, fingerprint `c64bd48e…`, ready and protected at cursor 3, Light and Dark before/after composed renders and the frozen build | Pending |

Gates: clang-format 22.1.8 clean on the six touched C++ files; clang-tidy
passed on the four touched translation units; documentation gate
`DOCUMENTATION_OK` and its 67 tests pass. Frozen root
`/Users/jaeger/.codex/attended-builds/bpr1528-mine-colour-v001.7cmsoidr`;
its own `--check` reached `ATTENDED_HOME_SHOWN:rows=3:delay=0ms`; executable
SHA-256 `0e60cfd77c536ca94130bdf7c0236d0e58a60abb50afcf532ba1ac69d13b58af`.
The review server (pid 7416) is handed to the coordinator. No owner desktop
was driven or captured; all renders were offscreen.

Next: on hand-back after cursor 3, an Accept merges the branch, records AC4
and finishes the card; a Reject's note becomes candidate v002.

## Closeout — 2026-10-16

**Owner decision, 2026-10-16T01:33:31Z: Accept**, no note (decision
`5492dc69-d79d-4c0a-9def-708766971283`, event 4 of session
`c752a6404866ca55ac7600c163aec391`), on candidate
`BPR-1528-mine-colour-v001` at `4773c9cea`, fingerprint `c64bd48e…`. Event
cursor consumed: 4.

`main` at `0b37d5438` merged into the branch at `b2da33635` without
conflicts. On the merged tree (`dev`, Debug): `qt_appearance_profile`,
`qt_home_list_scale_{100,125,200}`,
`application_personal_discovery_public_path` and
`application_home_waiting_public_path` pass 6/6; clang-format 22.1.8
`--dry-run -Werror` over the six touched C++ files is clean; the
documentation gate prints `DOCUMENTATION_OK`. The review server (pid 7416)
was stopped, the candidate's protection released and the frozen root
removed.

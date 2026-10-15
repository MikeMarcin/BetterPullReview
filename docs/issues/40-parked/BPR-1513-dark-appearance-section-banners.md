---
schema: "implement-rfc.card.v1"
id: "BPR-1513"
title: "Reduce bright Appearance section banners in dark mode"
status: "Parked"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"I don't like the dark mode section banner background color in appearance, it's like white on white and a flash bang","milestone_plan":{"id":"bpr1513-dark-appearance-banners","path":"docs/rfcs/plans/bpr1513-dark-appearance-banners/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pending","revision":"59cefd86f83c9a7ed2a586a06739ac774d98dfb0","evidence":{"kind":"observation","detail":"Dark header pixels use application_surface and actual renders are inspected; owner keep/change judgement is pending in the exact frozen attended candidate.","handoff":"docs/issues/_checkpoints/BPR-1513/handoff.json"}},{"criterion":"AC2","outcome":"pass","revision":"59cefd86f83c9a7ed2a586a06739ac774d98dfb0","evidence":{"kind":"observation","detail":"All4 painted banners match application Window; title ink matches application WindowText at >=4.5:1 across6 modes. Header styling uses existing roles; no hard-coded colour.","handoff":"docs/issues/_checkpoints/BPR-1513/handoff.json"}},{"criterion":"AC3","outcome":"pass","revision":"59cefd86f83c9a7ed2a586a06739ac774d98dfb0","evidence":{"kind":"observation","detail":"Composed open Appearance switches through all6 supported modes; shared-window Settings public path also passes.","handoff":"docs/issues/_checkpoints/BPR-1513/handoff.json"}},{"criterion":"AC4","outcome":"pending","revision":"59cefd86f83c9a7ed2a586a06739ac774d98dfb0","evidence":{"kind":"observation","detail":"Actual offscreen dark/light renders inspected and switch course passes; frozen launcher check passes and fresh attended candidate is ready. Owner visual judgement remains pending.","handoff":"docs/issues/_checkpoints/BPR-1513/handoff.json"}}]
---

# BPR-1513: Reduce bright Appearance section banners in dark mode

Correct the overly bright section banner backgrounds in Appearance when using dark mode. Use a subdued dark-theme surface with legible text that fits the surrounding settings UI.

## Acceptance

- AC1: Appearance section banners in dark mode no longer present the bright white-on-white/flash effect reported by the owner.
- AC2: Banner text remains clearly legible with restrained surface contrast, using the existing theme system rather than disconnected hard-coded styling.
- AC3: Light mode and supported theme switching remain correct, including switching while Appearance is open.
- AC4: Inspect actual offscreen renders of Appearance in dark and light modes and exercise the theme-switch path; prepare the required owner visual judgement with an exact frozen candidate.

## Coordination

Load frontend-design:frontend-design before UX edits as required by AGENTS.md. Coordinate Appearance/settings paths with BPR-1512 (center ribbon width setting). Never operate or capture the owner’s live desktop. Any human judgement uses a short attended session and frozen build.

## Execution record

Engineering checkpoint, 2026-10-14.

Mandate revision `owner-2026-10-14`; source candidate
`59cefd86f83c9a7ed2a586a06739ac774d98dfb0`, based on `9c80010a0`.
Frontend-design was loaded before UX edits. BPR-1512 was notified of the
separate `workbench_window.cpp` palette and mounting hunks.

Appearance headers now use the theme's application surface (`QPalette::Window`)
with application text, preserving the existing demibold title, native frame,
separator and actions. The prior `AlternateBase` role is the independent code
current-line fill. Actual composed rendering also exposed a missing delivery:
the separate top-level Settings shell retained the native light palette after
Dark was selected. The workbench now propagates its accepted palette to its
private and mounted Settings shells, including initial mounting.

The composed profile-startup course switches the open page through all six
modes, checks the actual resolved application surface and text, measures title
contrast at least 4.5:1 and samples all four painted header backgrounds. Actual
dark and light captures were inspected: the dark headers use `#1C2026` and light
headers `#F6F7F9`; the text remains readable and the headers blend into their
settings surfaces. All six captures remain uncommitted runtime evidence under
`build/bpr1513/renders/`. This is macOS arm64, Qt 6.11.2, offscreen evidence;
it does not certify native desktop appearance integration.

Verification against the candidate's unchanged three product paths:

- `application_profile_startup_public_path`, `qt_appearance_settings_panel`,
  `qt_settings_window` and `qt_appearance_profile`: 4/4 pass.
- `application_profile_multi_window_public_path`: pass, including the shared
  Settings shell moving between windows and an accepted theme change.
- Selected-file clang-format 22 check, clang-tidy 22 with warnings as errors,
  `git diff --check` and documentation gate: pass; documentation tests: 67 pass.
- Negative controls fail at the new banner assertion before the role change
  and at the strengthened selected-palette assertion before propagation.
  An invocation before its target build completed is superseded by the final
  completed-build run. Logs remain under `build/bpr1513/`.

AC2 and AC3 pass unattended. AC1's owner visual judgement and AC4's human
portion remain pending. The exact review session and candidate protections are
recorded in [the durable handoff](../_checkpoints/BPR-1513/handoff.json).

Documentation impact: the existing appearance allocation and Settings guide
already require immediate selected-theme projection. This repairs that
projection and changes presentation role selection only; no core, host,
persistence, provider, security, typography, layout or command contract changes.
No RFC acceptance or historical evidence is rewritten.

Review reconciliation, 2026-10-14.

The owner rejected candidate `BPR-1513-dark-banners-v001` (session
`5983d1b8…`, event 5, handed back at event 6) and left no note, so no
adjustment was requested. The system appearance is Dark, which rules out
light native controls under the dark profile palette. An offscreen render with
the macOS style shows the same subdued headers, but cannot draw native controls.
Rather than guess at v002, a fresh short session asks one named-alternatives
question about what to change. AC1 and AC4 stay pending, and the v001 source and
frozen build stay protected until v002 supersedes them. Details are in
[the handoff](../_checkpoints/BPR-1513/handoff.json).

Engineering checkpoint v002, 2026-10-14.

The owner asked for a better v002 straight away: the banners should form a
distinct band without a bright flash, with titles that read clearly, and the
same treatment in the Settings window. Frontend-design was loaded before the
edit. Each Appearance section header, and the Keep/Cancel footer that appears
while colours are edited, is now painted with the application surface moved 8%
toward the application text. The band is computed at paint time from the live
palette, so custom and high-contrast themes and live switching follow it,
without a style sheet, a second palette or a hard-coded colour. Dark is
`#2C3036` on `#1C2026` (1.23:1 band, 11.2:1 titles); Light is `#E5E6E8` on
`#F6F7F9` (1.17:1, 12.7:1); both high-contrast themes keep titles above 17:1.

Candidate `6b1d9e2af` on branch `bpr1513-dark-appearance-banners`, which has
main merged in. The five focused Appearance, theme-switching and Settings
courses pass, as do the three scaled Settings courses. Format, tidy and
`git diff --check` pass. The composed course now asserts the painted band, its
separation and its title contrast in six modes; with the band step at zero
(v001's flat surface) it fails at that assertion. The frozen v002 build passed
its launcher check and is under review in a fresh short session. The
reason session that had gone unanswered is closed, and the v001 protection is
released. AC1 and AC4 remain pending the owner's verdict. Details are in
[the handoff](../_checkpoints/BPR-1513/handoff.json).

Owner verdict on v002 and engineering checkpoint v003, 2026-10-14.

The owner rejected v002 in chat, relayed by the coordinator: "better but i
still feel like the original before v001 was better except the color". The
verdict is recorded in the v002 session as an observation, and that session is
closed.

v003 restores the original banner code exactly: the filled header strip and
colour-preview footer, frame, hairline, insets, demibold title and Reset
buttons. It changes only their palette role, from the code current-line fill
(`AlternateBase`) to the profile's raised surface (`Light`). The original had
no gradient or other emphasis to restore. As the owner saw it, the original
was a raised strip (`#F7F7F7` on `#EFEFEF`, 1.07:1), because Settings kept the
platform palette, and that same behaviour was the flash in Dark. v003 keeps the
raised strip: Light is `#FFFFFF` on `#F6F7F9` (1.07:1, titles 15.8:1), and Dark
is `#2A3039` on `#1C2026` (1.23:1, titles 11.3:1). The high-contrast themes
have pure black or white surfaces, so their strip is flat; titles stay at 21:1
and the frame and hairline remain. The Settings-window palette fix from v001
stays.

Candidate `b99680a10` on branch `bpr1513-dark-appearance-banners`. Eight
focused Appearance, theme-switching and Settings courses pass, including the
scaled Settings window. Format, tidy and `git diff --check` pass. With the
original role, the composed course fails. The frozen v003 build passed its
launcher check and is under review in a fresh short session, which shows the
original in Light, the original in Dark, v002 in Dark, and v003 in Dark and
Light. AC1 and AC4 remain pending. Details are in
[the handoff](../_checkpoints/BPR-1513/handoff.json).

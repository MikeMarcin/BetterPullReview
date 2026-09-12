# RFC-BPR-0028, RFC-BPR-0006 and RFC-BPR-0003 attended session, v009

Status: **prepared; four decisions pending; no host running**.

Four candidates, down from six. Two of v008's were the same question asked
twice and are now one; one is not judgeable until a body of work lands and is
not asked; and one of the four is not a judgement at all but a log this machine
cannot take without a hand on the mouse.

- Bundle: `build/app-release/src/app/BetterPullReview.app`, executable sha256
  `c2381f325093200563544ce3bb9e1ba04f847102e2f5b599ca6cc51c6f555653` as
  recorded in [session-identity.json](session-identity.json), tests disabled,
  signed with the local identity the preset carries, built from the source at
  `aff9fc64` (`cmake --build --preset app-release --target
  app_better_pull_review`, log `build/logs/app-release-aff9fc64.log`,
  `codesign --verify --deep --strict` exit 0).
- **No store moves schema in this build.** v008 moved the appearance store to
  version 5; this one adds no role and changes no column, so there is no second
  palette reset and an older build still reads the store.
- What changed in the bundle since v008, all of it
  [BPR-0744](../../../issues/40-parked/BPR-0744-preprocessor-directives-are-grey-but-their-arguments-take-their-own-roles.md):
  a preprocessor directive keyword stays grey, an ALL_CAPS word inside a
  directive takes the macro purple, a predefined macro like `__GNUC__` takes it
  too, and an `#include` path takes the string salmon instead of the directive
  grey. No colour value moved.
  [BPR-0745](../../../issues/40-parked/BPR-0745-the-one-pane-read-state-is-redrawn-over-the-link-map.md)
  is a design page, not a bundle change.
- Pull request: [NVIDIA/stdexec#1846](https://github.com/NVIDIA/stdexec/pull/1846),
  already in this Mac's catalogue from v008.
- Design page:
  [direction v7, state 2b](../../../design/RFC-BPR-0028-workbench-direction-v7.html#read-one-pane).
- Review page: [offline-package/index.html](offline-package/index.html), or the
  live URL once `attended_review.py serve` is started.
- Session: `cbd48b91000b00f1e72e1ad62c7c384c`, candidate revision
  `app-release-c2381f32-2026-09-12.5`, four candidates ready and protected at
  sequence 357. The v006, v007 and v008 verdicts stay recorded against their
  own revisions.
- Plan: `docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json`, milestone
  `attended-preparation-pack-head`.

## Starting the session

Not started. The host and the watch are armed only on the owner's say-so:

    python3 .claude/skills/attended-review/scripts/attended_review.py \
        --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview serve

then, from the coordinator, `wait --after 357` as a persistent background
watch. Or open the offline page above and import its export afterwards with
`attended_review.py import --file FILE`.

## Before anything else

**Quit the BetterPullReview that is running.** It has been up since 21:18 and
it is the v008 build. It also holds the profile, which is why one composition
contract cannot pass on this machine while it runs and why the new bundle's own
self-test reports `PROFILE_ACTIVATION_QUEUED` rather than starting.

## What each candidate asks

**BPR-0744, the directive ranges.** One candidate carrying the criterion
[BPR-0720](../../../issues/40-parked/BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md)
and
[BPR-0738](../../../issues/40-parked/BPR-0738-macros-and-functions-take-the-visual-assist-colours-on-the-dark-palette.md)
have both carried since v005. Asking it as two candidates is what produced "As
I answered in another question" in v008, so it is asked once. Open a stdexec
header on the dark palette and judge the directive lines. Two questions are
answered and are **not** repeated: the aliases' italic does not matter, and
`CE8AD4` stands as the macro purple. One new question rides the candidate: a
macro is recognised by ALL_CAPS, so
`#if defined(__GNUC__) && !defined(__clang__) && __cplusplus >= 202002L` paints
`__GNUC__` purple and `__clang__` and `__cplusplus` grey. Inside `#if`,
`#ifdef`, `#ifndef`, `#elif`, `#undef` and `defined()` every identifier is a
macro by the language's own rules, so all of them could be purple. That change
was costed on BPR-0744 and deliberately not taken.

**BPR-0734, Read in one pane.** A design page; nothing to install. State 2b is
redrawn over the link map as the v008 note asked: the gutter joins each
change's struck and added rows at a spine, and a change with an empty side
draws one ribbon rather than two. The other two answers are applied —
unified-diff order, and the choice in the View menu. Two consequences to refuse
if they are wrong: "like line numbers" makes it per window with no modality of
its own, so **Review takes one pane too**; and the whole-file overview went
with the change map, because the two-pane state has never had one.

**BPR-0739, the seam's hover highlight.** Shown, not described again. The
candidate carries the owner's own v008 screenshot cropped to the seam, so the
element has a picture: the blue vertical rule between the code pane's scroll
bar and the conversations drawer, lit because his pointer was on it. "I'm not
sure what this is, but leaning towards keep it" is a leaning, and nothing was
changed on the strength of one. Keep or drop.

**BPR-0746, one log.** Not a judgement, and deliberately **not** a re-ask of
the cursor. Nothing about cursor delivery changed since v008, so asking whether
it is fixed would be asking the owner to confirm a failure already recorded.
What is asked is one run from a terminal with a logging line, because telling
which of three causes makes macOS drop the cursor needs a real pointer moving
on the seam, and synthesising that motion is exactly what made the last probe
worthless: it recorded the pointer 1164 pixels from the handle and reported
success anyway. The hover question and this probe are the same trip to the same
seam.

## Not in this packet, and why

**[BPR-0646](../../../issues/40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md),
the epoch handles.** The owner's answer retired the question rather than
answering it: "An Epoch is an update to the PR, epochs identity should be
stable and repeatable if I look at the PR on different machines. A purely
observed epoch is not useful and terribly confusing." The criterion is right as
written and is not reworded, but the product still mints one epoch per
observation, so a fourth round on any bundle buildable today would fail for the
same reason as the previous three.
[BPR-0743](../../../issues/20-ready/BPR-0743-the-epoch-line-shows-only-the-updates-this-install-has-observed.md)
carries the work; the criterion comes back when the epoch line carries the pull
request's own updates.

## What could not be checked here, stated plainly

The bundle was not driven to a comparison before the candidates were marked
ready, because the running v008 build holds the profile and the new image
cannot start while it does. What *is* verified at the exact source the bundle
was built from: the classifier's own public entry over a real header (Qt's
`qsystemdetection.h`, span total unchanged at 865, `syntax_macro` 62 to 196,
`syntax_import` 3 to 0, `syntax_string` 0 to 3, every other role count
byte-identical), and 25 of 26 focused selectors including
`qt_composed_github_scale_100`, `qt_workbench_window_scale_{100,125,200}`,
`qt_settings_window*`, `syntax_classification_contract` and
`application_review_round_trip_composition_contract`. The twenty-sixth is
`application_composition_contract`, red for the same profile reason at HEAD and
here.

## Nothing here writes to GitHub

The pull request is read signed out. An anonymous read counts against GitHub's
sixty requests an hour from this machine.

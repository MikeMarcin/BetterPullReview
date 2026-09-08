---
schema: "implement-rfc.card.v1"
id: "BPR-0674"
title: "Overview is starved by the drawers"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
parked_on: "human-review"
acceptance: [{"criterion":"Choosing Overview collapses the three drawers to rails and the overview fills the centre","outcome":"pass","revision":"6213c1e0","evidence":{"kind":"observation","detail":"The presets returned early whenever the core refused a drawer command, which was whenever no comparison had resolved, so no preset ever ran in the owner's state. They now run regardless, Overview asks for all three drawers collapsed, and every modality names the repositories drawer that no preset had mentioned. qt_workbench_window_scale_{100,125,200} pin both; leaving files open in Overview, or never asking about repositories, each fail a named assertion. Eighty-five Qt, core, composition, overview and behaviour selectors pass."}},{"criterion":"No heading on the overview is elided at the window sizes the workbench supports","outcome":"pending","revision":"6213c1e0","evidence":{"kind":"freeform","detail":"Not closed here. A width floor on the overview page would become a floor on the whole centre, because a hidden page of a stacked widget sets the stack's minimum. With every drawer collapsed the page has the centre, which is what the owner asked for; whether the headings still elide at a narrow window is for them to see, and the floor would then belong to the stack rather than the page. Their other question, whether Overview should refuse a drawer opened by hand, is recorded on the card unanswered."}}]
---

# Overview is starved by the drawers

The owner, 2026-09-07: "Overview mode should have all drawers collapsed by
default, or maybe always. Those drawers don't really make sense in overview
imo."

Their capture shows why. In Overview at 1280 wide, the repositories drawer
(empty), the files drawer and the conversations drawer take most of the
window and the overview itself is a column about 150 pixels wide. Its own
headings are truncated to "Techni…etails" and "Compari…hole PR", the
since-my-last-review list clips at "My unresolved threa", and the
description and file table are unreadable. The page RFC-BPR-0028 gives the
whole centre to is the narrowest thing on screen.

The modality presets already decide drawer state per modality
([BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md),
AC-03). Overview's preset is what is wrong, not the drawers.

## Scope

- Overview collapses every drawer to its rail. Whether a reviewer can then
  open one by hand in Overview is the owner's call: "or maybe always" reads
  as a preference for always, and the card should ask before removing the
  ability.
- The overview page gets a minimum width that its own headings fit in, so
  no arrangement can squeeze it to a column again.
- Owned paths: `src/adapters/qt` workbench window modality presets and the
  overview page, their tests.

## Acceptance

- Choosing Overview collapses the three drawers to rails and the overview
  fills the centre at 1280 wide; a test pins it.
- No heading on the overview is elided at the window sizes the workbench
  supports.

## Notes (2026-09-07)

- **Why the preset never ran.** The presets were three lines of code that
  returned early whenever the core refused the drawer commands, which it
  did whenever no comparison had resolved
  ([[BPR-0675]]). The owner's Overview therefore had every drawer open,
  including the empty repositories drawer, because nothing had ever asked
  for anything else. With the drawer gate gone, a modality asks for its
  preset whether or not a comparison is ready, which is when a reviewer
  most wants the room arranged.
- **What Overview asks for now.** All three drawers collapsed to their
  rails. The repositories drawer was in no preset at all and stayed open
  across every modality, holding nothing and taking width in the owner's
  capture; every modality now names it, and outside Overview it keeps the
  state the reviewer left it in.
- **The minimum width was not added.** The centre is a stacked widget, and
  a hidden page's minimum is the stack's minimum, so a width floor on the
  overview would become a floor on the whole window. With every drawer
  collapsed in Overview the page has the centre, which is what the owner
  asked for; if it still reads badly at a narrow window, the floor belongs
  to the stack rather than to the page.
- **Evidence.** `qt_workbench_window_scale_{100,125,200}` pin that a
  modality asks for its three drawers with no comparison open and that
  Overview asks for all three collapsed, with the other three presets
  restated over three drawers. Eighty-five Qt, core, composition,
  overview and behaviour selectors pass, with `check_boundaries`, strict
  tidy, `clang-format --Werror` and `verify_documentation.py`.
- **Negative mutations.** Leaving the files drawer open in Overview fails
  "Overview asks for all three drawers collapsed"; never asking about the
  repositories drawer fails "a modality asks for its three drawers with no
  comparison open".
- **The owner's open question.** They said "all drawers collapsed by
  default, or maybe always". This is the default; a drawer opened by hand
  in Overview still opens. Whether Overview should refuse that is theirs
  to say, and the card asks it rather than deciding it.

---
id: BPR-0414
title: Anchor the root comment to the line of code it comments on
status: In Progress
rfc: RFC-BPR-0005
depends_on: [BPR-0191]
---

# Anchor the root comment to the line of code it comments on

Owner direction, recorded on the [[BPR-0413]] verdict, 2026-08-21:

> The existing drawer today needs to anchor the root comment to the line of code
> it's commenting on like code collaborator does.

This is about the drawer that ships, not about the secondary view the same
verdict chose. [[BPR-0211]] owns that one.

## What ships today

Verified from live source for this card, because the gap is smaller than "no
anchoring exists" and larger than "it just needs a signal".

- The drawer filters threads to the current **file** and stops there.
  `github_conversation_panel::set_anchor_file` keeps whole threads or none, and
  `workbench_window.cpp:1815-1826` sets that anchor when it publishes a diff.
- The line is **inert text**. `github_conversation_delegate.cpp:99,115-142`
  renders `Path:Line` into the facts line. Nothing positions anything.
- Selecting a thread drives exactly one thing: `workbench_window.cpp:695-699`
  wires `currentChanged` to `update_defect_surface()`, which enables the
  defect-classification action. It never touches the panes.
- `diff_workbench::select_line` exists (`diff_workbench.cpp:1035-1053`) and is
  the mechanism that would scroll and select. Its only caller is
  `diff_workbench.cpp:894`, reacting to in-pane interaction. **No conversation
  path calls it.**
- Nothing runs the other way either: selecting a line does not highlight or
  filter the drawer.

So [[BPR-0191]] delivered file-grained filtering and a text label against a
request for line-anchored conversation, and said so plainly at the time. What is
missing was never built rather than broken.

## The geometry already exists

This is mostly wiring, not new infrastructure.
`measured_alignment_axis::left_line_origin` / `right_line_origin`
(`measured_alignment_axis.hpp:133-136`) map a line index to document Y, and
`diff_workbench.cpp:196-197` already calls that exact pair; document Y converts
to viewport Y at `diff_workbench.cpp:1018-1020`. An anchor marker needs those
functions called with a comment's line instead of a pane's own.

## Two data gaps that make a naive anchor wrong

Both were found while scoping this and neither is mentioned on [[BPR-0191]] or
[[BPR-0212]].

**There is no `Side`.** `github_conversation_projection`
(`github_projection.hpp:124-139`) carries `Path` and `Line` and nothing that
says whether the line is on the base or the head. With two panes, an anchor has
no way to know which axis to query. A comment on a deleted line is base-side
only, and today's data cannot express that.

**`Line` silently means two different things.**
`github_read_normalizer.cpp:156-165` computes
`selected = aPreferred.isNull() ? aFallback : aPreferred` and is called as
`optional_line(thread["line"], thread["originalLine"])`. GitHub nulls `line`
once a thread goes outdated, so an outdated thread's `Line` is an
**`originalLine` in a revision the pane may not be showing**, wearing the same
field name as a live thread's current-head line. `Outdated` records that this
happened; nothing records which revision would make the number meaningful. That
is [[BPR-0212]]'s axis, and this card must not pretend to fix it.

## The RFC constrains the answer, and may contradict the request

`RFC-BPR-0005:58` is explicit:

> Comments remain in the right drawer and synchronize with line anchors; they
> never become inline full-width rows in the code view.

The same RFC's required interactions already promise, at `:65`, that "selecting
a connector, hunk, line, **or conversation** synchronizes the top panes and
lower detail pane" — so the synchronization half of this card is an existing
unimplemented RFC requirement, not new scope.

The tension is the other half. SmartBear Collaborator does put the comment
beside the code. If what the owner wants is the comment body rendered against
the line *in the code view*, that is the shape the RFC forecloses, and the RFC
would have to change rather than be quietly worked around. **Do not resolve that
by choosing a reading.** Build the part that is unambiguous and RFC-compatible,
and put the remainder back to the owner as a bounded question with something to
look at.

## Required final state

- Selecting a thread in the drawer selects and scrolls to its anchored line in
  the correct pane, through the existing `select_line` path.
- The anchored line is visibly marked while its thread is selected, in the
  gutter or margin — not as a full-width inline row.
- Selecting a line surfaces the threads anchored to it, rather than leaving the
  drawer showing every thread in the file.
- A thread whose anchor cannot be placed — outdated, or base-side with no
  `Side` to disambiguate — is visibly *not* placed, and says why. Guessing a row
  is worse than declining to draw one.
- The `Side` gap is either closed or explicitly recorded as the reason a class
  of comments cannot be anchored.
- Whether the comment body belongs in the code view goes back to the owner with
  a rendered mockup, not decided here.

## Verification

Focused selectors for the touched surfaces, plus `clang-format` on the files
changed. A rendered check is required: this is a visual claim, and a passing
signal test would not show that the marker lands on the right row.

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'workbench_window|github_conversation'
```

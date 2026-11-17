# Review report: findings you can draft from (BPR-2219)

The owner, 2026-11-16, on Review this pull request:

> that report is not acceptable. First it reads much more like an agent
> transcript than an actual report. Second we lost the most useful feature of
> the agent integration: it no longer gives the ability to draft a comment
> from the agent response. That UX needs to return into the report.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right. `png/` holds headless renders of its three frames, light and dark;
none is a screen capture. The content is the review of
MikeMarcin/BPR-Test#185, the pull request the card was reproduced on.

`frontend-design:frontend-design` was loaded first. The report's hand is not
reopened: the owner chose it on 2026-11-14
([round 1](../report-templates-v1/README.md), B, Dressed;
[round 2](../report-templates-v2/README.md)): Charter for reading, the system
sans for labels, mono for places, slate on paper by day and chalk on slate by
night, colour spent only on verdicts. This round is about what a review says
first and where its keep action stands.

## What a review report is

1. **The verdict, then the summary.** One word pair in the verdict's colour
   (Needs changes, Ready with comments, Ready to merge) over the summary,
   whose rule takes the same colour. The reviewer who reads nothing else
   knows what the agent would do.
2. **The tally**, as before: the findings by label.
3. **Findings grouped by what they need**: Must fix, Worth a look, Good. Each
   is headed with its Conventional Comments label, sits at its place in the
   code (a link that opens the file at the lines), says why in a few
   sentences, quotes the lines at fault and, where the fix is short, shows it
   as a suggestion.
4. **One keep action a finding** (RFC-BPR-0127): Draft comment where the
   finding names lines, Keep local where it names only a file or nothing.
   Once made it reads Drafted and Go to draft. Nothing is published.
5. **Keep summary** under the summary: the verdict, the summary and a line a
   finding, kept as a Local comment on the pull request.
6. **Not reviewed**, last.

What the agent said while it worked is not part of it.

## The three candidates

| | | Light | Dark |
|---|---|---|---|
| A | At the foot of each finding | [png](png/a-light.png) | [png](png/a-dark.png) |
| B | A rail of actions | [png](png/b-light.png) | [png](png/b-dark.png) |
| C | The comment, as it would be drafted | [png](png/c-light.png) | [png](png/c-dark.png) |

- **A, at the foot.** Draft comment stands under a finding's reasons, where
  Add as draft comment stood under a suggestion (look report-templates-v003,
  accepted). You read, then act. Every finding has it, not only one with a
  suggestion. The reading column keeps its width, so the page still fits a
  narrow pane.
- **B, a rail.** The actions line up in a column of their own at the right,
  and a band over the findings drafts the rest at once. The eye runs down
  the rail. It costs the reading column 146 points and needs a second layout
  when the pane is narrow.
- **C, the comment.** Each finding shows the comment its draft starts from,
  in the comments' own box, with Draft comment inside it. Nothing is a
  surprise, at the price of saying every finding twice and a page half as
  long again.

## Chosen: A

A is built. The action is where the eye already is when the finding has been
read, in the place the owner accepted for a suggestion's action; it adds no
second layout; and it is a button-shaped link of the page's own text, so Tab
and Return reach it as they reach the page's other links ([[BPR-2184]], N10).
The draft opens in the review at the finding's lines, so what C shows in the
report is seen there, where it can be edited.

B's band (draft the rest at once) is not built; the Agent tab's Draft all
remains the way to draft every finding of an answer.

## What the reproduction added

The real review (Claude Code, Sonnet, medium effort, on the pull request the
frames show) changed three things in what was built:

- Its summary and its Not reviewed section talked about the agent ("I read
  the full diff", "the shell denied the command"). The format now says the
  report is about the change, and the unchecked things are rows.
- It named two places for one finding and wrote two lines with a comma. The
  first place is the finding's; the others show beside it after "Also".
- It wrote its fixes as plain code blocks, so the old page, which offered a
  draft only under a suggestion block, offered none. Every finding now has
  its keep action, with or without a suggestion.

## The prompt is visible

By the owner's decision of 2026-11-16 the review prompt's instructions are
its body in Settings › Prompts, and the format above is shown read-only
under its editor as **Result: Findings report**, always added. Editing the
sections, the severity scheme or the verdict away does not break the page:
it shows what the report has, and every finding with a place still drafts.

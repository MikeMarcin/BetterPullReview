# What changed since my review

The reviewer reviewed this pull request before and has come back to new
commits. Say what changed since then, as the report below.

The request names two revisions: `returning.earlier_head`, the head the
reviewer last reviewed (or marked Seen, when `returning.basis` is `seen`),
and `returning.current_head`, the head now. `returning.changes` is the
change between those two revisions as a unified diff, `returning.commits`
the commits since, a merge of the base branch already one line; the pull
request's review threads are in `brief.thread.*`. Use only these. Do not
describe the pull request as a whole, and do not present anything from before
the earlier revision as new. When `returning.files.omitted` is not zero, say
in the summary that those files were left out and were not read. Do not
suggest that a conversation be resolved, and do not judge whether the pull
request is ready.

You are writing a report that a reviewer reads in BetterPullReview beside the code. Write Markdown, in exactly the shape the template that follows gives. The reviewer may read only the summary, so make it complete on its own.

Rules for every report:
- The first line is the title, as a level-one heading. Use the template's title.
- Then one summary paragraph of two to four sentences, with the answer in its first sentence. No list, heading or code in it.
- Then the template's sections as level-two headings, with the template's names, in the template's order. Leave out a section that would be empty. Add none of your own.
- An item is a level-three heading. The next line says where it is: `path:line` or `path:first-last`, in backticks, the path from the repository's root. Then why, in one to three sentences.
- A short thing is a list row: a hyphen, the place in backticks, then what to know. Where the template gives rows a label, write the label first, in bold.
- Every place in the code is written `path:line` in backticks. Never a bare file name and never a link.
- Quote code in a fenced block that names its language. Eight lines at most, and only the lines that matter.
- To propose a change, put a fenced block marked suggestion after the item's reasons, holding the lines as they should read. GitHub replaces exactly the lines the item names with the block, so the place names every line the block rewrites, first to last, a line the block keeps unchanged included: a block that rewrites lines 10 to 14 is at `path:10-14`, never at the one line at fault.
- Plain sentences. No heading below level three, no tables, no bold except where the template has it, no emoji and no closing remarks.
- Say only what the code and the conversations show. What you could not check goes in the section the template has for it.

The reviewer has reviewed this pull request before and wants to know what is different now. Group the changes by the area of the code they touch, not by commit. Name an area in a word or two. A merge of the base branch is one row, never a list of its commits. Say for each of the reviewer's conversations whether the lines under it changed.

<template>
# What changed since your review

How many commits and merges since the review, then the one or two changes that matter, then whether anything else the reviewer read has changed.

## Changed

### An area, in a word or two

- `path:line` What changed here, in a sentence.
- `path:line` The next change in this area.

```language
the changed lines, only when seeing them helps
```

## Merged main

- Merged main (N commits). Whether it touched this pull request's files.

## Your conversations

- **Lines changed** `path:line` The comment's first words.
- **Replied** `path:line` The comment's first words. Who replied, and whether the lines changed.
- **Unchanged** `path:line` The comment's first words.
</template>

# System prompt: Report: What changed since my review

- **Group in the library:** Agent tab
- **What it is for:** The template for What changed since my review.
- **Where it is used:** Sent with What changed since my review, after Report format.
- **Draft.** Not in the product. The text between the four-backtick fences is the entry's default.

````text
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
````

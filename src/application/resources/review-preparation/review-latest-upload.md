# What changed since my review

The reviewer reviewed this pull request before and has come back to new
commits. Say what changed since then.

The request names two revisions: `returning.earlier_head`, the head the
reviewer last reviewed (or marked Seen, when `returning.basis` is `seen`),
and `returning.current_head`, the head now. `returning.changes` is the
change between those two revisions as a unified diff; the pull request's
review threads are in `brief.thread.*`. Use only these. Do not describe the
pull request as a whole, and do not present anything from before the
earlier revision as new.

Answer in Markdown, in this order:

1. What the new commits changed, grouped by what each change does, with the
   file and line of each (`path:line`).
2. Which of the reviewer's own open conversations those changes touch: name
   each by file and line and say what changed at it. Say "None" when none.
3. What you could not determine from the change, and why. When
   `returning.files.omitted` is not zero, say that those files were left out
   and were not read.

Keep it short. Do not suggest that a conversation be resolved, and do not
judge whether the pull request is ready.

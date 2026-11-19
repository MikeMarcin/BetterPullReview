# Comment labels

Open each comment written for the reviewer to post with a Conventional Comments label: `label: subject`, the subject on the same line, then any discussion below it. Write the label in lowercase, with no bold.

- `praise`: something done well, said only when it is earned.
- `nitpick`: a trivial preference that need not change.
- `suggestion`: a proposed improvement; say what and why.
- `issue`: a problem the author must fix.
- `todo`: a small, necessary change.
- `question`: something you need the author to answer.
- `thought`: an idea the comment raises, not a request.
- `chore`: a process step to do before merging.
- `note`: something to know, with nothing to do.
- `typo`: a spelling or wording mistake.

The label carries the weight: `issue`, `todo`, `chore` and `typo` block the merge; `suggestion`, `nitpick`, `thought`, `note` and `praise` do not; `question` carries none. Add `(blocking)` or `(non-blocking)` after the label only when the weight differs from the label's: `suggestion (blocking): ...` for a change that must happen before merging, `issue (non-blocking): ...` for a problem that can wait. Never write `issue (blocking)` or `suggestion (non-blocking)`.

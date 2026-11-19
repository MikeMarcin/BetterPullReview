# Comment labels

Open each comment written for the reviewer to post with a Conventional Comments label: `label: subject`, the subject on the same line, then any discussion below it. Write the label in lowercase, with no bold.

- `praise`: something done well, said only when it is earned.
- `nitpick`: a trivial preference that need not change.
- `suggestion`: a proposed improvement; say what and why.
- `issue`: a problem that must be fixed before merging.
- `todo`: a small change to make.
- `question`: something you need the author to answer.
- `thought`: an idea the comment raises, not a request.
- `chore`: a process step to do, such as a changelog entry.
- `note`: something to know, with nothing to do.
- `typo`: a spelling or wording mistake.

The label carries the weight: only `issue` blocks the merge; `todo`, `chore`, `typo`, `suggestion`, `nitpick`, `thought`, `note` and `praise` do not; `question` carries none. Nothing blocks unless the pull request must not merge without it. Add `(blocking)` or `(non-blocking)` after the label only when the weight differs from the label's: `suggestion (blocking): ...` or `todo (blocking): ...` for a change that must happen before merging, `issue (non-blocking): ...` for a problem that can wait. Never write `issue (blocking)`, `todo (non-blocking)` or `suggestion (non-blocking)`.

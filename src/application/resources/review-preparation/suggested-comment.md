# Suggested comments

Write each comment you suggest for the reviewer to post (a finding's `suggestedComment`, or a reply written to be posted) as a comment the reviewer could post as it stands, addressed to the pull request's author.

- In plain words: the problem in one sentence, what goes wrong because of it, then the fix. Add a second short paragraph only when the fix needs it.
- When the request carries Comment labels (/bpr:comment-labels), open with the label it asks for, then the problem. Without it, start with the problem and no label. Never a severity such as "major:".
- Put code, identifiers, paths and values in `code spans`; put more than one line of code in a fenced block with its language. Write a proposed fix that way too. Use a fenced `suggestion` block only for a fix that is small, exact, self-contained and obviously correct, and replaces exactly the lines the comment is on; anything less certain stays a block in the file's language.
- Do not repeat the finding's claim or evidence; the reviewer has read them. Do not explain how you found it.
- Keep a real uncertainty as a question. No praise, apology, hedging or sign-off.

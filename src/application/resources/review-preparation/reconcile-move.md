Before this question, BetterPullReview moved your working directory to the pull request's new head. The note above names both heads. Your work from the earlier head, with any uncommitted edits as a work-in-progress commit, is kept on the saved branch it names, and the pre-move ref marks where it stood; files a rebase stopped on are listed there too.

Bring that work onto the new head: compare the saved branch with the new head, carry over what still applies, and resolve conflicts while keeping both sides' intent. Do not delete the saved branch or the pre-move ref, and do not push. Stop and ask if a resolution needs the author's judgment.

Then report what you did: what you carried over, what no longer applies and why, any conflicts and how you resolved them, and any checks you ran.

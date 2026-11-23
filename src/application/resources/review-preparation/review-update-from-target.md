Fetch the current target branch and the pull request's head branch from the named repositories. Start from the verified head in the agent's disposable workspace, then merge the target into it. Resolve conflicts while preserving both branches' intent. Run focused checks where practical and stop if a resolution needs the author's judgment.

Push the resulting commit to the pull request's head branch with a normal fast-forward push. If the head changed, stop and report it. If the branches are already up to date, say so. Report the result, any conflicts resolved, and the checks you ran.

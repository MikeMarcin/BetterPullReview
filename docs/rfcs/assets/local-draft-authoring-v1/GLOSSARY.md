# Glossary

Terms this pack adds. Local PR draft, candidate, successor candidate,
refresh, note and finding card keep the meanings the adopted RFCs give them.

| Term | Meaning | Not to be called |
|---|---|---|
| Author-Git runner | The second Git runner: the resolved Git executable run in a local draft's worktree with the author's own home, configuration, hooks, credential helpers and signing. | The installed-Git runner, which stays isolated |
| Isolated runner | The existing installed-Git runner with its own home and configuration, empty hooks, no credential helper and no protocol. Unchanged by this pack. | |
| Worktree write port | The application's port that writes one named regular file beneath a local draft's worktree root. Used by Save and Revert only. | The workspace adapter, which stays read-only |
| Change run | An agent turn whose working directory is the local draft's own worktree and which may change files there. | A conversation, which reads the candidate workspace only |
| Fix run | A change run started by Fix or Ask for a change…. It changes files and makes no commit. | |
| History run | A change run that may also run the author's Git to merge the target in, squash or rebase. | A BPR Git operation: BPR performs none of these itself |
| Changes session | The one agent session per local draft and agent that holds its change runs. Bound to the worktree, so it continues across candidates. | |
| Edits this worktree | The grant a change run needs; the grant line's fourth value. | A sandbox |
| Recorded state | What BPR records before a change run: the branch and head, the other branches and tags, and the bytes of every file with uncommitted changes. | A candidate: it is not offered, attached to or published |
| The run's changes | The files that differ from the recorded state after a change run, each with Keep and Revert. | |
| Self-review | The read-only review of the author's own candidate that Review my change starts, briefed by the Self-review built-in prompt (`local-draft.self-review`). | The review skill, which is the method for someone else's pull request |
| Keep / Revert | Keep selects a changed file for the successor candidate. Revert writes back its recorded bytes. | Accept / Reject, which the tour-stop and review surfaces use for other things |

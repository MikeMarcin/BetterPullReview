# Local PR drafts from a terminal

`bpr draft open` hands a Git worktree to the same local PR draft that Home's
**Prepare Pull Request…** opens (RFC-BPR-0108). An authoring harness runs it
from the worktree it changed; nothing is pushed, published or committed, and
no agent is started.

```sh
bpr draft open [PATH] [--context TEXT] [--profile NAME] [--data-root PATH] [--json]
bpr draft capabilities
```

`PATH` defaults to the current folder and is made absolute; BPR never uses a
last-used folder. The draft is found by the worktree's Git directories and
branch, never by path alone, so a second run resumes the same draft and a
branch switch, another worktree or a reused path does not. `--context` adds
its text (at most 16 KiB) to the draft as a note from the harness.

While the desktop owns the profile, the command goes to it over the same-user
command channel used by `bpr tour` and `bpr cycle`, and the desktop shows the
draft it opened. Otherwise it runs here, on the profile's store, under the
profile's lease.

`--json` writes one `bpr.draft.cli-result.v1` document: `ok`, and in `data`
the `outcome` (`created`, `resumed`, `choose_target`, `moved`, `recovery`),
the `draft` identity, `title`, `branch`, `target`, `candidates` and `notes`,
or the `recovery` (`detached_head`, `remote_chain`, `target_unavailable`,
`no_common_base`, `several_bases`, …) or `target_question`. A refusal has a
`code` and a `message` and exits 2.

A target that the remotes do not settle (a fork and its parent, a repository
with no known default branch) and a draft whose worktree moved are settled in
the desktop.

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

## Evidence (RFC-BPR-0109)

```sh
bpr draft open [PATH] --evidence FILE [--json]
```

`--evidence` imports one evidence document into the draft it opens, as the
Evidence area's **Import…** does. The document is refused whole, with its
reason, for anything this version does not define; nothing of a refused
document is applied. With `--json`, `data.evidence` counts what was added
(`claims`, `checks`, `results`, `questions`, `bound`, `artifacts_stored`,
`artifacts_not_stored`); a refusal has the code `evidence_refused` and
`data.evidence_refusal` (`too_large`, `malformed`, `format`, `version`,
`unknown_field`, `observed`, `invalid_value`, `reference`, `bounds`,
`duplicate`, `empty`, `stale`).

BetterPullReview does not read test, benchmark or prover report formats. A
tool or companion skill translates its own report into this one document:

```json
{
  "format": "bpr.evidence-import",
  "version": 1,
  "producer": { "kind": "tool", "name": "ctest-bridge", "version": "1.0" },
  "claims": [ { "key": "c1", "kind": "bug_fix", "text": "Cancelling a search releases its worker." } ],
  "checks": [ { "key": "k1", "method": "test", "title": "search_cancel_test",
                "command": "ctest -R search_cancel", "claims": ["c1"] } ],
  "results": [ { "check": "k1", "outcome": "pass", "class": "reported",
                 "material": { "head": "<commit>", "working": [ { "path": "src/search.cpp", "object": "<git hash-object>" } ] },
                 "inputs": ["src/"], "environment": "macOS 26, Debug",
                 "artifacts": [ { "name": "Log", "path": "logs/run.txt" }, { "name": "CI", "url": "https://…" } ] } ],
  "questions": [ { "text": "Does the timer path need the same fix?", "claim": "c1" } ]
}
```

- `producer.kind` is `author`, `tool` or `agent`. `claims[].kind` is
  `bug_fix`, `preserved_behavior`, `compatibility`, `performance`,
  `formal_property` or `other`. `checks[].method` is `test`, `benchmark`,
  `fuzz`, `property_test`, `inspection`, `model_check`, `proof` or `other`; a
  `model_check` or `proof` may carry `formal` (`property`, `subject`, `tool`,
  `assumptions`, `bounds`, `correspondence`).
- `results[].class` must be `reported`. A result that says `observed` is
  refused: only a later BPR-supervised action can observe an execution.
  `outcome` is `pass`, `fail`, `inconclusive`, `not_run` or `interrupted`. A
  result names its check by `check` (a key in the document) or `check_id` (a
  check already in the draft).
- `material` is what the check ran on: the commit, and every working file
  that differed from it with its `git hash-object` identity (or `"deleted":
  true`). A result binds to a candidate only when commit and working files
  match that candidate exactly; a commit alone never binds a candidate that
  includes working files. Otherwise the result is kept, unbound.
- `inputs` are the paths (a folder ends in `/`) the result depends on. A later
  candidate that leaves them unchanged, on the same base, keeps the result;
  without inputs any later candidate asks for a recheck.
- An artifact `path` is relative to the document's folder and is read beneath
  it without following links, up to 16 MiB each and 64 MiB per import; a
  `url` is kept as a link and never fetched. A missing, linked or oversized
  file is recorded as such.
- Claims, checks and questions arrive proposed: the author accepts or rejects
  each. Results arrive reported. The document is at most 16 MiB, and the same
  document is not applied twice.

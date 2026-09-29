# Command quick reference

`bpr tour capabilities --json` is authoritative for what this binary supports. The contract is `bpr.tour.cli-result.v1`: stop on anything else. After `open`, always pass `--profile <profile>` and `--draft <draft>` from its result. Every machine call uses `--json`. IDs and revisions are decimal strings; use the returned stop IDs, not ordinals.

```sh
bpr tour capabilities --json
bpr tour open <PR-URL> --json
bpr tour status --profile <profile> --draft <draft> --json
bpr tour show problem --profile <profile> --draft <draft> --json
bpr tour show stop:<stop> --source --profile <profile> --draft <draft> --json
bpr tour edit problem --file - --expected-revision <revision> --profile <profile> --draft <draft> --json
bpr tour edit stop:<stop>:body --file - --expected-revision <revision> --profile <profile> --draft <draft> --json
bpr tour add <path>:<first>-<last> --file - --expected-revision <revision> --profile <profile> --draft <draft> --json
bpr tour move <stop> --before <other-stop> --expected-revision <revision> --profile <profile> --draft <draft> --json
bpr tour remove <stop> --expected-revision <revision> --profile <profile> --draft <draft> --json
bpr tour ready <stop> --expected-revision <revision> --profile <profile> --draft <draft> --json
bpr tour undo --expected-revision <revision> --profile <profile> --draft <draft> --json
bpr tour preview --profile <profile> --draft <draft> --json
bpr tour recover --profile <profile> --draft <draft> --json
```

Edits take UTF-8 Markdown on stdin with the expected revision you just read; a stale revision is refused, never forced. Add `--request-id <id>` to an edit you may need to retry: the same id and payload returns the recorded outcome, and after `outcome_unknown` run `bpr tour status --request-id <id> ...` before repeating anything. `remove` removes route membership, not a published thread. `undo` and `redo` change local state only.

## Assistance

```sh
bpr tour hooks show --profile <profile> --json
bpr tour assist context --hook problem --field problem --profile <profile> --draft <draft> --json
bpr tour assist context --hook problem --field problem --quote "<exact passage>" --profile <profile> --draft <draft> --json
bpr tour assist context --hook comment --field stop:<stop> --profile <profile> --draft <draft> --json
bpr tour assist complete --ticket <ticket> --result - --profile <profile> --draft <draft> --json
bpr tour assist cancel --profile <profile> --draft <draft> --json
bpr tour assist context --hook outline --sources <path>:<first>-<last> --profile <profile> --draft <draft> --json
bpr tour outline use <proposal> --indices 0,2 --profile <profile> --draft <draft> --json
```

`assist context` returns one packet: the effective hook and its instructions, the exact field and byte range, the bounded sources, what was omitted, and the `prompt`. Follow that prompt and return exactly the JSON object it describes, as `assist complete --result -` on stdin. Put no target, range, hook, approval or ready state in it: any extra field makes it invalid and spends the ticket. With no selection the whole field is the target; an empty field is drafted from context. `--quote` resolves one exact current match; `--range-bytes A:B` names half-open UTF-8 bytes. A ticket is one request: issuing another supersedes it, and any edit to the draft, a hook change or thirty minutes make it stale. `outline use` inserts the chosen suggestions as Suggested stops. `hooks show` reads the shared hooks; `hooks edit` and `hooks reset` change the author's own settings, only on their explicit request.

Do not pipe user or PR text through an interpolated command. Never use `echo yes | ...`, `--yes` or `gh api` in place of a publication path the CLI does not offer.

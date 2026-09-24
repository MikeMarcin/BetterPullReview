# Command quick reference

The installed CLI's versioned help/capabilities is authoritative for supported flags. This skill targets `bpr.tour.cli-result.v1`; stop on an incompatible result rather than guessing. Keep `--profile <returned-profile>` and `--draft <returned-draft>` explicit after `open`.

```sh
bpr tour capabilities --json
bpr tour open <PR-URL> --profile <profile> --json
bpr tour status --profile <profile> --draft <draft> --json
bpr tour show problem --profile <profile> --draft <draft> --json
bpr tour show stop:<stop> --source --profile <profile> --draft <draft> --json
```

Local edits use `edit FIELD --file - --expected-revision <observed-revision>` with UTF-8 Markdown on stdin. Use structured stdin for creation or other operations whose parameters do not fit simple scalar flags. Match exact returned stop IDs, not current ordinals. `move <stop> --before <other-stop>` changes route order; `remove` removes membership, not the published thread. Machine writes require the corresponding expected field/structure revision and returned identities.

For assistance, request `assist context --hook problem --field problem` (or solution/comment). An explicit `--range-bytes begin:end` selects a half-open UTF-8 byte span. The CLI resolves quoted selections or returns ambiguity. Use the returned context/hook with the active agent. Submit the parent-contract result through `assist complete --ticket <ticket> --input -`. Never put target/range/ready/approval directives inside that result.

`assist context --hook outline` omits a prose field. `assist complete` returns a body-free proposal. After the user's choice, use `outline use <proposal> --indices <chosen-indices>`. This creates Suggested entries, not completed comments.

`ready <stop>` records only a current explicit author completion response, with expected revision and agent-relayed origin. `undo`/`redo` modifies local state only. `preview --refresh` may refresh GitHub observations but cannot write. `recover` is read-only remotely; `publish --resume <attempt>` requires renewed explicit authorization.

Do not pipe user/PR text through an interpolated command. Use safe stdin or a private user-authorized input file, argument arrays, and the actual tool's approval mechanism. Never use `echo yes | …` or `gh api` as a substitute for an unavailable publication path.

# Tour terminal development

The `tour-cli` preset builds the separate `bpr` executable with tests disabled:

```sh
cmake --preset tour-cli
cmake --build --preset tour-cli
cmake --install build/tour-cli --component TourCLI --prefix /absolute/install/root
/absolute/install/root/bin/bpr tour capabilities
/absolute/install/root/bin/bpr tour capabilities --json
```

`BPR_BUILD_TOUR_CLI` is independent of `BPR_BUILD_QT`. Core-only remains Qt-free;
the terminal uses `QCoreApplication`, Qt Core/Network, the existing GitHub and Git
adapters, and the profile-private SQLite services. It does not initialize a
desktop window or load Qt GUI, Widgets, QPA, WebEngine, syntax, tray, or updater
services. The Git adapter retains its existing isolated process supervisor.

The macOS development install deploys Qt Core, Qt Network and its native TLS
plugin with paths relative to `bin/bpr`. Other platforms currently use their
installed Qt runtime. This development install is not a signed release package;
release dependency notices, corresponding source, native qualification and
distribution are tracked by the extension's later verification milestone.

Capability discovery reports implemented commands explicitly. Human text is the
default; `--json` writes one `bpr.tour.cli-result.v1` envelope. Unsupported options
refuse before command execution. Available commands include `connect`, `open`, `status`, `show`, `edit`, `add`,
`move`, `remove`, `ready`, and `capabilities`.
`open` accepts a full HTTPS PR URL from any directory, or a decimal PR number
with `--repo HOST/OWNER/REPO`. Multiple roots require `--root` with a returned
candidate label or a unique review URL/ID. `open --offline` uses one exact saved
target and reports stale material; it never switches accounts or signs in.

The first open creates an owner-private local default profile through the shared
initializer. Native sign-in is explicit: `bpr tour connect --host github.com
--transport native` in a terminal uses the existing device flow and vault. It
prints the verification URL without launching a browser. Unsupported native
vaults and GHES device enrollment refuse with setup instructions. Explicit
`--transport gh` opts into the existing macOS gh executable/configuration trust
boundary and selects its configured identity; it never extracts tokens.

Machine reads after open require `--profile` and the opaque `draft_id` returned
by open. Every ID and revision is a decimal string (the opaque draft handle is a
SHA-256 lookup key). The target catalog includes profile, host authority,
account, repository, PR and draft; it contains no global last-opened pointer.
Concurrent profile ownership returns `owner_busy` (exit 4). `status` pages stop
summaries using `--cursor`; `show problem|solution|stop:ID` preserves raw Markdown
in JSON and neutralizes terminal controls in human output. `show stop:ID --source` requests bounded pinned source; unavailable material is
reported explicitly while the current comment remains readable. No worktree
file is substituted. Pinned-source acceptance is completed by BPR-1058.

`--data-root` selects an absolute private application root for isolated work.
Existing non-private roots are refused, never chmodded into ownership. Local
schema 1 is migrated atomically to schema 2 without rewriting existing tour
payloads. Corrupt or unknown catalogs remain errors, not empty drafts.
`edit problem|solution|stop:ID[:title|body] --file PATH|-` imports one exact
UTF-8 field (maximum 65536 bytes), retaining line endings. Machine edits require
`--expected-revision` from the returned draft. The whole document revision is a
conservative compare-and-swap boundary: even a different field changing requires
another snapshot. Profile ownership is released while input is awaited. An
explicit empty stdin clears the selected field. Invalid UTF-8, NUL, oversized
input and stale revisions leave the draft unchanged. Unix stdin and remote
waits respond to SIGINT/SIGTERM with exit 130 after draining owned work.

`add PATH:START-END`, or `add --path PATH --lines START-END`, pins one-based
inclusive lines, up to 200. Default side is RIGHT at the draft's pinned head;
LEFT and historical `--commit` are explicit. It uses the existing epoch and Git
cache to verify the selected commit/blob/path. Optional `--file PATH|-` imports
the comment body; title defaults to the path and can be edited separately.
Source inspection can make authorized remote reads and never content writes.
`move ID --before ID|--after ID` and `remove ID` retain stable identities and
published discussions. `ready ID` records an explicit relayed current-content
decision in payload version 8; it does not invent a human visit or a publication
grant. Older payloads retain an unspecified origin. Status distinguishes
`ready` from `can_ready`. Editor configuration and durable undo follow in
BPR-1057; Walk and pinned-source acceptance follow in BPR-1058.

Desktop and terminal authoring use `application::review_tour_host::execute` with
the exact owner, draft and expected revision. Admission and durable save are
separate observations. The parent importer, source semantics and staged
publication services remain authoritative.

# Provider-neutral `bpr cycle` CLI

The commands below are **proposed product contracts**, not commands implemented by
this archive. Reconcile names with the live `bpr` command registry at intake; record
any justified remap and update the single shared skill candidate and fixtures.
Do not teach an agent invented flags against an older installed binary.

## Runtime

Extend the AI-free CLI runtime planned by RFC-BPR-0044 and the owner service in 0045.
The desktop calls shared C++ services directly, never shells out to its own CLI.
The CLI does not require the desktop installed/running, a GUI/display server, a model,
a provider executable, an account or GitHub for local operations. Qt Core/Network
may remain where already justified; no Widgets/QPA/hidden QApplication. No always-on
HTTP/TCP server, browser extension or generic shell-execution service is introduced.

## Normal human commands

```sh
bpr cycle open .
bpr cycle show RC-17
bpr cycle capture RC-17
bpr cycle note RC-17 --file src/cache.cpp --lines 40:52
bpr cycle review RC-17
bpr cycle freeze RC-17
bpr cycle export RC-17 --round F1 --format markdown --output review.md
bpr cycle round read RC-17 --round F1
bpr cycle response import RC-17 --round F1 --file response.json
bpr cycle compare RC-17 --from R0 --to R1
bpr cycle publication prepare RC-17
```

`note` uses a configured trusted editor or explicit stdin/input-file; omitted text
must not start AI. `review` shows/edits the next assignment using the same drafts.
Freeze and delivery can be composed into one interactive command, but internal
identities and effects remain separate. `export --output -` prints the same Markdown.
Existing files are not overwritten without explicit human choice or an exact
expected-file revision in noninteractive use. A shell redirection controlled by the
user is outside BPR's overwrite guarantee.

Default output is useful human-readable prose/tables, not indented JSON. Machine
clients pass `--json`. Progress/errors go to stderr, never into a JSON or Markdown
payload stream. Noninteractive mode cannot open an editor/browser, prompt forever,
read accidental stdin or treat EOF as approval. Return typed `interaction_required`
and a normal next command. No generic `--yes` authorization bypass.

## Machine retrieval: whole assignment, no hidden export

```sh
bpr cycle capabilities --json
bpr cycle round read RC-17 --round F1 --json
```

Capabilities identifies supported schemas, build capabilities, policy availability,
byte/page limits and whether response append is allowed. `round read` resolves the
explicit cycle+round to the exact prepared payload and digest. It uses the selected
workspace/profile and a locally authorized share scope; IDs are not bearer grants.
On first use, the user may expose this ready round or use permitted manual export.
There is no permission to read other drafts/history from the ability to name an ID.

CWD may find a project and an unambiguous local cycle, but never a global active tab.
A machine call with several candidates returns `target_ambiguous` with only permitted
minimal metadata. Once resolved, retain full opaque cycle/profile/round identities on
all calls. Source context requests are bounded to explicitly included material or a
separate admitted local workspace-read capability. Do not fall back to scraping SQLite,
reading private directories, pulling an entire repository, or using a new HTTP daemon.

`round read` returns a complete instruction payload when within the advertised limit.
For larger admitted rounds it returns explicit total counts, continuation and the
immutable payload digest. `round page` uses a bound cursor; the caller must retrieve
and verify every instruction/suggestion page before implementing. Source attachments
may be retrieved on demand using `round source` with exact attachment identity and
range. Complete feedback is mandatory; extra context is not a license to export
unselected private material. See [ROUNDS-AND-PORTABILITY.md](ROUNDS-AND-PORTABILITY.md).

## Command families and mutation classes

| Family | Mutation / authority |
|---|---|
| capabilities, list, show, round read/page/source, compare | bounded observations; round retrieval can record a retrieval receipt, never completion |
| open, capture, note, review, freeze, amend | local data writes under local authoring authority and expected revisions |
| export | selected disclosure to stdout/file/clipboard according to channel policy |
| response import/append | append untrusted recipient reports only; does not edit original instructions or verify |
| verify / withdraw / defer | explicit reviewer authority; unavailable to response-only agent grants |
| publication prepare/preview/recover | local plan and explicit admitted remote reads; no remote write |
| publication publish | finite confirmed branch/PR plan via the shared publication service |
| publication link | validate an externally created PR and associate it; never fabricate public identities |

The first external-agent profile is read-ready-round plus append-response, not general
cycle editing. Manual human CLI use may expose broader commands. Allow-lists are
capability checks at the host boundary, not an `allowed-tools` skill hint. Prose
payloads use stdin/private files, never shell interpolation into arguments. Return
exact revision errors instead of last-writer-wins. No review data is an executable.

## Single-owner cross-interface contract

When no desktop owns the profile, the CLI acquires the owner lease for the operation.
When the desktop owns it, forward finite bounded commands over the already admitted
same-user authenticated owner channel. Extend its versioned operation registry and
security documentation explicitly: existing activation-only IPC is not enough.
Validate profile owner/process identity, request IDs, versions and expected revisions;
reject stale endpoint substitution and wrong-user clients. Busy IME/draft flush yields
a useful retry/conflict, not omitted text or a second writer. No mandatory daemon.

On native platforms where qualified owner transport is unavailable, do not silently
start a conflicting writer. Supply a truthful capability outcome and the safe close-
desktop path until that transport is delivered. The acceptance matrix requires actual
Windows/macOS/Linux qualification rather than inheriting macOS-only evidence.

These application checks are not a sandbox against another process already authorized
to read the same OS user's files. Real access containment requires OS/harness policy.
The CLI is not allowed to claim it has certified an unknown external provider.

## Results and errors

Use `bpr.cycle.cli-result.v1`: command, operation ID, outcome, complete flag, data or
typed error. Schemas/identities are discoverable. Distinguish `unsupported_version`,
`policy_denied`, `capability_unavailable`, `target_ambiguous`, `round_not_ready`,
`source_unavailable`, `stale_revision`, `draft_busy`, `capture_unstable`, `over_limit`,
`requires_partition`, `conflict`, `interaction_required`, `cancelled` and `uncertain`.
No empty success for missing content. Preserve unknowns and absence explicitly.

## Portable skill boundary

The `bpr-cycle` skill is a small consumer of these commands. The current harness uses
its own session, model, tools and approval rules. The skill neither starts another
model, changes orchestration topology, publishes, verifies, nor installs itself.
A harness without native skill support can follow the same Markdown instructions;
a remote/browser-only harness lacking local command reachability uses file/clipboard.
Skill syntax compatibility and direct reachability require observed evidence per
harness; do not promise universal integration merely because SKILL.md is portable.

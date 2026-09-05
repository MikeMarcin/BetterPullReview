# Issue-board commands

The portable entry point is
`python3 .agents/skills/implement-rfc/scripts/issue_board.py`.
It uses this skill's `config/project.json`; global `--repo-root PATH` and
`--project-config PATH` overrides precede the subcommand. Python 3.10+ and
Git for applied moves are the only runtime requirements.

```sh
python3 .agents/skills/implement-rfc/scripts/issue_board.py validate --json
python3 .agents/skills/implement-rfc/scripts/issue_board.py queue --rfc RFC-ID --platform mac
python3 .agents/skills/implement-rfc/scripts/issue_board.py move ISSUE-ID --to LANE --dry-run
python3 .agents/skills/implement-rfc/scripts/issue_board.py move ISSUE-ID --to LANE --json
```

Use actual IDs and configured lane directory names. All commands accept
`--json`, returning a versioned result. Validation failures return nonzero and
an `errors` list containing `path`, `field`, and `reason`; successful validation
reports `valid` and `card_count`. Ordinary argument errors use argparse's exit
status 2. Human output is compact and does not print card bodies.

## Queue and moves

`queue` includes explicit RFC links and optional `--include ISSUE-ID ...` IDs.
It reports open cards by numeric ID; `--all` includes Done. The sole in-progress
card wins, otherwise the lowest-ID dependency-ready card in generic ready or
the selected platform lane wins. Multiple in-progress cards produce a null
`next` plus `ambiguous_in_progress`; no eligible work also produces a null
`next`. Platform defaults to the local Python host. Pass it explicitly for a
station. Backlog candidates remain backlog, and other-platform work reports
both its deferral and any unmet dependencies. Closeout paths can include older
terminal records: the agent reconciles the current scope with those records.

`move` validates the existing and proposed board before writing. Entering the
parked lane requires `--parked-on TOKEN`; entering blocked requires
`--blocked-by TOKEN`. A repeated move can reuse that card's existing reason.
Leaving either lane removes its reason field. Done is terminal; repeating an
already-satisfied command is a no-op. The tool never supplies acceptance
evidence, changes RFC statuses, edits dependencies, allocates IDs, or commits.
Use the returned `changed_paths` (including both ends of a move) for explicit
staging. A successful move is not an ownership claim until its commit exists.

Live Markdown inline links and reference definitions are repaired by resolved
destination, preserving fragments, query strings, and labels. Relative links
within the moved card are rebased. Code examples, wiki IDs, external URLs, and
plain historical path mentions are unchanged. A reference in configured
protected content refuses the whole move; review that reference's authority
before making any manual repair. The tool supports ordinary Markdown link
destinations, including angle brackets and percent-encoded paths; HTML links
and complex Markdown extensions are outside this command's link parser.

Applied moves serialize through `implement-rfc-board.lock` in the checkout's
Git directory. The tool rereads observed files before writing and refuses
destination collisions. It restores its applied writes after ordinary I/O
failure and reports any rollback it could not safely finish. This is not a
crash-atomic multi-file transaction and cannot exclude editors or other tools
that do not honor its lock. After a process interruption, confirm the recorded
PID has ended before removing the lock and inspect Git state before retrying.
`--dry-run` writes no files or lock. Commands do not stage or reset user edits.

## Card contract

[The card schema](../schemas/issue-card.schema.json) is evaluated by the shared
`board_model.py` implementation, used by the CLI, portable audit, and project
documentation gate. It validates decoded metadata rather than Markdown prose.
The parser supports UTF-8, LF or CRLF, one key/value per line between `---`
fences, text scalars, and inline text lists. It rejects duplicate keys, missing
fences, comments and unsupported YAML constructs. Use JSON double-quoted text
or YAML single-quoted text for values containing a colon followed by whitespace.
Plain scalars, including dates and closeout markers, are text; only inline lists
become arrays. This limited format avoids YAML's implicit type conversions.

Required fields are `id`, `title`, `status`, the configured `rfc_link_field`,
and `depends_on`. Each card links one RFC, or the explicit text `none` for
workflow maintenance. `depends_on` is an inline array of unique issue IDs.
Supported optional fields are `parked_on`, `blocked_by`, `milestone`,
`completed` (YYYY-MM-DD text), and `closeout` (`true` or `false` text). A
configured closeout front-matter marker also declares that particular text
field. Unknown fields are errors; extend the schema intentionally rather than
silently ignoring metadata. The configured RFC key replaces `rfc` in the
effective schema. Existing text and field order are retained during a move.

Board checks additionally require filename/ID agreement, unique IDs, existing
RFC references, existing dependencies, no self-dependencies or cycles,
lane/status agreement, valid reason metadata, satisfied dependencies in
nondeferring lanes, and a next-ID counter ahead of used IDs in its sequence.
The audit adds its existing historical counter and process observations;
structural errors are failures even in Done. Product evidence remains an agent
judgment, and the project structure gate still runs at closeout.

## Project configuration

The optional `bookkeeping` object becomes required to use `move`. Configure:

- `deferring_lanes`: lane directory names allowed to retain unmet dependencies.
- `parked_on`: accepted parked reasons; an empty list permits any valid token.
- `blocked_by_forbidden_prefixes`: reason prefixes that belong elsewhere.
- `link_roots`: repository-relative files or directories to scan for Markdown.
- `protected_globs`: repository-relative patterns for immutable source/evidence.
- `editable_globs`: exceptions for explicitly maintained records within those
  protected trees, such as live-import notes.

Older project configs can still validate and report queues: backlog, blocked,
and platform-ready lanes defer dependencies by default, and reason tokens must
have valid syntax. Add project-specific policy before enabling moves. The
project schema and loader validate the object together. No policy is inferred
from project names or Markdown prose.

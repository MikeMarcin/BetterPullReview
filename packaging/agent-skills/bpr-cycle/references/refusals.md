# Refusals

A refusal writes nothing to standard output except its JSON, changes nothing, and says why. Its `next`, when present, may be a reviewer's command: show it to the user to run, and never run it yourself. Each refusal below is final for this harness: report it, keep anything you already finished, and do not work around it.

| Code | Detail | Meaning | What to do |
|---|---|---|---|
| `policy_denied` | `decision` `not_granted` | No grant for this round, or a read-only grant and you tried to respond. Naming a round is not permission. | Tell the user the reviewer has to grant this round; show `next`. |
| `policy_denied` | `decision` `revoked` | The reviewer withdrew the grant. | Stop reading. What you read stays read; tell the user. |
| `policy_denied` | `decision` `channel_denied` or `channel_unconfigured` | The disclosure policy does not let this round reach a harness. | Tell the user. Never change policy yourself. |
| `policy_denied` | none | You ran a command that is not a consumer command. | It is the reviewer's; do not try another one. |
| `requires_partition` | the round's measure | The round is larger than your budget, or one item is larger than any page. | Tell the user; the reviewer splits it. Act on none of it. |
| `round_not_ready` | | The round is not frozen. | Ask for a frozen round. |
| `not_found`, `target_ambiguous` | | No such cycle or round here, or the folder finds several. | Ask for the exact `RC-n` and `Fn`. |
| `invalid_input` | | A malformed option, response or cursor; a cursor of another round, digest or page size. | Fix the form; for a cursor, read again from `round read`. |
| `conflict` | | The response names another cycle, round or digest, or reuses a `response_id` with other content. | Check the IDs against the read. |
| `unsupported_version`, `capability_unavailable`, `unsupported_command` | | This binary or the running desktop does not offer it. | Use the Markdown handoff. |
| `uncertain` | | The answer was lost (exit 6). | Read again; repeat an append with the same document. |
| `storage_unavailable`, `profile_unavailable`, `operation_failed` | | BPR could not do it. | Report it. |

If `bpr` cannot be run at all, or your harness may not run local commands, that is a reachability limit, not a refusal: ask for the round's Markdown and reply in prose by item ID.

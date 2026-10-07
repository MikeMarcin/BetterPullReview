# Recover without making it worse

| CLI result | Response |
|---|---|
| `target_ambiguous` | Present the returned choices; get the author's exact profile, PR or root. |
| `revision_conflict`, `stale` | Keep the proposed text. Read the current draft, show the difference, and follow the author's next instruction. Never force a newer revision onto an old edit. |
| `ambiguous_quote`, `quote_not_found`, `invalid_range` | Quote more of the passage, or ask which occurrence. Never widen the scope. |
| `unknown_ticket`, `expired` | The request was spent, superseded, cancelled or too old. Ask for context again if the author still wants it. |
| `invalid_result` | Your result did not follow the contract. Reread the prompt and ask again; do not add fields to satisfy the CLI. |
| `insufficient_context` | Tell the author what is missing. Ask for it; do not fill the gap with guesses. |
| `editor_busy` | The desktop editor holds uncommitted input-method text. Wait for the author to finish; do not force a commit or start another writer. |
| `owner_protocol_unsupported` | Name the CLI and desktop compatibility needed. Do not open SQLite or take the runtime lock. |
| `context_export_refused` | The host will not send draft text to an agent it cannot verify. Keep manual authoring available; do not relabel this agent as approved or read the same text another way to get around it. |
| `interaction_required` | Give the next normal human action. With the desktop open, `add`, `show --source` and `walk` read source only for a pull request open in it; other provider reads and settings changes need the profile to themselves, and publication needs the author at a terminal. No simulated approval. |
| `outcome_unknown` | Run `status --request-id <id>` to see whether the owner committed it. Do not repeat the edit. |
| uncertain or partial publication | Run read-only `recover`. Keep confirmed comment IDs; never repost comments. |
| `authentication_required` | Explain the admitted `connect` action and run it only on an explicit setup request. Never extract tokens or change global gh authentication. |
| missing CLI or incompatible schema | State the dependency and version. Do not install software or create another publisher. |

Local drafts and undo history do not exist on another machine. A pending GitHub review may expose staged text to its author but is not a complete copy of the work in progress, nor a recovered publication grant. A published tour recovers from ordinary GitHub comments and its index; private notes and provenance stay unknown.

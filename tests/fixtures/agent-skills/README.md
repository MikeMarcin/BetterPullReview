# Agent skill fixtures

[[BPR-1916]]. `home/` stands in for a person's home folder: its
`.claude/skills` and `.agents/skills` hold fixture skills that `/name` in a
prompt can reference. The attended prompt-editor look passes it with
`--attended-skill-home`, so a fixture build never reads the person's own
skills. The skills are written for these looks and say so; they are nobody's
real guidance.

Consumed by `app_better_pull_review --attended-prompt-editor` (Settings,
Built-in prompts: completion, Preview and the staged plugin of a read-only
Claude run).

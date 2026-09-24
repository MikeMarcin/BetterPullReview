# Locked extension decisions

| Decision | Consequence |
|---|---|
| The skill is named pr-tour. | SKILL.md, package directory and install metadata use this exact name. No review-tour alias is required. |
| Thin wrapper over bpr tour. | No second publisher, parser, database, credential handling or recovery code in the prompt/scripts. |
| One shared tour engine. | Desktop invokes services directly; CLI composes the same code; active agent calls the CLI. |
| Published tours canonically live on GitHub. | All available shared meaning is recoverable by any interface/machine without BPR-only metadata. |
| Normal WIP stays local. | Save/edit/switch does not create GitHub comments. Server-pending staging exists only during explicit publication. |
| Same-machine WIP handoff is included. | One profile-private draft/journal and owner forwarding; no clipboard/export/import requirement or forced desktop close for normal switching. |
| Raw terminal remains AI-free. | Configured editor, source excerpts, local commands and manual publish work without the skill or provider. |
| Active agent performs requested assistance. | Reuse effective four hooks, no nested agent/model session or new orchestration. |
| In-place, selection-scoped rewrite with ordinary undo. | No mandatory Use/Apply/Dismiss; exact range/generation checks remain application-enforced. |
| Outline only; one stop at a time. | No hidden bodies in skeletons, no batch ready, no implicit generation or publication. |
| Existing pack is already imported. | Add only new RFCs/cards; resolve live mappings rather than replace/re-import RFC0039–0043. |

Non-goals: GUI redesign, zero-Qt absolutism, full-screen TUI, generic BPR remote control, always-running daemon, cloud WIP sync, Jira retrieval, full-tour generation, new model routes or broad PR management. These are not unfinished acceptance obligations of this extension.

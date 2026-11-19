# Pull request description

Help the author write the full description of a pull request they are preparing locally, before it exists. The description is the one body the pull request will carry: write it for the reviewer who reads it first.

Say what the change does and why, then what a reviewer should know to judge it: the approach, a non-obvious decision, what is left out. Use the author's words and the material supplied: the candidate's changed files and the tour's stops. Do not invent intent, incidents, test outcomes or guarantees.

Keep the author's own structure, headings, template markers and checklist exactly where they are; change only what you were asked to. No enterprise tag, ticket or risk scale is required.

Cite code as `path:first-last` (or `path:line`), with the file's path in the candidate and one-based lines in the candidate's version, and cite only lines the reader should look at. A permalink cannot exist until the pull request's commits do, so never write a URL for code.

When the change moves existing behaviour unchanged into a branch (behind a flag, an option or a platform check), say that the existing path is kept and where the condition is; do not describe what the existing path does. Explain the new path and what selects it.

Return proposed text for the author to accept; never treat it as accepted.

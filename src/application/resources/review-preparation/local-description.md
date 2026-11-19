# Pull request description

Write the description of a pull request for its author: the Markdown body shown under its title, written for the reviewer who reads it first. The pull request may be on GitHub already or still a local draft being prepared.

Use these three parts, each under its own level-two heading:

- ## Summary: what the change does and why, in two or three sentences.
- ## What changed: the changes a reviewer should know to judge it, as a short list naming files or areas: the approach, a decision that is not obvious, what is left out.
- ## How to test: the steps or commands that show it works, and what to look for. Where the change does not say how to test it, say what to check.

Work from the material supplied: the change, the current description and, for a local draft, the tour's stops. Do not invent intent, incidents, test outcomes or guarantees.

When there is a description already, rewrite it: keep the author's own structure, headings, template markers, checklist and words where they still hold, and change only what the change calls for. No enterprise tag, ticket or risk scale is required.

Cite code as `path:first-last` (or `path:line`), with the file's path in the change and one-based lines at its head, and cite only lines the reader should look at. Never write a URL for code.

When the change moves existing behaviour unchanged into a branch (behind a flag, an option or a platform check), say that the existing path is kept and where the condition is; do not describe what the existing path does. Explain the new path and what selects it.

Answer with the description alone, ready to paste: no title, no greeting or preamble, no remarks after it, no review findings and no comment label. It is not a comment and is not posted; the author reads it, edits it and saves it.

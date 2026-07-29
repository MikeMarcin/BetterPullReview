# Documentation Instructions

This scope owns live product, architecture, UX, RFC, issue-board, security,
development, and verification documentation.

- Preserve the authority and architecture terms used by the normative RFCs.
- Keep RFC front matter and `docs/rfcs/README.md` status synchronized.
- Treat issue-card folder placement as status and keep front matter aligned.
- Keep imported files under `docs/rfcs/plans/` clearly source-only.
- Record commands and direct evidence that can fail when a claim is false.
- Do not check in local screenshots or logs as substitutes for runtime proof.
- Run `python3 scripts/verify_documentation.py --root .` after edits.

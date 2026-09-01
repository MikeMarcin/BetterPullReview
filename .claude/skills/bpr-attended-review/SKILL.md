---
name: bpr-attended-review
description: "Prepare, collect, validate, and record attended human-approval verdicts through deterministic digest-bound artifacts: offline review pages with exact per-subject decisions and batch-bound verdict export, and markdown operator packets for single bounded decisions. Use when the agent needs to run an attended review checkpoint, build a human approval page for captured image, video, or audio evidence, present visual or UI acceptance-criterion evidence for sign-off, review diff-rendering or workbench interaction states, run an A/B comparison, multiple-choice, or select-N alternative selection, write an operator review packet for a terms or dependency-acquisition decision, validate a returned verdict against its exact batch manifest, or record verdicts as repository evidence."
---

# BPR Attended Review portable entrypoint

This is a manually maintained Claude Code entrypoint; it is not owned by the
implement-rfc renderer. Read and follow
`${CLAUDE_PROJECT_DIR}/.agents/skills/bpr-attended-review/SKILL.md` as the
authoritative skill before doing any attended-review work. Resolve every
relative path in that skill from
`${CLAUDE_PROJECT_DIR}/.agents/skills/bpr-attended-review/`. Keep this
frontmatter description synchronized with the canonical skill's description.

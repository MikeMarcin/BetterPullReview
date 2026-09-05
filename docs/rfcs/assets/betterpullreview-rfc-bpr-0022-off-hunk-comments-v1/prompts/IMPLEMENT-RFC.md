# Implementation prompt

Use the repository's `$implement-rfc` skill to import and implement
RFC-BPR-0022 from this pack.

Treat `docs/rfcs/future/RFC-BPR-0022-semantic-source-line-comments-off-hunk-publication.md`
as normative and `docs/rfcs/plans/RFC-BPR-0022-issue-plan.md` as the immutable
planning snapshot. Follow the live board after import.

Key constraints:

- Create RFC-BPR-0022 as a new post-pack RFC. Do not rewrite RFC-BPR-0008.
- Preserve the existing `source_line_ref`, target/head checks, acting identity,
  designated mutation authority, one serialized coordinator, and server-truth
  refresh.
- Keep semantic source anchor separate from GitHub remote subject.
- Use a native line comment when the exact operation accepts it.
- Use the ordinary file-level review-comment plus canonical visible permalink
  only after exact unsupported evidence authorizes it.
- Never start fallback after an indeterminate native write, and never treat
  generic 422 as unsupported.
- Migrate recovery to one logical publication with deterministic native and
  fallback child attempts; prove at most one remote comment.
- Make capability evidence repository/actor/profile/operation/API/classifier
  scoped and freshness-bounded.
- Keep normal Qt UX representation-blind; no capability prompt or fallback
  button.
- Rehydrate canonical fallbacks after database loss, but never guess invalid or
  ambiguous anchors.
- Do not absorb RFC-BPR-0021's general historical revision join.
- Use fixtures for automated work and the existing attended designated-live
  path for real mutations.

At each slice, run the focused tests that directly falsify its acceptance
boundary plus one complete selected/public path when applicable. Commit each
completed live issue slice intentionally. Stop closeout until RFC-BPR-0008 is
Auto-Verified and the native/fallback live evidence is recorded.

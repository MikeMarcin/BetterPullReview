---
schema: "implement-rfc.card.v1"
id: "BPR-2182"
title: "The author-Git runner refuses only a push the core would not build, and takes paths literally"
status: "Done"
rfcs: ["RFC-BPR-0114"]
depends_on: []
priority: "P1"
platforms: ["mac","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2182--BPR-2183--8556cfbfdde3.json","origin":"critical architecture review of 03aae05cb (B1, N1)","criteria":["AC01","AC02"]}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"47a10b45f","evidence":{"kind":"command","detail":"core_local_changes (URL and branch containing force build the plain push; the only option is --no-follow-tags, no + refspec); local_changes force_words_push (a push to salesforce-enforce.git on fix/enforce-limits reaches the hosted branch; the same command with --force added or a + refspec answers malformed and launches nothing)"}},{"criterion":"AC02","outcome":"pass","revision":"47a10b45f","evidence":{"kind":"command","detail":"local_changes literal_paths (pages/[id].tsx, a*.txt, q?.txt and :top.txt stage and unstage alone beside pages/i.tsx, pages/d.tsx, ab.txt, qz.txt and top.txt, with commits and on an unborn branch)"}}]
---

# BPR-2182: The author-Git runner refuses only a push the core would not build, and takes paths literally

Part of [RFC-BPR-0114](../../rfcs/RFC-BPR-0114-commit-and-push-a-local-pr-draft-through-the-authors-own-git.md)
(AC03 push, AC04 no force). From the critical architecture review of
`03aae05cb`:

- **B1 (blocking).** `installed_author_git::push` refused any argument
  containing the substring `force`, so Push failed for a remote such as
  `git@github.com:salesforce/repo.git` and a branch such as
  `fix/enforce-limits`; `tests/core/local_changes_tests.cpp` asserted the same
  substring rule.
- **N1.** Stage and Unstage passed paths as Git pathspecs, so staging
  `pages/[id].tsx` also staged `pages/i.tsx`, and `:top.txt` staged
  `top.txt`.

## Acceptance

- **AC01:** A push runs only when its command equals what
  `core::make_push_command` rebuilds from the command's own directory, URL and
  branch. A URL and a branch that contain "force" push; the same command with
  an option added or a `+` refspec is refused as malformed before any Git
  launches.
- **AC02:** Stage and Unstage take each path as itself: `[`, `*`, `?` and a
  leading `:` are neither a pattern nor pathspec magic, on a branch with
  commits (`restore --staged`) and on an unborn one (`rm --cached`).

## Notes

`--literal-pathspecs` is passed on Stage, Unstage and every read rather than
`GIT_LITERAL_PATHSPECS=1` in `author_environment`: commit and push hooks
inherit that environment and keep the author's own pathspec rules. Nothing
the owner sees changes, so there is no look.

## Delivered

- `47a10b45f`: B1 and N1 in `installed_author_git.cpp`; `core_local_changes`
  and `local_changes` (`force_words_push`, `literal_paths`) pass.
- `3c6c9d59c`: `VerifyArchitectureBoundaries.cmake` requires the push to
  equal `core::make_push_command`'s rebuild instead of the old substring
  rule's `aArgument.starts_with('+')`; `architecture_boundaries` passes.

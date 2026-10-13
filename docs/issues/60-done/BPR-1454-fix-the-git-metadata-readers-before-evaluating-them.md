---
schema: "implement-rfc.card.v1"
id: "BPR-1454"
title: "Fix the Git metadata readers: repository format, config syntax, ignore case, POSIX classes, index checks and HEAD"
status: "Done"
rfcs: ["RFC-BPR-0054", "RFC-BPR-0083"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan": {"id": "git-metadata-readers-v1", "path": "docs/rfcs/plans/git-metadata-readers-v1/milestone-plan.json"}, "mandate_revision": "git-metadata-readers-r1", "rfc_base_sha": "9a4f61976bba23727acd77d10c23c041f87208f0", "reservation": "docs/issues/_reservations/BPR-1454--BPR-1464--0c7219d3be8c.json"}
acceptance: [{"criterion":"BPR-1454:AC1","outcome":"pass","revision":"ae60c0250","evidence":{"kind":"observation","detail":"review_cycle_git_readers rows format, reftable and outgoing-reftable (macOS, Git 2.50.1): version 2, an unknown version 1 extension (extensions.futureformat), objectFormat in a version 0 repository, and a reftable checkout are refused as workspace/unsupported with words naming the key; the rebind material check and outgoing resolution of a checkout migrated with git refs migrate refuse too; allowed extensions (preciousObjects, noop) capture. core_review_cycle_manifest covers the allowlist checked against Git's setup.c rules (v0 tolerated noop/preciousObjects/partialClone/worktreeConfig; v1 noop-v1/objectFormat/refStorage/relativeWorktrees; compatObjectFormat declined), case-sensitive values and invalid values. Each row fails at 9a4f61976 (negative control in the ledger).","path":"docs/issues/60-done/BPR-1454-fix-the-git-metadata-readers-before-evaluating-them.md"}},{"criterion":"BPR-1454:AC2","outcome":"pass","revision":"ae60c0250","evidence":{"kind":"observation","detail":"core parse_git_config implements Git's config.c grammar (BOM, sections, quoted and dotted subsections, key rules, quoting, escapes, inline # and ; comments, continuations, whitespace runs); malformed configs Git refuses are refused (core_review_cycle_manifest). Real-Git row config: SHA-256 repository with an inline comment on objectformat captures as SHA-256 (git rev-parse agrees); at base it was refused. Includes are not followed (unit test).","path":"docs/issues/60-done/BPR-1454-fix-the-git-metadata-readers-before-evaluating-them.md"}},{"criterion":"BPR-1454:AC3","outcome":"pass","revision":"ae60c0250","evidence":{"kind":"observation","detail":"Row ignore-case: core.ignoreCase written quoted and continued folds .gitignore (Secret.env ignores secret.env, confirmed by git check-ignore) and the filter attribute (*.BIN matches image.bin); config.worktree under extensions.worktreeConfig wins. Unit tests pin Git's WM_CASEFOLD quirks ([A] and \\K keep case; ranges, [:upper:] and [:lower:] fold), each checked against Git 2.50.1 by probe.","path":"docs/issues/60-done/BPR-1454-fix-the-git-metadata-readers-before-evaluating-them.md"}},{"criterion":"BPR-1454:AC4","outcome":"pass","revision":"ae60c0250","evidence":{"kind":"observation","detail":"Row posix-classes: a[[:space:]]b ignores 'a b', x[![:space:]]y keeps 'x y', [[:digit:]][[:upper:]] ignores 7Q, as Git does; unit tests cover all twelve class names, an unknown class and an unterminated bracket matching nothing, and literal members. Fails at base.","path":"docs/issues/60-done/BPR-1454-fix-the-git-metadata-readers-before-evaluating-them.md"}},{"criterion":"BPR-1454:AC5","outcome":"pass","revision":"ae60c0250","evidence":{"kind":"observation","detail":"Row index-checksum: an index with one flipped stat byte is refused with the checksum words; an index Git wrote under index.skipHash (all-zero trailer, asserted) captures. Unit test: matching SHA-1 trailer accepted, changed byte refused. Fails at base.","path":"docs/issues/60-done/BPR-1454-fix-the-git-metadata-readers-before-evaluating-them.md"}},{"criterion":"BPR-1454:AC6","outcome":"pass","revision":"ae60c0250","evidence":{"kind":"observation","detail":"Row index-extension: a real index with an appended lower-case 'abcd' extension and a recomputed trailer is refused with the required-extension words; unit tests keep TREE optional and refuse 'abcd' and '1xyz', link and sdir keep their flags. Fails at base.","path":"docs/issues/60-done/BPR-1454-fix-the-git-metadata-readers-before-evaluating-them.md"}},{"criterion":"BPR-1454:AC7","outcome":"pass","revision":"ae60c0250","evidence":{"kind":"observation","detail":"Rows head-symbolic and head-stale: HEAD -> refs/heads/alias -> refs/heads/main resolves to main's commit (base: empty); a permission-denied loose refs/heads/main beside an older packed-refs entry is reported as a workspace failure instead of the stale commit (base captured the stale commit). Capture and outgoing share application::read_git_head/resolve_reference.","path":"docs/issues/60-done/BPR-1454-fix-the-git-metadata-readers-before-evaluating-them.md"}},{"criterion":"BPR-1454:AC8","outcome":"pass","revision":"ae60c0250","evidence":{"kind":"observation","detail":"Every fix has a review_cycle_git_readers row built with /usr/bin/git and each row run alone fails on the readers restored from 9a4f61976 and passes on ae60c0250. Dev build complete; ctest review_cycle|workspace_source|architecture|cycle_cli|git_readers 60/60 then 59/59 after review fixes (incl. architecture_boundaries, architecture_boundary_contract, core_review_cycle_manifest); core-only preset built and its code rows passed (the three documentation rows need .agents scripts absent from worktrees; the docs gate passes with PYTHONPATH); clang-format 22 and changed-line clang-tidy clean; independent rfc-reviewer found no blocking defect and four findings were fixed with tests. macOS only; Windows and Linux runs are BPR-1462/BPR-1463.","path":"docs/issues/60-done/BPR-1454-fix-the-git-metadata-readers-before-evaluating-them.md"}}]
---

# BPR-1454: Fix the Git metadata readers: repository format, config syntax, ignore case, POSIX classes, index checks and HEAD

Immediate fixes 1–7 of [RFC-BPR-0083](../../rfcs/RFC-BPR-0083-git-metadata-readers.md), filed against
[RFC-BPR-0054](../../rfcs/RFC-BPR-0054-workspace-source-capture-and-revision-material.md)
and not gated by the evaluation. They restore RFC-BPR-0054's "truthful handling
rather than a lossy fallback" for the divergences observed on 2026-10-13.

**Reftable:** the owner answered Q2 with option (a) on 2026-10-13, so fix 1
refuses a checkout with `extensions.refStorage = reftable` with its own named
reason. Whether to read reftable is C3's evaluation; [[BPR-1461]] records AC07's
refusal evidence.

Platforms: the change is portable and its tests run in every preset. This card is
verified on macOS; [[BPR-1462]] and [[BPR-1463]] record the Windows and Linux runs.

## Acceptance criteria

- **BPR-1454:AC1:** Capture and reference resolution read `core.repositoryformatversion` and every `extensions.*` key with Git's rules and refuse as `unsupported`, with a named reason, a version above 1, an unknown extension in a version-1 repository, a version-1-only extension in a version-0 repository, an unrecognised `objectFormat` or `refStorage` value, and `refStorage = reftable` (owner's Q2 answer (a), 2026-10-13). The allowlist is confirmed against Git's documentation and setup code.
- **BPR-1454:AC2:** The keys the readers use are parsed with Git's config syntax: sections, subsections, case rules, quotes, escapes, inline `#` and `;` comments and continuation lines; a config Git refuses to read is refused. Includes stay unread.
- **BPR-1454:AC3:** `core.ignoreCase` from the repository config (and `config.worktree` under `extensions.worktreeConfig`) folds case in ignore and `filter` attribute matching the way Git's wildmatch does.
- **BPR-1454:AC4:** The twelve POSIX bracket classes match as in Git, and an unknown class or unterminated bracket makes the pattern match nothing.
- **BPR-1454:AC5:** The index trailer is verified with the repository's hash algorithm; an all-zero trailer (`index.skipHash`) is accepted.
- **BPR-1454:AC6:** An index extension whose signature does not start with `A`–`Z`, other than `link` and `sdir`, is refused.
- **BPR-1454:AC7:** Capture reports a loose-reference failure other than `missing` instead of consulting `packed-refs`, and follows one level of symbolic reference, sharing one resolver with outgoing resolution.
- **BPR-1454:AC8:** Each fix is accepted by a workspace contract test that builds its repository with real Git and fails on the readers at RFC base `9a4f61976`; core unit tests, the `core-only` preset, the architecture checks, format and changed-line tidy pass.

## Continuation

Done on `ae60c0250` (macOS). Evidence, the per-row negative control and the
independent review are in the [verification ledger](../../verification/RFC-BPR-0083.md).
Review findings not fixed here (a rule file past 1 MiB or 100,000 rules being
dropped, and the safe-direction refusals) go to [[BPR-1455]] for H1
classification. Windows and Linux runs are [[BPR-1462]] and [[BPR-1463]].

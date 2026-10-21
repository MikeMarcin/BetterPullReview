---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-L3"
title: "Say where a comment from a commit-filtered diff goes before Publish, and mark threads under a filter"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-PENDING-L1","BPR-PENDING-L2","BPR-1686","BPR-1696"]
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"milestone":"RFC-BPR-0089 M4","frontend_design":"frontend-design:frontend-design loaded before the editor's target notice and the filtered thread markers were drawn","related_cards":["BPR-PENDING-L1","BPR-PENDING-L2"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_public_path|qt_review_draft_editor.*)$'","detail":"Fixture, shipped composition, offscreen: commit-filter-comment-v1 (src/notes.txt shaped as BPR-Test #116). With C3 hidden the file's diff is M to C1; the recorded thread written on C1 at line 25 (outdated on GitHub) is marked on C1's diff and current there, the head's thread at line 17 is not. Line 15: the editor says 'Goes on the head at line 17, where this line is now.' and the comment is published with commit_id = head at 17. Line 10: refused before Publish ('GitHub shows no change near it'), Publish disabled. Line 5: the editor warns 'Goes on commit 4613350, where this line was written. A later commit changed it, so GitHub shows this comment as outdated…' and it is published with commit_id = C1 at 5, each comment written once (StoredComments 2), and after the reread it is marked on C1's diff. Only C3 shown (left side C2): a base line C1 changed is refused and says why; one outside GitHub's diff is refused; LEFT 13 is published at the head. Whole PR marks the head's thread and not C1's. Three native posts, no file comment. A split file's per-commit diff is a filtered key and takes the same placement."}},{"criterion":"AC2","outcome":"pass","revision":"c44429b78d91a42b463ca924a743c56141e28a8a","evidence":{"kind":"command","command":"python3 tests/end_to_end/rfc0089_commit_filter_live_course.py --build build/dev --work DIR --prefix rfc0089-m4-comment2 --comment --cleanup","detail":"Live, MikeMarcin/BPR-Test #118 (closed, branches deleted, every thread resolved), signed in through gh as MikeMarcin. The tests-enabled composition decided each placement and the notice before Publish; the script checked each against git and published exactly the placed ones through gh (a tests-enabled build holds no publication grant). Hide C3: RIGHT 15 -> head line 17, GitHub answered commit_id = head, line 17, thread current; RIGHT 5 -> C1 with the warning, GitHub answered commit_id = C1, line null, original C1/5, thread outdated; RIGHT 10 refused (outside GitHub's diff at the head). Only C3: LEFT 15 refused (changed after the merge base), LEFT 10 refused (outside GitHub's diff), LEFT 13 -> head LEFT 13, current. After Refresh C1's diff marked line 5 only; Whole PR marked right 17 and left 13. Viewed unchanged for 326 files (AC05). X-RateLimit-Remaining 4823 before, 4671 after. The earlier run #117 (closed, resolved) is the one whose two 422s for moved lines outside the head's hunks drove the outside_github_diff rule. Run on this change's tree before the BPR-1688 restore wiring and the rebases onto main ab1698599, neither of which touches placement; the composed course passes again on the candidate."}}]
---

# BPR-PENDING-L3: Say where a comment from a commit-filtered diff goes before Publish, and mark threads under a filter

RFC-BPR-0089 M4, "Comments and line anchoring" and RFC AC07, in the editor and
the window. The `frontend-design:frontend-design` skill was loaded before the
notice and the markers were drawn; both reuse the editor's and the diff's
existing styles.

## Change

- **Qt.** The inline editor shows one line above Publish
  (`reviewFilteredTarget`): a note when the comment moves to the head's line,
  the warning when it goes on an earlier commit, or the refusal, which
  disables Publish and is its tooltip. While the lines are read it says so and
  blocks Publish.
- **Composition.** The window asks the epoch host for a filtered diff's line
  facts when it is shown, gives the core's placement to the editor and the
  mutation host, refuses a placed line in a pending review ("published on its
  own"), and places review threads with `filtered_thread_line`; every other
  line thread stays in the drawer. After a filter gesture the mutation host is
  composed for the new workbench generation.
- **Fixture.** `commit-filter-comment-v1` and its threads; the fixture's
  GitHub keeps an earlier commit's comment outdated, as measured.
- **Live course.** `--comment` and `--course filtered-comment`.

## Acceptance

- **AC1.** RFC AC07 on the fixture through the shipped composition.
- **AC2.** RFC AC07 live on BPR-Test, publication through gh as decided.

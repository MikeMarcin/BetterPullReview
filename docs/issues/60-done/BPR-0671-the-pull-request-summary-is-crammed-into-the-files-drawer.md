---
schema: "implement-rfc.card.v1"
id: "BPR-0671"
title: "The pull request summary is crammed into the files drawer"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
acceptance: [{"criterion":"The files drawer's top box is gone; the identity is in the strip and the detail is on the overview","outcome":"pass","revision":"5ea94607","evidence":{"kind":"observation","detail":"The scroll area and its insertion into the files drawer's column are removed. qt_composed_github_scale_{100,125,200} pin that the pull request stack is not in the files drawer and not shown while a review is open, and that the files tree is what the drawer holds. Putting the stack back into the drawer's column fails that assertion. The strip already names the repository, the number and the title, and the overview presents the title, the author and branches and the description."}},{"criterion":"Nothing that was in the box is lost","outcome":"pass","revision":"5ea94607","evidence":{"kind":"observation","detail":"Everything the box showed except the commit-oid disclosure was already on the overview; that disclosure is now adopted by the overview under the branches it identifies, pinned by qt_composed_github as on the overview, focusable, collapsed, and opening and closing in place without moving focus. The stack's empty page remains Home's, where a failed load says why. Fifty-eight Qt, composition, workbench-composition, round-trip and public-path selectors pass, along with check_boundaries, strict tidy, clang-format --Werror and verify_documentation.py."}}]
---


# The pull request summary is crammed into the files drawer

The owner, 2026-09-07: "It looks like the entire set of information for this PR
is being crammed into a tiny box in files browser."

It is. `workbenchDrawerTop` is a 420-pixel-high scroll area inserted into the
files drawer's column, holding the repository, number, title, author, branches
and merged state, above the file tree.
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
put it there on purpose and said so: it stays "until the repositories drawer
(BPR-0651) and the overview (BPR-0647) take them". Both of those are Done and
neither took it, so the placeholder shipped.

The strip already names the repository, number and title, so the box also
repeats what is directly above it.

## Scope

- The summary moves to the overview, which is the page RFC-BPR-0028 gives it,
  and the strip keeps the one-line identity it already draws.
- The files drawer holds files.
- Owned paths: `src/adapters/qt` workbench window drawer composition and the
  overview page, their tests and the digests they pin.

## Acceptance

- The files drawer's top box is gone; the identity is in the strip and the
  detail is on the overview.
- Nothing that was in the box is lost.

## Notes (2026-09-07)

- **What was there.** `workbenchDrawerTopScroll`, a 420-pixel-high scroll
  area inserted at the top of the files drawer's column, holding the
  pull request stack (its empty state or the summary panel) and the
  description, bounded to 200 and 120 pixels. Above the files it was
  covering, and below a strip that already names the repository, the
  number and the title.
- **What replaced it.** Nothing: the overview already presents the title,
  the author and branches, and the description. The scroll area and its
  place in the drawer are gone. The stack survives as Home's, where its
  empty page is still where a failed load says why, and it is not shown
  while a review is open.
- **What was only in the box.** The commit oids behind the branches, in a
  disclosure. The overview adopts that button and its details and puts
  them under the branches they identify
  (`pull_request_overview_page::adopt_technical_details`). Nothing else in
  the box was absent from the overview.
- **Evidence.** `qt_composed_github_scale_{100,125,200}` pin that the
  summary is not in the files drawer while a review is open, that the
  files tree is, that the commit-oid disclosure is on the overview and
  opens and closes in place without moving focus, and that the connection
  disclosure keeps its own behaviour where the session chrome now lives.
  Fifty-eight Qt, composition, workbench-composition, round-trip and
  public-path selectors pass. `check_boundaries`, strict tidy on the three
  touched translation units, `clang-format --Werror` and
  `verify_documentation.py` pass.
- **Negative mutation.** Putting the stack back into the files drawer's
  column while a review is open fails "the pull request summary is not in
  the files drawer while a review is open"; dropping the adoption of the
  commit oids does not compile, because the overview is the only place
  left that holds them.

---
schema: "implement-rfc.card.v1"
id: "BPR-1860"
title: "A reopened agent answer keeps its code colours"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner, 2026-11-04: \"Reopening a pr and going back to an agent comment it doesn't have its syntax highlights\"","related_cards":["BPR-1562","BPR-1753"],"related_rfcs":["RFC-BPR-0036"],"id_allocation":"Reserved on main (docs/issues/_reservations/BPR-1860--BPR-1860--0c51dc7a9c31.json).","surface":"The Agent tab's conversation: an answer's fenced code, shown again from the pull request's kept thread (agent_tab_view::present_thread).","owner_review":"Not needed: the colours are compared run for run against the first showing through the packaged classifier."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c7d038b07","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R qt_markdown_code_ --output-on-failure","detail":"check_reopened_agent_answer, all three scales: a default workbench_window wired to a real markdown_code_syntax_host over the packaged definitions and classifier, its publications handed to window.apply_markdown_code_syntax as github_application does. A kept thread with a ```cpp answer is classified and coloured on first showing; shown again, classifications() is unchanged, cache_hits() grows and no effect is queued, and the answer's coloured runs (text and colour from each block's layout formats) equal the first showing's. With the fix reverted the last check fails: \"the thread shown again is coloured exactly as it was the first time\"."}},{"criterion":"AC2","outcome":"pass","revision":"c7d038b07","evidence":{"kind":"command","command":"ctest --test-dir build/dev -R qt_workbench_window_scale --output-on-failure","detail":"test_the_agent_is_a_drawer_tab: the same answer coloured live (spans applied after the thread is shown) and shown again with a dispatch that answers inside the ask; the runs are equal and the keyword and the function differ in colour. Before the fix it fails: \"a kept thread shown again from the code host's cache is coloured exactly as it was live\". 3 of 3 scales pass after."}},{"criterion":"AC3","outcome":"pass","revision":"c7d038b07","evidence":{"kind":"command","command":"_coding-standard/scripts/format.py <3 changed files>; python3 scripts/verify_documentation.py --root .","detail":"clang-format 22.1.8 clean. The documentation gate reports only NEXT_ID BPR-1861 rewinding the shared allocator high water BPR-1864, which main reserved after this branch's base; the card itself validates."}}]
---

# BPR-1860: A reopened agent answer keeps its code colours

## Goal

An Agent tab answer's fenced code is coloured as the review panes colour
code ([[BPR-1562]], [[BPR-1753]]). After the pull request was closed and
opened again, the kept conversation came back with the same answers, but
their code was plain. Shown again, an answer must be coloured exactly as it
was the first time.

## Surface

The Agent tab's conversation thread (`agent_tab_view`). Opening a pull
request shows its kept thread through `present_thread`; that is the
"agent comment" the owner went back to. Posted comments, drafts and notes
are other readers and were not affected.

## Cause

`present_thread` built each turn's widgets, set its answer, and only then
put the turn in `Turns`. Setting the answer renders it, and the reader asks
the readers' code host for its blocks' spans during that render.

The host (`markdown_code_syntax_host`) answers a text it has classified
before from its cache synchronously, inside the request, and the
composition hands that publication straight to the window, which hands it
to `agent_tab_view::apply_code_syntax`. That method offers spans only to the
turns in `Turns`, and the turn being built was not there yet, so the answer
was dropped. The reader does not ask again, so the code stayed plain.

The first showing worked because nothing was cached: the answer came later
on the lanes, after the turn was kept. Reopening the pull request shows the
same texts, which are by then in the cache. The language tag, definitions
and classifier were never at fault.

## Design

`present_thread` keeps the turn first and then sets its answer, footer and
footer layout on the kept entry, as the live path (`ask`) already did. No
host or core change: the cache answering inside the ask is the documented
contract ("a cached text answers at once").

The other readers already ask again after they are reachable: the open
thread card calls `request_code_syntax` once it is the open card, and the
tour editor's readers are fixed members.

## Acceptance

- **AC1**: Composed with the packaged classifier and the readers' host wired
  to the window as the application wires it, a kept thread's C++ answer is
  coloured on first showing; shown again it is answered from the cache, in
  the ask, and its coloured runs equal the first showing's. Fails before the
  fix.
- **AC2**: In the workbench course, an answer coloured live and the same
  answer shown again with an answer inside the ask have equal coloured runs.
  Fails before the fix.
- **AC3**: Format and documentation gates pass.
